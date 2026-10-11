// SPDX-License-Identifier: GPL-2.0-only
/*
 * In-memory attribute index of the MoobFS memory layer (phase 2d).
 *
 * Built at mount time by walking the lower tree and reading every
 * user.moob.* attribute; kept current by the xattr, unlink, rmdir and rename
 * paths of main.c; never persisted. Answers MOOB_IOC_QUERY and
 * MOOB_IOC_GET_INFO issued on the mount root.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <linux/xattr.h>
#include <linux/uaccess.h>
#include <linux/dcache.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/uio.h>
#include <linux/limits.h>
#include "moobl.h"
#include "moobfs_attr.h"
#include "moobfs_ioctl.h"

struct moobl_ix_name {
	struct rb_node node;
	struct rb_root entries;		/* struct moobl_ix_entry, ordered by (type, value, ino) */
	u32 count;
	char name[MOOB_ATTR_NAME_MAX + 1];
};

struct moobl_ix_inode {
	u64 ino;
	struct list_head entries;	/* this inode's typed attributes */
	struct inode *upper;		/* cached layer inode, or NULL */
	char *path;			/* mount-relative path recorded at scan or write time */
};

struct moobl_ix_entry {
	struct rb_node node;
	struct list_head by_ino;
	struct moobl_ix_name *name;
	struct moobl_ix_inode *owner;
	u8 type;
	u16 len;			/* payload length, type byte excluded */
	u8 value[];
};

static inline struct moobl_index *IX(struct super_block *sb)
{
	return &MOOBL_SB(sb)->index;
}

/* ---- ordering ---------------------------------------------------------- */

static u64 double_key(const u8 *payload)
{
	u64 bits = moob_attr_get64(payload);

	/* Maps IEEE 754 binary64 to an unsigned key with the same ordering. */
	return (bits & (1ULL << 63)) ? ~bits : (bits | (1ULL << 63));
}

/* Compares a stored entry with (type, payload, len); ino breaks ties on request. */
static int value_cmp(u8 atype, const u8 *a, u16 alen, u8 btype, const u8 *b, u16 blen)
{
	if (atype != btype)
		return atype < btype ? -1 : 1;
	switch (atype) {
	case MOOB_T_INT64:
	case MOOB_T_TIME: {
		s64 x = (s64)moob_attr_get64(a), y = (s64)moob_attr_get64(b);

		return x < y ? -1 : x > y ? 1 : 0;
	}
	case MOOB_T_DOUBLE: {
		u64 x = double_key(a), y = double_key(b);

		return x < y ? -1 : x > y ? 1 : 0;
	}
	case MOOB_T_BOOL:
		return a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0;
	default: {
		int c = memcmp(a, b, min(alen, blen));

		if (c)
			return c;
		return alen < blen ? -1 : alen > blen ? 1 : 0;
	}
	}
}

static int entry_cmp(const struct moobl_ix_entry *e, u8 type, const u8 *v, u16 len, u64 ino)
{
	int c = value_cmp(e->type, e->value, e->len, type, v, len);

	if (c)
		return c;
	return e->owner->ino < ino ? -1 : e->owner->ino > ino ? 1 : 0;
}

/* ---- accounting and lookup --------------------------------------------- */

static int charge(struct moobl_index *ix, size_t bytes)
{
	if (ix->used + bytes > ix->budget)
		return -ENOMEM;
	ix->used += bytes;
	return 0;
}

static void uncharge(struct moobl_index *ix, size_t bytes)
{
	ix->used -= min_t(u64, ix->used, bytes);
}

static struct moobl_ix_name *name_find(struct moobl_index *ix, const char *name)
{
	struct rb_node *n = ix->names.rb_node;

	while (n) {
		struct moobl_ix_name *nm = rb_entry(n, struct moobl_ix_name, node);
		int c = strcmp(name, nm->name);

		if (!c)
			return nm;
		n = c < 0 ? n->rb_left : n->rb_right;
	}
	return NULL;
}

static struct moobl_ix_name *name_get(struct moobl_index *ix, const char *name)
{
	struct rb_node **p = &ix->names.rb_node, *parent = NULL;
	struct moobl_ix_name *nm;

	while (*p) {
		int c;

		parent = *p;
		nm = rb_entry(parent, struct moobl_ix_name, node);
		c = strcmp(name, nm->name);
		if (!c)
			return nm;
		p = c < 0 ? &parent->rb_left : &parent->rb_right;
	}
	if (charge(ix, sizeof(*nm)))
		return ERR_PTR(-ENOMEM);
	nm = kzalloc(sizeof(*nm), GFP_KERNEL);
	if (!nm) {
		uncharge(ix, sizeof(*nm));
		return ERR_PTR(-ENOMEM);
	}
	strscpy(nm->name, name, sizeof(nm->name));
	nm->entries = RB_ROOT;
	rb_link_node(&nm->node, parent, p);
	rb_insert_color(&nm->node, &ix->names);
	return nm;
}

