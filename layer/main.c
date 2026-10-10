// SPDX-License-Identifier: GPL-2.0-only
/*
 * MoobFS memory layer, phase 2a: a minimal stacked filesystem that passes
 * every operation through to a directory of a lower filesystem.
 *
 * This phase implements only the pass-through skeleton (lookup, readdir,
 * create, mkdir, unlink, rmdir, read, write, mmap, fsync, truncate, stat).
 * It exists to validate the stacking approach of ADR-0007 on the pinned
 * kernel. Typed attributes, the index and QUERY arrive in later phases.
 *
 * Patterns follow fs/ecryptfs (one upper inode per lower inode, lower path
 * kept in dentry fsdata) and fs/overlayfs (I/O delegated through
 * fs/backing-file.c, mounter credentials for backing I/O).
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/fs_context.h>
#include <linux/fs_parser.h>
#include <linux/fs_stack.h>
#include <linux/backing-file.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/statfs.h>
#include <linux/cred.h>
#include <linux/uio.h>
#include <linux/splice.h>
#include <linux/seq_file.h>
#include <linux/xattr.h>
#include <linux/fiemap.h>

#define MOOBL_SUPER_MAGIC 0x4d4f4f42	/* "MOOB" */
#define MOOBL_NAME "moobfs_layer"

struct moobl_sb {
	struct path lower;		/* lower root */
	const struct cred *creds;	/* mounter credentials for backing I/O */
	bool nolinks;			/* refuse link and symlink creation */
};

struct moobl_inode {
	struct inode vfs_inode;
	struct inode *lower;
};

struct moobl_dentry {
	struct path lower;
	struct rcu_head rcu;
};

static struct kmem_cache *moobl_inode_cache;

static inline struct moobl_sb *MOOBL_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct moobl_inode *MOOBL_I(struct inode *inode)
{
	return container_of(inode, struct moobl_inode, vfs_inode);
}

static inline struct inode *moobl_lower_inode(struct inode *inode)
{
	return MOOBL_I(inode)->lower;
}

static inline struct path *moobl_lower_path(struct dentry *dentry)
{
	return &((struct moobl_dentry *)dentry->d_fsdata)->lower;
}

static inline struct dentry *moobl_lower_dentry(struct dentry *dentry)
{
	return moobl_lower_path(dentry)->dentry;
}

static int moobl_set_lower_path(struct dentry *dentry, struct vfsmount *mnt,
				struct dentry *lower_dentry)
{
	struct moobl_dentry *info = kzalloc(sizeof(*info), GFP_KERNEL);

	if (!info)
		return -ENOMEM;
	info->lower.mnt = mntget(mnt);
	info->lower.dentry = lower_dentry;	/* reference transferred */
	dentry->d_fsdata = info;
	return 0;
}

/* ---- inodes ----------------------------------------------------------- */

static const struct inode_operations moobl_dir_iops;
static const struct inode_operations moobl_file_iops;
static const struct inode_operations moobl_special_iops;
static const struct inode_operations moobl_symlink_iops;
static const struct file_operations moobl_dir_fops;
static const struct file_operations moobl_file_fops;
static const struct address_space_operations moobl_aops = {
	/* Data never lives in this mapping; mmap is delegated to the lower file. */
	.direct_IO = noop_direct_IO,
};

static int moobl_inode_test(struct inode *inode, void *lower)
{
	return moobl_lower_inode(inode) == lower;
}

static int moobl_inode_set(struct inode *inode, void *data)
{
	struct inode *lower = data;

	MOOBL_I(inode)->lower = lower;
	fsstack_copy_attr_all(inode, lower);
	fsstack_copy_inode_size(inode, lower);
	inode->i_ino = lower->i_ino;
	inode->i_mapping->a_ops = &moobl_aops;
	if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &moobl_dir_iops;
		inode->i_fop = &moobl_dir_fops;
	} else if (S_ISREG(inode->i_mode)) {
		inode->i_op = &moobl_file_iops;
		inode->i_fop = &moobl_file_fops;
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &moobl_symlink_iops;
	} else {
		/* Devices and fifos: attributes only; opening them is refused. */
		inode->i_op = &moobl_special_iops;
		init_special_inode(inode, inode->i_mode, lower->i_rdev);
	}
	return 0;
}

/* Returns a referenced upper inode for the lower inode, taking a reference on lower. */
static struct inode *moobl_get_inode(struct super_block *sb, struct inode *lower)
{
	struct inode *inode;

