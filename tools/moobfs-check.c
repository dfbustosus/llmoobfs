/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * moobfs-check: validate every typed attribute under a lower directory (or a
 * mounted layer) and the format marker on its root. Diagnoses only; never
 * repairs. Output is one JSON object. Exit: 0 clean, 2 problems found, 1 error.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ftw.h>
#include <sys/xattr.h>
#include "moobfs_attr.h"

static unsigned long files, attrs, problems;
static int first_problem = 1;
static size_t root_len;

static void report(const char *path, const char *name, const char *why)
{
	const char *rel = path + root_len;

	if (*rel == '/')
		rel++;
	printf("%s\n    {\"path\": \"", first_problem ? "" : ",");
	for (; *rel; rel++) {
		if (*rel == '"' || *rel == '\\') putchar('\\');
		if ((unsigned char)*rel < 0x20) printf("\\u%04x", (unsigned char)*rel);
		else putchar(*rel);
	}
	printf("\", \"attribute\": \"%s\", \"problem\": \"%s\"}", name ? name : "", why);
	first_problem = 0;
	problems++;
}

static int visit(const char *path, const struct stat *sb, int flag, struct FTW *ftw)
{
	char *list, *p;
	ssize_t len;
	unsigned long per_file = 0;
	(void)sb; (void)ftw;

	if (flag == FTW_SL || flag == FTW_SLN || flag == FTW_NS || flag == FTW_DNR)
		return 0;
	files++;
	len = llistxattr(path, NULL, 0);
	if (len < 0) {
		if (errno == ENOTSUP || errno == EOPNOTSUPP)
			return 0;
		report(path, NULL, "cannot list attributes");
		return 0;
	}
	if (!len)
		return 0;
	list = malloc((size_t)len);
	if (!list)
		return 1;
	len = llistxattr(path, list, (size_t)len);
	if (len < 0) {
		free(list);
		report(path, NULL, "cannot list attributes");
		return 0;
	}
	for (p = list; p < list + len; p += strlen(p) + 1) {
		ma_u8 value[MOOB_ATTR_VALUE_MAX + 1];
		const char *name = p + MOOB_ATTR_PREFIX_LEN;
		size_t nlen;
		ssize_t n;
		int err;

		if (strncmp(p, MOOB_ATTR_PREFIX, MOOB_ATTR_PREFIX_LEN))
			continue;
		nlen = strlen(name);
		if (moob_attr_name_reserved(name, nlen)) {
			if (ftw->level != 0)
				report(path, name, "format marker outside the root");
			continue;
		}
		attrs++;
		per_file++;
		if (!moob_attr_name_ok(name, nlen)) {
			report(path, name, moob_attr_strerror(MOOB_ATTR_EBADNAME));
			continue;
		}
		n = lgetxattr(path, p, value, sizeof value);
		if (n < 0) {
			report(path, name, errno == ERANGE ? "value larger than the maximum" : "cannot read value");
			continue;
		}
		err = moob_attr_validate(value, (size_t)n);
		if (err)
			report(path, name, moob_attr_strerror(err));
	}
	if (per_file > MOOB_ATTR_MAX_PER_FILE)
		report(path, NULL, "more typed attributes than the per-file limit");
	free(list);
	return 0;
}

int main(int argc, char **argv)
{
	ma_u8 value[16];
	ssize_t n;
	unsigned int version = 0;
	const char *format_state;

	if (argc != 2) {
		fprintf(stderr, "usage: moobfs-check <directory>\n");
		return 64;
	}
	root_len = strlen(argv[1]);
	n = lgetxattr(argv[1], MOOB_ATTR_PREFIX MOOB_ATTR_FORMAT_NAME, value, sizeof value);
	if (n < 0 && errno == ENODATA) {
		format_state = "missing";
	} else if (n < 0) {
		format_state = "unreadable";
	} else {
		version = moob_attr_parse_format(value, (size_t)n);
		format_state = !version ? "invalid" : version > MOOB_ATTR_FORMAT_VERSION ? "newer" : "ok";
	}
	printf("{\n  \"format\": {\"state\": \"%s\", \"version\": %u, \"supported\": %u},\n  \"problems\": [",
	       format_state, version, MOOB_ATTR_FORMAT_VERSION);
	if (strcmp(format_state, "ok") && strcmp(format_state, "missing"))
		report(argv[1], MOOB_ATTR_FORMAT_NAME, format_state);
	if (nftw(argv[1], visit, 32, FTW_PHYS | FTW_MOUNT)) {
		fprintf(stderr, "moobfs-check: %s: %s\n", argv[1], strerror(errno));
		return 1;
	}
	printf("%s],\n  \"files\": %lu,\n  \"typed_attributes\": %lu,\n  \"problem_count\": %lu\n}\n",
	       first_problem ? "" : "\n  ", files, attrs, problems);
	return problems ? 2 : 0;
}