static void name_put_if_empty(struct moobl_index *ix, struct moobl_ix_name *nm)
{
	if (nm->count)
		return;
	rb_erase(&nm->node, &ix->names);
	kfree(nm);
	uncharge(ix, sizeof(*nm));
}

static struct moobl_ix_inode *inode_rec_get(struct moobl_index *ix, u64 ino)
{
	struct moobl_ix_inode *rec = xa_load(&ix->inodes, ino);

	if (rec)
		return rec;
	if (charge(ix, sizeof(*rec)))
		return ERR_PTR(-ENOMEM);
	rec = kzalloc(sizeof(*rec), GFP_KERNEL);
	if (!rec) {
		uncharge(ix, sizeof(*rec));
		return ERR_PTR(-ENOMEM);
	}
	rec->ino = ino;
	INIT_LIST_HEAD(&rec->entries);
	if (xa_err(xa_store(&ix->inodes, ino, rec, GFP_KERNEL))) {
		kfree(rec);
		uncharge(ix, sizeof(*rec));
		return ERR_PTR(-ENOMEM);
	}
	ix->inode_count++;
	return rec;
}

static void inode_rec_put_if_empty(struct moobl_index *ix, struct moobl_ix_inode *rec)
{
	if (!list_empty(&rec->entries))
		return;
	xa_erase(&ix->inodes, rec->ino);
	if (rec->path)
		uncharge(ix, strlen(rec->path) + 1);
	kfree(rec->path);
	kfree(rec);
	uncharge(ix, sizeof(*rec));
	ix->inode_count--;
}

static void entry_free(struct moobl_index *ix, struct moobl_ix_entry *e)
{
	rb_erase(&e->node, &e->name->entries);
	list_del(&e->by_ino);
	e->name->count--;
	ix->entries--;
	uncharge(ix, sizeof(*e) + e->len);
	name_put_if_empty(ix, e->name);
	kfree(e);
}

static struct moobl_ix_entry *entry_find(struct moobl_ix_inode *rec, const char *name)
{
	struct moobl_ix_entry *e;

	list_for_each_entry(e, &rec->entries, by_ino)
		if (!strcmp(e->name->name, name))
			return e;
	return NULL;
}

/* Replaces or inserts one typed attribute. Caller holds the write lock. */
static int index_put(struct moobl_index *ix, u64 ino, const char *name, const u8 *value,
		     size_t size)
{
	struct moobl_ix_inode *rec;
	struct moobl_ix_name *nm;
	struct moobl_ix_entry *e, *old;
	struct rb_node **p, *parent = NULL;
	u16 len = (u16)(size - 1);

	if (value[0] == MOOB_T_RAW)
		return 0;	/* raw payloads are stored but never indexed */
	rec = inode_rec_get(ix, ino);
	if (IS_ERR(rec))
		return PTR_ERR(rec);
	old = entry_find(rec, name);
	if (old)
		entry_free(ix, old);	/* before name_get: freeing may drop the name node */
	nm = name_get(ix, name);
	if (IS_ERR(nm)) {
		inode_rec_put_if_empty(ix, rec);
		return PTR_ERR(nm);
	}
	if (charge(ix, sizeof(*e) + len)) {
		name_put_if_empty(ix, nm);
		inode_rec_put_if_empty(ix, rec);
		return -ENOMEM;
	}
	e = kmalloc(sizeof(*e) + len, GFP_KERNEL);
	if (!e) {
		uncharge(ix, sizeof(*e) + len);
		name_put_if_empty(ix, nm);
		inode_rec_put_if_empty(ix, rec);
		return -ENOMEM;
	}
	e->name = nm;
	e->owner = rec;
	e->type = value[0];
	e->len = len;
	memcpy(e->value, value + 1, len);
	p = &nm->entries.rb_node;
	while (*p) {
		struct moobl_ix_entry *cur;

		parent = *p;
		cur = rb_entry(parent, struct moobl_ix_entry, node);
		p = entry_cmp(cur, e->type, e->value, e->len, ino) < 0 ? &parent->rb_right
								       : &parent->rb_left;
	}
	rb_link_node(&e->node, parent, p);
	rb_insert_color(&e->node, &nm->entries);
	list_add(&e->by_ino, &rec->entries);
	nm->count++;
	ix->entries++;
	return 0;
}

