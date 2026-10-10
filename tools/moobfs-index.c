/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * moobfs-index: user-space oracle for the kernel QUERY. Walks a directory,
 * reads every typed attribute with the shared encoding and answers the same
 * conjunctive queries, printing "<ino>\t<path>" lines sorted by inode number.
 *
 *   moobfs-index <dir> <name> <op> <type> <value> [... up to 4 terms]
 */
#define _GNU_SOURCE
#include <ftw.h>
#include <sys/xattr.h>
#include <sys/stat.h>
#include <inttypes.h>
#include "query-common.h"

static struct moob_query_term terms[MOOB_QUERY_MAX_TERMS];
static int nterms;
static size_t root_len;

struct hit { uint64_t ino; char *path; };
static struct hit *hits;
static size_t nhits, cap;

static int value_cmp(int type, const ma_u8 *a, size_t alen, const ma_u8 *b, size_t blen)
{
	switch (type) {
	case MOOB_T_INT64: case MOOB_T_TIME: {
		int64_t x = (int64_t)moob_attr_get64(a), y = (int64_t)moob_attr_get64(b);
		return x < y ? -1 : x > y ? 1 : 0;
	}
	case MOOB_T_DOUBLE: {
		ma_u64 xb = moob_attr_get64(a), yb = moob_attr_get64(b);
		ma_u64 x = (xb >> 63) ? ~xb : (xb | (1ULL << 63));
		ma_u64 y = (yb >> 63) ? ~yb : (yb | (1ULL << 63));
		return x < y ? -1 : x > y ? 1 : 0;
	}
	case MOOB_T_BOOL:
		return a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0;
	default: {
		int c = memcmp(a, b, alen < blen ? alen : blen);
		if (c) return c;
		return alen < blen ? -1 : alen > blen ? 1 : 0;
	}
	}
}

static int term_matches(const char *path, const struct moob_query_term *t)
{
	char full[MOOB_ATTR_PREFIX_LEN + MOOB_QUERY_NAME_MAX + 1];
	ma_u8 v[MOOB_ATTR_VALUE_MAX + 1];
	ssize_t n;
	int c;

	snprintf(full, sizeof full, "%s%s", MOOB_ATTR_PREFIX, t->name);
	n = lgetxattr(path, full, v, sizeof v);
	if (n <= 0 || moob_attr_validate(v, (size_t)n) != MOOB_ATTR_OK || v[0] == MOOB_T_RAW)
		return 0;
	if (t->op == MOOB_OP_EXISTS)
		return 1;
	if (v[0] != t->type)
		return 0;
	if (t->op == MOOB_OP_PREFIX)
		return (size_t)n - 1 >= t->len && !memcmp(v + 1, t->value, t->len);
	c = value_cmp(t->type, v + 1, (size_t)n - 1, t->value, t->len);
	switch (t->op) {
	case MOOB_OP_EQ: return c == 0;
	case MOOB_OP_LT: return c < 0;
	case MOOB_OP_LE: return c <= 0;
	case MOOB_OP_GT: return c > 0;
	case MOOB_OP_GE: return c >= 0;
	}
	return 0;
}

static int visit(const char *path, const struct stat *sb, int flag, struct FTW *ftw)
{
	int i;
	(void)ftw;

	if (flag != FTW_F && flag != FTW_D)
		return 0;
	for (i = 0; i < nterms; i++)
		if (!term_matches(path, &terms[i]))
			return 0;
	if (nhits == cap) {
		cap = cap ? cap * 2 : 64;
		hits = realloc(hits, cap * sizeof *hits);
		if (!hits) return 1;
	}
	hits[nhits].ino = sb->st_ino;
	hits[nhits].path = strdup(path + root_len + (path[root_len] == '/'));
	nhits++;
	return 0;
}

static int by_ino(const void *a, const void *b)
{
	const struct hit *x = a, *y = b;
	return x->ino < y->ino ? -1 : x->ino > y->ino;
}

int main(int argc, char **argv)
{
	size_t i;

	if (argc < 6) {
		fprintf(stderr, "usage: moobfs-index <dir> <name> <op> <type> <value> [...]\n");
		return 64;
	}
	nterms = parse_terms(argc, argv, 2, terms);
	if (nterms < 1) {
		fprintf(stderr, "moobfs-index: invalid query terms\n");
		return 65;
	}
	root_len = strlen(argv[1]);
	if (nftw(argv[1], visit, 32, FTW_PHYS | FTW_MOUNT)) {
		perror("nftw");
		return 1;
	}
	qsort(hits, nhits, sizeof *hits, by_ino);
	for (i = 0; i < nhits; i++)
		printf("%" PRIu64 "\t%s\n", hits[i].ino, hits[i].path);
	return 0;
}