	if (lower->i_sb != MOOBL_SB(sb)->lower.dentry->d_sb)
		return ERR_PTR(-EXDEV);
	ihold(lower);
	inode = iget5_locked(sb, (unsigned long)lower, moobl_inode_test,
			     moobl_inode_set, lower);
	if (!inode) {
		iput(lower);
		return ERR_PTR(-ENOMEM);
	}
	if (!(inode->i_state & I_NEW))
		iput(lower);	/* existing upper inode already holds its reference */
	else
		unlock_new_inode(inode);
	return inode;
}

static struct inode *moobl_alloc_inode(struct super_block *sb)
{
	struct moobl_inode *mi = alloc_inode_sb(sb, moobl_inode_cache, GFP_KERNEL);

	if (!mi)
		return NULL;
	mi->lower = NULL;
	return &mi->vfs_inode;
}

static void moobl_free_inode(struct inode *inode)
{
	kmem_cache_free(moobl_inode_cache, MOOBL_I(inode));
}

static void moobl_evict_inode(struct inode *inode)
{
	truncate_inode_pages_final(&inode->i_data);
	clear_inode(inode);
	if (moobl_lower_inode(inode))
		iput(moobl_lower_inode(inode));
}

static int moobl_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	int err = vfs_statfs(moobl_lower_path(dentry), buf);

	if (!err)
		buf->f_type = MOOBL_SUPER_MAGIC;
	return err;
}

static int moobl_show_options(struct seq_file *m, struct dentry *root)
{
	seq_puts(m, ",lower=");
	seq_path(m, &MOOBL_SB(root->d_sb)->lower, " \t\n\\,");
	if (MOOBL_SB(root->d_sb)->nolinks)
		seq_puts(m, ",nolinks");
	return 0;
}

static const struct super_operations moobl_sops = {
	.alloc_inode = moobl_alloc_inode,
	.free_inode = moobl_free_inode,
	.evict_inode = moobl_evict_inode,
	.statfs = moobl_statfs,
	.show_options = moobl_show_options,
};

/* ---- dentries --------------------------------------------------------- */

static int moobl_d_revalidate(struct dentry *dentry, unsigned int flags)
{
	struct dentry *lower;
	int rc = 1;

	if (flags & LOOKUP_RCU)
		return -ECHILD;
	lower = moobl_lower_dentry(dentry);
	if (lower->d_flags & DCACHE_OP_REVALIDATE)
		rc = lower->d_op->d_revalidate(lower, flags);
	if (d_really_is_positive(dentry)) {
		struct inode *inode = d_inode(dentry);

		fsstack_copy_attr_all(inode, moobl_lower_inode(inode));
		if (!inode->i_nlink)
			return 0;
	}
	return rc;
}

static void moobl_dentry_free_rcu(struct rcu_head *head)
{
	kfree(container_of(head, struct moobl_dentry, rcu));
}

static void moobl_d_release(struct dentry *dentry)
{
	struct moobl_dentry *info = dentry->d_fsdata;

	if (info) {
		path_put(&info->lower);
		call_rcu(&info->rcu, moobl_dentry_free_rcu);
	}
}

static const struct dentry_operations moobl_dops = {
	.d_revalidate = moobl_d_revalidate,
	.d_release = moobl_d_release,
};

/* ---- directory inode operations --------------------------------------- */

static struct dentry *moobl_lookup(struct inode *dir, struct dentry *dentry,
				   unsigned int flags)
{
	struct path *parent = moobl_lower_path(dentry->d_parent);
	struct dentry *lower_dentry;
	struct inode *lower_inode, *inode;
	int err;

	if (dentry->d_name.len > NAME_MAX)
		return ERR_PTR(-ENAMETOOLONG);
	lower_dentry = lookup_one_len_unlocked(dentry->d_name.name, parent->dentry,
					       dentry->d_name.len);
	if (IS_ERR(lower_dentry))
		return ERR_CAST(lower_dentry);
	err = moobl_set_lower_path(dentry, parent->mnt, lower_dentry);
	if (err) {
		dput(lower_dentry);
		return ERR_PTR(err);
	}
	fsstack_copy_attr_atime(dir, d_inode(parent->dentry));
	lower_inode = READ_ONCE(lower_dentry->d_inode);
	if (!lower_inode) {
		d_add(dentry, NULL);
		return NULL;
	}
	inode = moobl_get_inode(dir->i_sb, lower_inode);
	if (IS_ERR(inode))
		return ERR_CAST(inode);
	return d_splice_alias(inode, dentry);
}

/*
 * Lock the lower parent and re-lookup the lower child under that lock, as
 * ecryptfs does, so the dentry used for the mutation is current.
 */
static int moobl_lock_parent(struct dentry *dentry, struct dentry **lower_dentry,
			     struct inode **lower_dir)
{
	struct dentry *lower_parent = moobl_lower_dentry(dentry->d_parent);