static void index_set_path(struct moobl_index *ix, struct moobl_ix_inode *rec, const char *path)
{
	size_t len = strlen(path) + 1;
	char *copy;

	if (rec->path && !strcmp(rec->path, path))
		return;
	if (charge(ix, len))
		return;	/* keep the old name; QUERY will fall back to the dentry cache */
	copy = kmemdup(path, len, GFP_KERNEL);
	if (!copy) {
		uncharge(ix, len);
		return;
	}
	if (rec->path)
		uncharge(ix, strlen(rec->path) + 1);
	kfree(rec->path);
	rec->path = copy;
}

/* ---- mount-time scan --------------------------------------------------- */

struct scan_name {
	struct list_head list;
	int len;
	char name[];
};

struct scan_ctx {
	struct dir_context ctx;
	struct list_head names;
	int err;
};

static bool scan_actor(struct dir_context *ctx, const char *name, int len, loff_t pos,
		       u64 ino, unsigned int d_type)
{
	struct scan_ctx *sc = container_of(ctx, struct scan_ctx, ctx);
	struct scan_name *sn;

	if ((len == 1 && name[0] == '.') || (len == 2 && name[0] == '.' && name[1] == '.'))
		return true;
	sn = kmalloc(sizeof(*sn) + len, GFP_KERNEL);
	if (!sn) {
		sc->err = -ENOMEM;
		return false;
	}
	sn->len = len;
	memcpy(sn->name, name, len);
	list_add_tail(&sn->list, &sc->names);
	return true;
}

struct scan_dir {
	struct list_head list;
	struct dentry *dentry;		/* lower directory, referenced */
	char *path;			/* mount-relative, "" for the root */
};

/* Reads the typed attributes of one lower dentry into the index. */
static int scan_attrs(struct super_block *sb, struct dentry *lower, u64 ino, const char *path,
		      char *list, size_t list_cap, u8 *value)
{
	struct moobl_index *ix = IX(sb);
	ssize_t len;
	char *p;
	int err = 0;

	len = vfs_listxattr(lower, list, list_cap);
	if (len == -EOPNOTSUPP || len == -ENODATA || len == 0)
		return 0;
	if (len == -ERANGE)
		return 0;	/* more than list_cap bytes of names: left out of the index, reported by moobfs-check */
	if (len < 0)
		return (int)len;
	for (p = list; p < list + len && !err; p += strlen(p) + 1) {
		const char *name = p + MOOB_ATTR_PREFIX_LEN;
		ssize_t n;

		if (strncmp(p, MOOB_ATTR_PREFIX, MOOB_ATTR_PREFIX_LEN))
			continue;
		if (moob_attr_name_reserved(name, strlen(name)) || !moob_attr_name_ok(name, strlen(name)))
			continue;
		n = vfs_getxattr(&nop_mnt_idmap, lower, p, value, MOOB_ATTR_VALUE_MAX + 1);
		if (n <= 0 || moob_attr_validate(value, n) != MOOB_ATTR_OK)
			continue;	/* invalid encodings are not indexed; moobfs-check reports them */
		err = index_put(ix, ino, name, value, n);
		if (!err) {
			struct moobl_ix_inode *rec = xa_load(&ix->inodes, ino);

			if (rec)
				index_set_path(ix, rec, path);
		}
	}
	return err;
}

int moobl_index_build(struct super_block *sb)
{
	struct moobl_sb *sbi = MOOBL_SB(sb);
	struct moobl_index *ix = &sbi->index;
	const struct cred *old;
	LIST_HEAD(stack);
	struct scan_dir *sd;
	char *list;
	u8 *value;
	int err = 0;

	if (!ix->enabled)
		return 0;
	list = kvmalloc(XATTR_LIST_MAX, GFP_KERNEL);
	value = kmalloc(MOOB_ATTR_VALUE_MAX + 1, GFP_KERNEL);
	sd = kzalloc(sizeof(*sd), GFP_KERNEL);
	if (!list || !value || !sd) {
		err = -ENOMEM;
		goto out;
	}
	sd->dentry = dget(sbi->lower.dentry);
	sd->path = kstrdup("", GFP_KERNEL);
	if (!sd->path) {
		dput(sd->dentry);
		err = -ENOMEM;
		goto out;
	}
	list_add(&sd->list, &stack);
	sd = NULL;
	old = override_creds(sbi->creds);
	down_write(&ix->lock);
	{
		ssize_t probe = vfs_listxattr(sbi->lower.dentry, NULL, 0);

		ix->xattr_ok = probe >= 0 || probe == -ENODATA;
	}
	while (!list_empty(&stack) && !err) {
		struct scan_dir *cur = list_first_entry(&stack, struct scan_dir, list);
		struct path dpath = { .mnt = sbi->lower.mnt, .dentry = cur->dentry };
		struct scan_ctx sc = { .ctx.actor = scan_actor, .names = LIST_HEAD_INIT(sc.names) };
		struct scan_name *sn, *tmp;
		struct file *dir;

		list_del(&cur->list);
		err = scan_attrs(sb, cur->dentry, d_inode(cur->dentry)->i_ino, cur->path, list,
				 XATTR_LIST_MAX, value);
		if (err)
			goto drop;
		dir = dentry_open(&dpath, O_RDONLY | O_DIRECTORY, sbi->creds);
		if (IS_ERR(dir)) {
			err = PTR_ERR(dir);
			goto drop;
		}
		err = iterate_dir(dir, &sc.ctx);
		fput(dir);
		if (!err)
			err = sc.err;
		list_for_each_entry_safe(sn, tmp, &sc.names, list) {
			struct dentry *child;

			if (!err) {
				child = lookup_one_len_unlocked(sn->name, cur->dentry, sn->len);
				if (IS_ERR(child)) {
					err = PTR_ERR(child);
				} else if (d_really_is_positive(child) &&
					   (d_is_dir(child) || d_is_reg(child))) {
					char *cpath = kasprintf(GFP_KERNEL, "%s%s%.*s", cur->path,
								cur->path[0] ? "/" : "", sn->len, sn->name);

					if (!cpath) {
						err = -ENOMEM;
					} else if (d_is_dir(child)) {
						struct scan_dir *nd = kzalloc(sizeof(*nd), GFP_KERNEL);

						if (!nd) {
							err = -ENOMEM;
							kfree(cpath);
						} else {
							nd->dentry = dget(child);
							nd->path = cpath;
							list_add(&nd->list, &stack);
						}
					} else {
						err = scan_attrs(sb, child, d_inode(child)->i_ino, cpath, list,
								 XATTR_LIST_MAX, value);
						kfree(cpath);
					}
					if (!IS_ERR(child))
						dput(child);
				} else {
					dput(child);
				}
			}
			list_del(&sn->list);
			kfree(sn);
		}
drop:
		dput(cur->dentry);
		kfree(cur->path);
		kfree(cur);
	}
	while (!list_empty(&stack)) {
		struct scan_dir *left = list_first_entry(&stack, struct scan_dir, list);

		list_del(&left->list);
		dput(left->dentry);
		kfree(left->path);
		kfree(left);
	}
	up_write(&ix->lock);
	revert_creds(old);
	if (err)
		pr_warn("moobfs_layer: index build failed (%d); mount with noindex to skip it\n", err);
out:
	kfree(sd);
	kfree(value);
	kvfree(list);
	return err;
}

void moobl_index_init(struct super_block *sb, u64 budget, bool enabled)
{
	struct moobl_index *ix = IX(sb);

	init_rwsem(&ix->lock);
	ix->names = RB_ROOT;
	xa_init(&ix->inodes);
	ix->budget = budget;
	ix->enabled = enabled;
	ix->initialized = true;
}

void moobl_index_destroy(struct super_block *sb)
{
	struct moobl_index *ix = IX(sb);
	struct moobl_ix_inode *rec;
	unsigned long ino;

	if (!ix->initialized)
		return;	/* fill_super failed before the index existed */
	down_write(&ix->lock);
	xa_for_each(&ix->inodes, ino, rec) {
		struct moobl_ix_entry *e, *tmp;

		list_for_each_entry_safe(e, tmp, &rec->entries, by_ino)
			entry_free(ix, e);
		inode_rec_put_if_empty(ix, rec);
	}
	xa_destroy(&ix->inodes);
	up_write(&ix->lock);
}

/* ---- updates from main.c ------------------------------------------------ */

static void record_dentry_path(struct moobl_index *ix, struct moobl_ix_inode *rec,
			       struct dentry *dentry)
{
	char *buf = (char *)__get_free_page(GFP_KERNEL);
	char *p;

	if (!buf)
		return;
	p = dentry_path_raw(dentry, buf, PAGE_SIZE);
	if (!IS_ERR(p))
		index_set_path(ix, rec, *p == '/' ? p + 1 : p);
	free_page((unsigned long)buf);
}