	*lower_dir = d_inode(lower_parent);
	inode_lock_nested(*lower_dir, I_MUTEX_PARENT);
	*lower_dentry = lookup_one_len(dentry->d_name.name, lower_parent,
				       dentry->d_name.len);
	if (IS_ERR(*lower_dentry)) {
		inode_unlock(*lower_dir);
		return PTR_ERR(*lower_dentry);
	}
	return 0;
}

static void moobl_unlock_parent(struct dentry *lower_dentry, struct inode *lower_dir)
{
	dput(lower_dentry);
	inode_unlock(lower_dir);
}

static int moobl_instantiate(struct inode *dir, struct dentry *dentry,
			     struct dentry *lower_dentry, struct inode *lower_dir)
{
	struct inode *inode;

	if (d_really_is_negative(lower_dentry))
		return -EIO;
	inode = moobl_get_inode(dir->i_sb, d_inode(lower_dentry));
	if (IS_ERR(inode))
		return PTR_ERR(inode);
	fsstack_copy_attr_times(dir, lower_dir);
	fsstack_copy_inode_size(dir, lower_dir);
	d_instantiate(dentry, inode);
	return 0;
}

static int moobl_create(struct mnt_idmap *idmap, struct inode *dir,
			struct dentry *dentry, umode_t mode, bool excl)
{
	struct dentry *lower_dentry;
	struct inode *lower_dir;
	int err;

	err = moobl_lock_parent(dentry, &lower_dentry, &lower_dir);
	if (err)
		return err;
	err = vfs_create(&nop_mnt_idmap, lower_dir, lower_dentry, mode, excl);
	if (!err)
		err = moobl_instantiate(dir, dentry, lower_dentry, lower_dir);
	moobl_unlock_parent(lower_dentry, lower_dir);
	return err;
}

static int moobl_mkdir(struct mnt_idmap *idmap, struct inode *dir,
		       struct dentry *dentry, umode_t mode)
{
	struct dentry *lower_dentry;
	struct inode *lower_dir;
	int err;

	err = moobl_lock_parent(dentry, &lower_dentry, &lower_dir);
	if (err)
		return err;
	err = vfs_mkdir(&nop_mnt_idmap, lower_dir, lower_dentry, mode);
	if (!err)
		err = moobl_instantiate(dir, dentry, lower_dentry, lower_dir);
	if (!err)
		set_nlink(dir, lower_dir->i_nlink);
	moobl_unlock_parent(lower_dentry, lower_dir);
	return err;
}

static int moobl_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	struct dentry *lower_dentry;
	struct inode *lower_dir;
	int err;

	err = moobl_lock_parent(dentry, &lower_dentry, &lower_dir);
	if (err)
		return err;
	dget(lower_dentry);	/* keep the lower dentry from going negative under us */
	if (d_unhashed(lower_dentry) || d_really_is_negative(lower_dentry))
		err = -ENOENT;
	else
		err = vfs_unlink(&nop_mnt_idmap, lower_dir, lower_dentry, NULL);
	if (!err) {
		fsstack_copy_attr_times(dir, lower_dir);
		set_nlink(inode, moobl_lower_inode(inode)->i_nlink);
		inode_set_ctime_to_ts(inode, inode_get_ctime(dir));
	}
	dput(lower_dentry);
	moobl_unlock_parent(lower_dentry, lower_dir);
	return err;
}

static int moobl_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct dentry *lower_dentry;
	struct inode *lower_dir;
	int err;

	err = moobl_lock_parent(dentry, &lower_dentry, &lower_dir);
	if (err)
		return err;
	dget(lower_dentry);
	if (d_unhashed(lower_dentry) || d_really_is_negative(lower_dentry))
		err = -ENOENT;
	else
		err = vfs_rmdir(&nop_mnt_idmap, lower_dir, lower_dentry);
	if (!err) {
		fsstack_copy_attr_times(dir, lower_dir);
		set_nlink(dir, lower_dir->i_nlink);
		clear_nlink(d_inode(dentry));
	}
	dput(lower_dentry);
	moobl_unlock_parent(lower_dentry, lower_dir);
	if (!err)
		d_drop(dentry);
	return err;
}