int moobl_index_set(struct super_block *sb, struct dentry *dentry, const char *name,
		    const void *value, size_t size)
{
	struct moobl_index *ix = IX(sb);
	struct inode *inode = d_inode(dentry);
	struct moobl_ix_inode *rec;
	int err;

	if (!ix->enabled)
		return 0;
	down_write(&ix->lock);
	err = index_put(ix, inode->i_ino, name, value, size);
	if (err) {
		/* The lower write already succeeded; the index is now incomplete until remount. */
		if (!ix->incomplete)
			pr_warn("moobfs_layer: index update failed (%d); QUERY results are incomplete until remount\n", err);
		ix->incomplete = true;
	} else {
		rec = xa_load(&ix->inodes, inode->i_ino);
		if (rec) {
			rec->upper = inode;
			record_dentry_path(ix, rec, dentry);
		}
	}
	up_write(&ix->lock);
	return err;
}

void moobl_index_remove(struct super_block *sb, u64 ino, const char *name)
{
	struct moobl_index *ix = IX(sb);
	struct moobl_ix_inode *rec;
	struct moobl_ix_entry *e;

	if (!ix->enabled)
		return;
	down_write(&ix->lock);
	rec = xa_load(&ix->inodes, ino);
	if (rec) {
		e = entry_find(rec, name);
		if (e)
			entry_free(ix, e);
		inode_rec_put_if_empty(ix, rec);
	}
	up_write(&ix->lock);
}

void moobl_index_drop_inode(struct super_block *sb, u64 ino)
{
	struct moobl_index *ix = IX(sb);
	struct moobl_ix_inode *rec;
	struct moobl_ix_entry *e, *tmp;

	if (!ix->enabled)
		return;
	down_write(&ix->lock);
	rec = xa_load(&ix->inodes, ino);
	if (rec) {
		list_for_each_entry_safe(e, tmp, &rec->entries, by_ino)
			entry_free(ix, e);
		inode_rec_put_if_empty(ix, rec);
	}
	up_write(&ix->lock);
}

void moobl_index_renamed(struct super_block *sb, struct dentry *dentry)
{
	struct moobl_index *ix = IX(sb);
	struct moobl_ix_inode *rec;

	if (!ix->enabled)
		return;
	down_write(&ix->lock);
	if (d_is_dir(dentry)) {
		ix->paths_stale = true;	/* descendants keep old names; QUERY uses the dentry cache */
	} else {
		rec = xa_load(&ix->inodes, d_inode(dentry)->i_ino);
		if (rec)
			record_dentry_path(ix, rec, dentry);
	}
	up_write(&ix->lock);
}

void moobl_index_inode_cached(struct super_block *sb, struct inode *inode)
{
	struct moobl_index *ix = IX(sb);
	struct moobl_ix_inode *rec;

	if (!ix->enabled)
		return;
	down_write(&ix->lock);
	rec = xa_load(&ix->inodes, inode->i_ino);
	if (rec)
		rec->upper = inode;
	up_write(&ix->lock);
}

void moobl_index_inode_evicted(struct super_block *sb, struct inode *inode)
{
	struct moobl_index *ix = IX(sb);
	struct moobl_ix_inode *rec;

	if (!ix->enabled)
		return;
	down_write(&ix->lock);
	rec = xa_load(&ix->inodes, inode->i_ino);
	if (rec && rec->upper == inode)
		rec->upper = NULL;
	up_write(&ix->lock);
}

/* ---- QUERY -------------------------------------------------------------- */

static int term_check(const struct moob_query_term *t)
{
	size_t nlen = strnlen(t->name, sizeof(t->name));

	if (nlen == sizeof(t->name) || !moob_attr_name_ok(t->name, nlen) || t->reserved[0] ||
	    t->pad[0] || t->pad[1] || t->pad[2])
		return -EINVAL;
	if (t->op == MOOB_OP_EXISTS)
		return (t->type || t->len) ? -EINVAL : 0;
	if (t->op < MOOB_OP_EQ || t->op > MOOB_OP_PREFIX)
		return -EINVAL;
	switch (t->type) {
	case MOOB_T_STRING:
		if (t->len > MOOB_QUERY_VALUE_MAX || !moob_utf8_valid(t->value, t->len))
			return -EINVAL;
		return 0;
	case MOOB_T_INT64:
	case MOOB_T_TIME:
	case MOOB_T_DOUBLE:
		if (t->len != 8 || t->op == MOOB_OP_PREFIX)
			return -EINVAL;
		if (t->type == MOOB_T_DOUBLE) {
			u8 v[9] = { MOOB_T_DOUBLE };

			memcpy(v + 1, t->value, 8);
			if (moob_attr_validate(v, 9) != MOOB_ATTR_OK)
				return -EINVAL;
		}
		return 0;
	case MOOB_T_BOOL:
		if (t->len != 1 || t->value[0] > 1 || t->op == MOOB_OP_PREFIX)
			return -EINVAL;
		return 0;
	default:
		return -EINVAL;	/* raw values are not indexed and cannot be queried */
	}
}

static bool entry_matches(const struct moobl_ix_entry *e, const struct moob_query_term *t)
{
	int c;

	if (t->op == MOOB_OP_EXISTS)
		return true;
	if (e->type != t->type)
		return false;
	if (t->op == MOOB_OP_PREFIX)
		return e->len >= t->len && !memcmp(e->value, t->value, t->len);
	c = value_cmp(e->type, e->value, e->len, t->type, t->value, t->len);
	switch (t->op) {
	case MOOB_OP_EQ: return c == 0;
	case MOOB_OP_LT: return c < 0;
	case MOOB_OP_LE: return c <= 0;
	case MOOB_OP_GT: return c > 0;
	case MOOB_OP_GE: return c >= 0;
	default: return false;
	}
}

static bool inode_matches_rest(struct moobl_ix_inode *rec, const struct moob_query *q)
{
	u32 i;

	for (i = 1; i < q->nterms; i++) {
		struct moobl_ix_entry *e = entry_find(rec, q->terms[i].name);

		if (!e || !entry_matches(e, &q->terms[i]))
			return false;
	}
	return true;
}

static void fill_result(struct moobl_index *ix, struct moobl_ix_inode *rec,
			struct moob_query_result *r, char *pathbuf)
{
	const char *path = NULL;

	r->ino = rec->ino;
	r->flags = 0;
	if (rec->upper) {
		struct dentry *alias = d_find_alias(rec->upper);

		if (alias) {
			char *p = dentry_path_raw(alias, pathbuf, PAGE_SIZE);

			if (!IS_ERR(p))
				path = *p == '/' ? p + 1 : p;
			dput(alias);
		}
	}
	if (!path && rec->path && !ix->paths_stale)
		path = rec->path;
	if (!path) {
		r->flags |= MOOB_RES_PATH_UNKNOWN;
		r->path_len = 0;
		r->path[0] = '\0';
		return;
	}
	r->path_len = (u32)strlen(path);
	if (r->path_len >= MOOB_QUERY_PATH_MAX) {
		r->flags |= MOOB_RES_PATH_TRUNCATED;
		r->path_len = MOOB_QUERY_PATH_MAX - 1;
	}
	memcpy(r->path, path, r->path_len);
	r->path[r->path_len] = '\0';
}

/*
 * Candidates are gathered under the index lock in bounded chunks, then checked
 * against the caller's own credentials with the lock released: the recorded
 * path is resolved through the VFS (search permission on every directory,
 * read permission on the file), the resolved object must belong to this mount
 * and carry the indexed inode number. Entries the caller may not reach are
 * omitted silently and do not count toward limit or truncated, so neither
 * existence nor count leaks. Operational failures (ENOMEM, EFAULT) are
 * returned, never turned into an empty answer. The scan resumes after each
 * chunk from the last (type, value, ino) key, which stays meaningful even if
 * the tree changed meanwhile, so the number of examined candidates is not
 * bounded by the result limit.
 */
#define MOOBL_QUERY_CHUNK 256U

struct moobl_candidate {
	u64 ino;
	char *path;	/* mount-relative, or NULL when unknown */
};

struct moobl_resume {
	bool valid;
	u8 type;
	u16 len;
	u64 ino;
	u8 value[MOOB_QUERY_VALUE_MAX];
};

static bool caller_may_read(struct file *file, struct super_block *sb, const char *path, u64 ino)
{
	struct path p;
	bool ok = false;

	if (!path || !*path)
		return false;
	if (vfs_path_lookup(file->f_path.dentry, file->f_path.mnt, path, 0, &p))
		return false;
	if (p.mnt == file->f_path.mnt && p.dentry->d_sb == sb && d_inode(p.dentry) &&
	    d_inode(p.dentry)->i_ino == ino &&
	    !inode_permission(&nop_mnt_idmap, d_inode(p.dentry), MAY_READ))
		ok = true;
	path_put(&p);
	return ok;
}

/* First entry strictly after the resume key, or the first entry when no key. */
static struct rb_node *first_after(struct moobl_ix_name *nm, const struct moobl_resume *r)
{
	struct rb_node *n = nm->entries.rb_node, *best = NULL;