static int moobl_rename(struct mnt_idmap *idmap, struct inode *old_dir,
			struct dentry *old_dentry, struct inode *new_dir,
			struct dentry *new_dentry, unsigned int flags)
{
	struct dentry *lower_old_dir = moobl_lower_dentry(old_dentry->d_parent);
	struct dentry *lower_new_dir = moobl_lower_dentry(new_dentry->d_parent);
	struct dentry *lower_old = dget(moobl_lower_dentry(old_dentry));
	struct dentry *lower_new = dget(moobl_lower_dentry(new_dentry));
	struct inode *target = d_inode(new_dentry);
	struct dentry *trap;
	struct renamedata rd = {
		.old_mnt_idmap = &nop_mnt_idmap,
		.old_dir = d_inode(lower_old_dir),
		.old_dentry = lower_old,
		.new_mnt_idmap = &nop_mnt_idmap,
		.new_dir = d_inode(lower_new_dir),
		.new_dentry = lower_new,
		.flags = flags,
	};
	int err;

	trap = lock_rename(lower_old_dir, lower_new_dir);
	if (IS_ERR(trap)) {
		err = PTR_ERR(trap);
		goto out_put;
	}
	err = -EINVAL;
	if (lower_old->d_parent != lower_old_dir || lower_new->d_parent != lower_new_dir)
		goto out_unlock;
	if (d_unhashed(lower_old) || d_unhashed(lower_new))
		goto out_unlock;
	if (trap == lower_old)
		goto out_unlock;
	err = -ENOTEMPTY;
	if (trap == lower_new)
		goto out_unlock;
	err = vfs_rename(&rd);
	if (err)
		goto out_unlock;
	if (target)
		fsstack_copy_attr_all(target, moobl_lower_inode(target));
	fsstack_copy_attr_all(new_dir, d_inode(lower_new_dir));
	if (new_dir != old_dir)
		fsstack_copy_attr_all(old_dir, d_inode(lower_old_dir));
	if ((flags & RENAME_EXCHANGE) && target)
		fsstack_copy_attr_all(d_inode(old_dentry),
				      moobl_lower_inode(d_inode(old_dentry)));
out_unlock:
	unlock_rename(lower_old_dir, lower_new_dir);
out_put:
	dput(lower_new);
	dput(lower_old);
	return err;
}

static int moobl_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry)
{
	struct inode *inode = d_inode(old_dentry);
	struct dentry *lower_old = moobl_lower_dentry(old_dentry);
	struct dentry *lower_dentry;
	struct inode *lower_dir;
	int err;

	if (MOOBL_SB(dir->i_sb)->nolinks)
		return -EPERM;
	err = moobl_lock_parent(new_dentry, &lower_dentry, &lower_dir);
	if (err)
		return err;
	err = vfs_link(lower_old, &nop_mnt_idmap, lower_dir, lower_dentry, NULL);
	if (!err)
		err = moobl_instantiate(dir, new_dentry, lower_dentry, lower_dir);
	if (!err)
		set_nlink(inode, moobl_lower_inode(inode)->i_nlink);
	moobl_unlock_parent(lower_dentry, lower_dir);
	return err;
}

static int moobl_symlink(struct mnt_idmap *idmap, struct inode *dir,
			 struct dentry *dentry, const char *symname)
{
	struct dentry *lower_dentry;
	struct inode *lower_dir;
	int err;

	if (MOOBL_SB(dir->i_sb)->nolinks)
		return -EPERM;
	err = moobl_lock_parent(dentry, &lower_dentry, &lower_dir);
	if (err)
		return err;
	err = vfs_symlink(&nop_mnt_idmap, lower_dir, lower_dentry, symname);
	if (!err)
		err = moobl_instantiate(dir, dentry, lower_dentry, lower_dir);
	moobl_unlock_parent(lower_dentry, lower_dir);
	return err;
}

static const char *moobl_get_link(struct dentry *dentry, struct inode *inode,
				  struct delayed_call *done)
{
	if (!dentry)
		return ERR_PTR(-ECHILD);
	fsstack_copy_attr_atime(inode, moobl_lower_inode(inode));
	return vfs_get_link(moobl_lower_dentry(dentry), done);
}

static int moobl_fiemap(struct inode *inode, struct fiemap_extent_info *fieinfo,
			u64 start, u64 len)
{
	struct inode *lower = moobl_lower_inode(inode);
	const struct cred *old;
	int err;

	if (!lower->i_op->fiemap)
		return -EOPNOTSUPP;
	old = override_creds(MOOBL_SB(inode->i_sb)->creds);
	err = lower->i_op->fiemap(lower, fieinfo, start, len);
	revert_creds(old);
	return err;
}

/* ---- extended attributes: opaque pass-through (typed validation is phase 2c) */

static ssize_t moobl_listxattr(struct dentry *dentry, char *list, size_t size)
{
	ssize_t ret = vfs_listxattr(moobl_lower_dentry(dentry), list, size);

	if (ret >= 0)
		fsstack_copy_attr_atime(d_inode(dentry), moobl_lower_inode(d_inode(dentry)));
	return ret;
}