	if (!r->valid)
		return rb_first(&nm->entries);
	while (n) {
		struct moobl_ix_entry *e = rb_entry(n, struct moobl_ix_entry, node);

		if (entry_cmp(e, r->type, r->value, r->len, r->ino) > 0) {
			best = n;
			n = n->rb_left;
		} else {
			n = n->rb_right;
		}
	}
	return best;
}

/*
 * Where the scan of the first term starts and where it can stop. Entries are ordered by
 * (type, value, ino), so every op except EXISTS matches a contiguous run: EQ, GE, GT and
 * PREFIX start at the first entry whose (type, value) is not below the term; LT and LE start
 * at the first entry of the term's type. EXISTS keeps the full scan.
 */
static struct rb_node *seek_first(struct moobl_ix_name *nm, const struct moob_query_term *t)
{
	struct rb_node *n = nm->entries.rb_node, *best = NULL;

	if (t->op == MOOB_OP_EXISTS)
		return rb_first(&nm->entries);
	while (n) {
		struct moobl_ix_entry *e = rb_entry(n, struct moobl_ix_entry, node);
		int c;

		if (t->op == MOOB_OP_LT || t->op == MOOB_OP_LE)
			c = e->type < t->type ? -1 : 0;
		else
			c = value_cmp(e->type, e->value, e->len, t->type, t->value, t->len);
		if (c >= 0) {
			best = n;
			n = n->rb_left;
		} else {
			n = n->rb_right;
		}
	}
	return best;
}

/* True when no entry at or after e (in tree order) can match the term. */
static bool past_end(const struct moobl_ix_entry *e, const struct moob_query_term *t)
{
	int c;

	if (t->op == MOOB_OP_EXISTS)
		return false;
	if (e->type != t->type)
		return e->type > t->type;
	c = value_cmp(e->type, e->value, e->len, t->type, t->value, t->len);
	switch (t->op) {
	case MOOB_OP_EQ:
	case MOOB_OP_LE:
		return c > 0;
	case MOOB_OP_LT:
		return c >= 0;
	case MOOB_OP_PREFIX:
		/* strings with a given prefix form one run starting at the prefix itself */
		return c > 0 && !(e->len >= t->len && !memcmp(e->value, t->value, t->len));
	default:
		return false;
	}
}

static long do_query(struct super_block *sb, struct file *file, void __user *arg)
{
	struct moobl_index *ix = IX(sb);
	struct moob_query *q;
	struct moob_query_result __user *uout;
	struct moob_query_result *one = NULL;
	struct moobl_candidate *cand = NULL;
	struct moobl_resume *resume = NULL;
	struct moob_query_result tmp;
	char *pathbuf = NULL;
	u32 i, count = 0;
	bool exhausted = false;
	long err = 0;

	if (!ix->enabled)
		return -ENOSYS;
	q = memdup_user(arg, sizeof(*q));
	if (IS_ERR(q))
		return PTR_ERR(q);
	if (q->version != MOOB_UAPI_VERSION || !q->nterms || q->nterms > MOOB_QUERY_MAX_TERMS ||
	    !q->limit || q->limit > MOOB_QUERY_MAX_LIMIT || q->flags || q->reserved[0] ||
	    q->reserved[1]) {
		err = -EINVAL;
		goto out_q;
	}
	for (i = 0; i < q->nterms; i++) {
		err = term_check(&q->terms[i]);
		if (err)
			goto out_q;
	}
	/* one result at a time is copied to the caller's array, so the kernel never allocates
	 * (or zeroes) a limit-sized buffer: the cost is per result, not per requested limit */
	uout = u64_to_user_ptr(q->results);
	one = kmalloc(sizeof(*one), GFP_KERNEL);
	cand = kcalloc(MOOBL_QUERY_CHUNK, sizeof(*cand), GFP_KERNEL);
	resume = kzalloc(sizeof(*resume), GFP_KERNEL);
	pathbuf = (char *)__get_free_page(GFP_KERNEL);
	if (!one || !cand || !resume || !pathbuf) {
		err = -ENOMEM;
		goto out_buf;
	}
	q->truncated = 0;
	while (!exhausted && !err) {
		struct moobl_ix_name *nm;
		struct rb_node *n;
		u32 ncand = 0;

		down_read(&ix->lock);
		nm = name_find(ix, q->terms[0].name);
		n = !nm ? NULL : resume->valid ? first_after(nm, resume) : seek_first(nm, &q->terms[0]);
		for (; n; n = rb_next(n)) {
			struct moobl_ix_entry *e = rb_entry(n, struct moobl_ix_entry, node);

			if (past_end(e, &q->terms[0])) {
				n = NULL;
				break;
			}
			resume->valid = true;
			resume->type = e->type;
			resume->len = e->len;
			resume->ino = e->owner->ino;
			memcpy(resume->value, e->value, e->len);
			if (!entry_matches(e, &q->terms[0]) || !inode_matches_rest(e->owner, q))
				continue;
			fill_result(ix, e->owner, &tmp, pathbuf);
			cand[ncand].ino = tmp.ino;
			cand[ncand].path = NULL;
			if (!(tmp.flags & MOOB_RES_PATH_UNKNOWN)) {
				cand[ncand].path = kstrdup(tmp.path, GFP_ATOMIC);
				if (!cand[ncand].path) {
					err = -ENOMEM;
					break;
				}
			}
			ncand++;
			if (ncand == MOOBL_QUERY_CHUNK) {
				n = rb_next(n);
				break;
			}
		}
		if (!n)
			exhausted = true;
		up_read(&ix->lock);
		for (i = 0; i < ncand; i++) {
			if (!err && caller_may_read(file, sb, cand[i].path, cand[i].ino)) {
				if (count == q->limit) {
					q->truncated = 1;
					exhausted = true;
				} else {
					memset(one, 0, sizeof(*one));
					one->ino = cand[i].ino;
					one->path_len = (u32)strlen(cand[i].path);
					memcpy(one->path, cand[i].path, one->path_len + 1);
					if (copy_to_user(&uout[count], one, sizeof(*one)))
						err = -EFAULT;
					else
						count++;
				}
			}
			kfree(cand[i].path);
			cand[i].path = NULL;
		}
	}
	if (err)
		goto out_buf;
	q->count = count;
	if (copy_to_user(arg, q, sizeof(*q)))
		err = -EFAULT;
out_buf:
	if (cand)
		for (i = 0; i < MOOBL_QUERY_CHUNK; i++)
			kfree(cand[i].path);
	kfree(cand);
	kfree(resume);
	if (pathbuf)
		free_page((unsigned long)pathbuf);
	kfree(one);
out_q:
	kfree(q);
	return err;
}