static int moobl_xattr_get(const struct xattr_handler *handler, struct dentry *dentry,
			   struct inode *inode, const char *name, void *value, size_t size)
{
	return vfs_getxattr(&nop_mnt_idmap, moobl_lower_dentry(dentry),
			    xattr_full_name(handler, name), value, size);
}

static int moobl_xattr_set(const struct xattr_handler *handler, struct mnt_idmap *idmap,
			   struct dentry *dentry, struct inode *inode, const char *name,
			   const void *value, size_t size, int flags)
{
	struct dentry *lower = moobl_lower_dentry(dentry);
	const char *full = xattr_full_name(handler, name);
	int err;

	if (value)
		err = vfs_setxattr(&nop_mnt_idmap, lower, full, value, size, flags);
	else
		err = vfs_removexattr(&nop_mnt_idmap, lower, full);
	if (!err)
		fsstack_copy_attr_all(inode, d_inode(lower));
	return err;
}

static const struct xattr_handler moobl_xattr_handler = {
	.prefix = "",	/* every namespace; permission checks happen in the VFS and below */
	.get = moobl_xattr_get,
	.set = moobl_xattr_set,
};

static const struct xattr_handler * const moobl_xattr_handlers[] = {
	&moobl_xattr_handler,
	NULL
};

/* ---- attribute operations shared by files and directories -------------- */

static int moobl_getattr(struct mnt_idmap *idmap, const struct path *path,
			 struct kstat *stat, u32 request_mask, unsigned int flags)
{
	struct dentry *dentry = path->dentry;
	struct inode *inode = d_inode(dentry);
	struct kstat lower_stat;
	int err;

	err = vfs_getattr_nosec(moobl_lower_path(dentry), &lower_stat, request_mask, flags);
	if (err)
		return err;
	fsstack_copy_attr_all(inode, moobl_lower_inode(inode));
	fsstack_copy_inode_size(inode, moobl_lower_inode(inode));
	generic_fillattr(&nop_mnt_idmap, request_mask, inode, stat);
	stat->blocks = lower_stat.blocks;
	stat->blksize = lower_stat.blksize;
	return 0;
}

static int moobl_setattr(struct mnt_idmap *idmap, struct dentry *dentry,
			 struct iattr *ia)
{
	struct inode *inode = d_inode(dentry);
	struct dentry *lower_dentry = moobl_lower_dentry(dentry);
	struct inode *lower_inode = d_inode(lower_dentry);
	struct iattr lower_ia = *ia;
	int err;

	err = setattr_prepare(&nop_mnt_idmap, dentry, ia);
	if (err)
		return err;
	if (ia->ia_valid & ATTR_FILE)
		lower_ia.ia_file = ia->ia_file->private_data;
	inode_lock(lower_inode);
	err = notify_change(&nop_mnt_idmap, lower_dentry, &lower_ia, NULL);
	inode_unlock(lower_inode);
	if (!err) {
		fsstack_copy_attr_all(inode, lower_inode);
		fsstack_copy_inode_size(inode, lower_inode);
	}
	return err;
}

static int moobl_permission(struct mnt_idmap *idmap, struct inode *inode, int mask)
{
	return inode_permission(&nop_mnt_idmap, moobl_lower_inode(inode), mask);
}

static const struct inode_operations moobl_dir_iops = {
	.lookup = moobl_lookup,
	.create = moobl_create,
	.mkdir = moobl_mkdir,
	.unlink = moobl_unlink,
	.rmdir = moobl_rmdir,
	.rename = moobl_rename,
	.link = moobl_link,
	.symlink = moobl_symlink,
	.getattr = moobl_getattr,
	.setattr = moobl_setattr,
	.permission = moobl_permission,
	.listxattr = moobl_listxattr,
};

static const struct inode_operations moobl_file_iops = {
	.getattr = moobl_getattr,
	.setattr = moobl_setattr,
	.permission = moobl_permission,
	.listxattr = moobl_listxattr,
	.fiemap = moobl_fiemap,
};

static const struct inode_operations moobl_symlink_iops = {
	.get_link = moobl_get_link,
	.getattr = moobl_getattr,
	.setattr = moobl_setattr,
	.permission = moobl_permission,
	.listxattr = moobl_listxattr,
};

static const struct inode_operations moobl_special_iops = {
	.getattr = moobl_getattr,
	.setattr = moobl_setattr,
	.permission = moobl_permission,
	.listxattr = moobl_listxattr,
};

/* ---- regular file operations ------------------------------------------ */

static void moobl_file_accessed(struct file *file)
{
	if (!(file->f_flags & O_NOATIME))
		touch_atime(&file->f_path);
}

static void moobl_file_modified(struct file *file)
{
	struct inode *inode = file_inode(file);

	fsstack_copy_attr_all(inode, moobl_lower_inode(inode));
	fsstack_copy_inode_size(inode, moobl_lower_inode(inode));
}

static void moobl_file_end_write(struct file *file, loff_t pos, ssize_t ret)
{
	moobl_file_modified(file);
}

#define MOOBL_CTX(file) {						\
	.cred = MOOBL_SB(file_inode(file)->i_sb)->creds,		\
	.user_file = (file),						\
	.accessed = moobl_file_accessed,				\
	.end_write = moobl_file_end_write,				\
}

static int moobl_open(struct inode *inode, struct file *file)
{
	struct path *lower = moobl_lower_path(file->f_path.dentry);
	struct file *realfile;

	/* Creation and truncation were handled by the VFS before open. */
	file->f_flags &= ~(O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC);
	realfile = backing_file_open(file, file->f_flags, lower,
				     MOOBL_SB(inode->i_sb)->creds);
	if (IS_ERR(realfile))
		return PTR_ERR(realfile);
	file->private_data = realfile;
	return 0;
}

static int moobl_release(struct inode *inode, struct file *file)
{
	fput(file->private_data);
	return 0;
}

static ssize_t moobl_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	struct file *file = iocb->ki_filp;
	struct backing_file_ctx ctx = MOOBL_CTX(file);

	if (!iov_iter_count(iter))
		return 0;
	return backing_file_read_iter(file->private_data, iter, iocb,
				      iocb->ki_flags, &ctx);
}

static ssize_t moobl_write_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	struct file *file = iocb->ki_filp;
	struct backing_file_ctx ctx = MOOBL_CTX(file);
	struct inode *inode = file_inode(file);
	ssize_t ret;

	if (!iov_iter_count(iter))
		return 0;
	inode_lock(inode);
	ret = file_remove_privs(file);
	if (!ret)
		ret = backing_file_write_iter(file->private_data, iter, iocb,
					      iocb->ki_flags, &ctx);
	inode_unlock(inode);
	return ret;
}

static ssize_t moobl_splice_read(struct file *in, loff_t *ppos,
				 struct pipe_inode_info *pipe, size_t len,
				 unsigned int flags)
{
	struct backing_file_ctx ctx = MOOBL_CTX(in);

	return backing_file_splice_read(in->private_data, ppos, pipe, len, flags, &ctx);
}

static ssize_t moobl_splice_write(struct pipe_inode_info *pipe, struct file *out,
				  loff_t *ppos, size_t len, unsigned int flags)
{
	struct backing_file_ctx ctx = MOOBL_CTX(out);
	struct inode *inode = file_inode(out);
	ssize_t ret;

	inode_lock(inode);
	ret = file_remove_privs(out);
	if (!ret)
		ret = backing_file_splice_write(pipe, out->private_data, ppos, len,
						flags, &ctx);
	inode_unlock(inode);
	return ret;
}

static int moobl_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct backing_file_ctx ctx = MOOBL_CTX(file);

	return backing_file_mmap(file->private_data, vma, &ctx);
}

static int moobl_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	const struct cred *old = override_creds(MOOBL_SB(file_inode(file)->i_sb)->creds);
	int err = vfs_fsync_range(file->private_data, start, end, datasync);

	revert_creds(old);
	return err;
}

static loff_t moobl_llseek(struct file *file, loff_t offset, int whence)
{
	struct file *real = file->private_data;
	struct inode *inode = file_inode(file);
	loff_t ret;

	if (offset == 0) {
		if (whence == SEEK_CUR)
			return file->f_pos;
		if (whence == SEEK_SET)
			return vfs_setpos(file, 0, 0);
	}
	inode_lock(inode);
	real->f_pos = file->f_pos;
	ret = vfs_llseek(real, offset, whence);
	file->f_pos = real->f_pos;
	inode_unlock(inode);
	return ret;
}

static long moobl_fallocate(struct file *file, int mode, loff_t offset, loff_t len)
{
	struct inode *inode = file_inode(file);
	const struct cred *old;
	long ret;

	inode_lock(inode);
	ret = file_remove_privs(file);
	if (!ret) {
		old = override_creds(MOOBL_SB(inode->i_sb)->creds);
		ret = vfs_fallocate(file->private_data, mode, offset, len);
		revert_creds(old);
		moobl_file_modified(file);
	}
	inode_unlock(inode);
	return ret;
}