static long do_get_info(struct super_block *sb, void __user *arg)
{
	struct moobl_sb *sbi = MOOBL_SB(sb);
	struct moobl_index *ix = &sbi->index;
	struct moob_info info;
	const char *fs = sbi->lower.dentry->d_sb->s_type->name;

	memset(&info, 0, sizeof(info));
	info.uapi_version = MOOB_UAPI_VERSION;
	info.format_version = MOOB_ATTR_FORMAT_VERSION;
	info.attr_name_max = MOOB_ATTR_NAME_MAX;
	info.attr_string_max = MOOB_ATTR_STRING_MAX;
	info.attr_raw_max = MOOB_ATTR_RAW_MAX;
	info.attr_max_per_file = MOOB_ATTR_MAX_PER_FILE;
	strscpy(info.lower_fs, fs, sizeof(info.lower_fs));
	/* Capabilities are advertised from the lower filesystem type, not probed. */
	if (!strcmp(fs, "btrfs"))
		info.caps |= MOOB_CAP_REFLINK | MOOB_CAP_DATA_CHECKSUMS | MOOB_CAP_SNAPSHOTS;
	else if (!strcmp(fs, "xfs") || !strcmp(fs, "ocfs2") || !strcmp(fs, "bcachefs"))
		info.caps |= MOOB_CAP_REFLINK;
	if (sbi->nolinks)
		info.flags |= MOOB_FLAG_NOLINKS;
	down_read(&ix->lock);
	if (ix->xattr_ok)
		info.caps |= MOOB_CAP_XATTR;
	if (!ix->enabled)
		info.flags |= MOOB_FLAG_INDEX_OFF;
	if (ix->paths_stale)
		info.flags |= MOOB_FLAG_PATHS_STALE;
	if (ix->incomplete)
		info.flags |= MOOB_FLAG_INDEX_INCOMPLETE;
	info.index_budget = ix->budget;
	info.index_used = ix->used;
	info.index_entries = ix->entries;
	info.index_inodes = ix->inode_count;
	up_read(&ix->lock);
	return copy_to_user(arg, &info, sizeof(info)) ? -EFAULT : 0;
}

long moobl_root_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct super_block *sb = file_inode(file)->i_sb;

	if (file->f_path.dentry != sb->s_root)
		return -ENOTTY;
	switch (cmd) {
	case MOOB_IOC_QUERY:
		return do_query(sb, file, (void __user *)arg);
	case MOOB_IOC_GET_INFO:
		return do_get_info(sb, (void __user *)arg);
	default:
		return -ENOTTY;
	}
}