static loff_t moobl_remap_file_range(struct file *in, loff_t pos_in, struct file *out,
				     loff_t pos_out, loff_t len, unsigned int flags)
{
	struct inode *inode_out = file_inode(out);
	const struct cred *old;
	loff_t ret;

	if (file_inode(in)->i_sb != inode_out->i_sb)
		return -EXDEV;
	old = override_creds(MOOBL_SB(inode_out->i_sb)->creds);
	if (flags & REMAP_FILE_DEDUP)
		ret = vfs_dedupe_file_range_one(in->private_data, pos_in, out->private_data,
						pos_out, len, flags);
	else
		ret = vfs_clone_file_range(in->private_data, pos_in, out->private_data,
					   pos_out, len, flags);
	revert_creds(old);
	moobl_file_modified(out);
	return ret;
}

static ssize_t moobl_copy_file_range(struct file *in, loff_t pos_in, struct file *out,
				     loff_t pos_out, size_t len, unsigned int flags)
{
	const struct cred *old;
	ssize_t ret;

	if (file_inode(in)->i_sb != file_inode(out)->i_sb)
		return -EXDEV;
	old = override_creds(MOOBL_SB(file_inode(out)->i_sb)->creds);
	ret = vfs_copy_file_range(in->private_data, pos_in, out->private_data, pos_out,
				  len, flags);
	revert_creds(old);
	moobl_file_modified(out);
	return ret;
}

static int moobl_flush(struct file *file, fl_owner_t id)
{
	struct file *real = file->private_data;

	if (real->f_op->flush)
		return real->f_op->flush(real, id);
	return 0;
}

static const struct file_operations moobl_file_fops = {
	.open = moobl_open,
	.release = moobl_release,
	.llseek = moobl_llseek,
	.read_iter = moobl_read_iter,
	.write_iter = moobl_write_iter,
	.splice_read = moobl_splice_read,
	.splice_write = moobl_splice_write,
	.mmap = moobl_mmap,
	.fsync = moobl_fsync,
	.fallocate = moobl_fallocate,
	.flush = moobl_flush,
	.remap_file_range = moobl_remap_file_range,
	.copy_file_range = moobl_copy_file_range,
};

/* ---- directory file operations ---------------------------------------- */

static int moobl_dir_open(struct inode *inode, struct file *file)
{
	struct file *real = dentry_open(moobl_lower_path(file->f_path.dentry),
					O_RDONLY | O_DIRECTORY,
					MOOBL_SB(inode->i_sb)->creds);

	if (IS_ERR(real))
		return PTR_ERR(real);
	file->private_data = real;
	return 0;
}

static int moobl_iterate(struct file *file, struct dir_context *ctx)
{
	struct file *real = file->private_data;
	int err;

	real->f_pos = ctx->pos;
	err = iterate_dir(real, ctx);
	ctx->pos = real->f_pos;
	fsstack_copy_attr_atime(file_inode(file), file_inode(real));
	return err;
}

static loff_t moobl_dir_llseek(struct file *file, loff_t offset, int whence)
{
	struct file *real = file->private_data;
	loff_t ret;

	inode_lock(file_inode(file));
	ret = vfs_llseek(real, offset, whence);
	if (ret >= 0)
		file->f_pos = ret;
	inode_unlock(file_inode(file));
	return ret;
}

static const struct file_operations moobl_dir_fops = {
	.open = moobl_dir_open,
	.release = moobl_release,
	.iterate_shared = moobl_iterate,
	.llseek = moobl_dir_llseek,
	.read = generic_read_dir,
	.fsync = moobl_fsync,
};

/* ---- mount ------------------------------------------------------------- */

struct moobl_fc {
	char *lower;
	bool nolinks;
};

enum { Opt_lower, Opt_nolinks };

static const struct fs_parameter_spec moobl_params[] = {
	fsparam_string("lower", Opt_lower),
	fsparam_flag("nolinks", Opt_nolinks),
	{}
};

static int moobl_parse_param(struct fs_context *fc, struct fs_parameter *param)
{
	struct moobl_fc *ctx = fc->fs_private;
	struct fs_parse_result result;
	int opt = fs_parse(fc, moobl_params, param, &result);

	if (opt < 0)
		return opt;
	switch (opt) {
	case Opt_lower:
		kfree(ctx->lower);
		ctx->lower = param->string;
		param->string = NULL;
		break;
	case Opt_nolinks:
		ctx->nolinks = true;
		break;
	}
	return 0;
}

static int moobl_fill_super(struct super_block *sb, struct fs_context *fc)
{
	struct moobl_fc *ctx = fc->fs_private;
	struct moobl_sb *sbi;
	struct inode *root_inode;
	struct dentry *root;
	int err;

	if (!ctx->lower)
		return invalfc(fc, "lower= is required");
	sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	sb->s_fs_info = sbi;
	err = kern_path(ctx->lower, LOOKUP_FOLLOW | LOOKUP_DIRECTORY, &sbi->lower);
	if (err)
		return invalfc(fc, "cannot resolve lower directory");
	if (sbi->lower.dentry->d_sb->s_magic == MOOBL_SUPER_MAGIC)
		return invalfc(fc, "lower cannot be another moobfs_layer mount");
	sb->s_stack_depth = sbi->lower.dentry->d_sb->s_stack_depth + 1;
	if (sb->s_stack_depth > FILESYSTEM_MAX_STACK_DEPTH)
		return invalfc(fc, "maximum filesystem stacking depth exceeded");
	sbi->creds = prepare_creds();
	if (!sbi->creds)
		return -ENOMEM;
	sbi->nolinks = ctx->nolinks;
	sb->s_xattr = moobl_xattr_handlers;
	sb->s_magic = MOOBL_SUPER_MAGIC;
	sb->s_op = &moobl_sops;
	sb->s_d_op = &moobl_dops;
	sb->s_maxbytes = sbi->lower.dentry->d_sb->s_maxbytes;
	sb->s_time_gran = sbi->lower.dentry->d_sb->s_time_gran;
	sb->s_blocksize = sbi->lower.dentry->d_sb->s_blocksize;
	sb->s_blocksize_bits = sbi->lower.dentry->d_sb->s_blocksize_bits;
	sb->s_flags |= SB_NOSEC;	/* lower enforces suid/sgid clearing */
	sb->s_iflags |= SB_I_NOEXEC | SB_I_NODEV;

	root_inode = moobl_get_inode(sb, d_inode(sbi->lower.dentry));
	if (IS_ERR(root_inode))
		return PTR_ERR(root_inode);
	root = d_make_root(root_inode);
	if (!root)
		return -ENOMEM;
	sb->s_root = root;
	err = moobl_set_lower_path(root, sbi->lower.mnt, dget(sbi->lower.dentry));
	if (err)
		return err;
	return 0;
}

static int moobl_get_tree(struct fs_context *fc)
{
	return get_tree_nodev(fc, moobl_fill_super);
}

static void moobl_free_fc(struct fs_context *fc)
{
	struct moobl_fc *ctx = fc->fs_private;

	if (ctx) {
		kfree(ctx->lower);
		kfree(ctx);
	}
}

static const struct fs_context_operations moobl_context_ops = {
	.parse_param = moobl_parse_param,
	.get_tree = moobl_get_tree,
	.free = moobl_free_fc,
};

static int moobl_init_fs_context(struct fs_context *fc)
{
	struct moobl_fc *ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);

	if (!ctx)
		return -ENOMEM;
	fc->fs_private = ctx;
	fc->ops = &moobl_context_ops;
	return 0;
}

static void moobl_kill_sb(struct super_block *sb)
{
	struct moobl_sb *sbi = sb->s_fs_info;

	kill_anon_super(sb);
	if (sbi) {
		if (sbi->creds)
			put_cred(sbi->creds);
		path_put(&sbi->lower);
		kfree(sbi);
	}
}

static struct file_system_type moobl_fs_type = {
	.owner = THIS_MODULE,
	.name = MOOBL_NAME,
	.init_fs_context = moobl_init_fs_context,
	.parameters = moobl_params,
	.kill_sb = moobl_kill_sb,
};
MODULE_ALIAS_FS(MOOBL_NAME);

static void moobl_inode_init_once(void *obj)
{
	inode_init_once(&((struct moobl_inode *)obj)->vfs_inode);
}

static int __init moobl_init(void)
{
	int err;

	moobl_inode_cache = kmem_cache_create("moobl_inode", sizeof(struct moobl_inode),
					      0, SLAB_RECLAIM_ACCOUNT | SLAB_ACCOUNT,
					      moobl_inode_init_once);
	if (!moobl_inode_cache)
		return -ENOMEM;
	err = register_filesystem(&moobl_fs_type);
	if (err)
		kmem_cache_destroy(moobl_inode_cache);
	return err;
}

static void __exit moobl_exit(void)
{
	unregister_filesystem(&moobl_fs_type);
	rcu_barrier();
	kmem_cache_destroy(moobl_inode_cache);
}

module_init(moobl_init);
module_exit(moobl_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Studio Moob");
MODULE_DESCRIPTION("MoobFS memory layer (phase 2a pass-through)");
