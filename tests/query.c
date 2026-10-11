/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * ioctl client for the MoobFS layer, used by the VM tests.
 *   query [--uid N] <mount> <name> <op> <type> <value> [...]   prints "<ino>\t<path>" sorted by ino
 *   --uid N drops privileges to uid/gid N before opening, to test isolation.
 *   query <mount> info                               prints GET_INFO fields
 *   query <mount> invalid                            checks that malformed requests are refused
 *   query [--uid N] <domain-dir> q2 <terms...>       QUERY2 on a domain directory (paths relative to it)
 *   query <domain-dir> q2invalid                     malformed QUERY2 requests are refused
 *   query [--uid N] <dir> q2errno <terms...>         prints the errno of QUERY2 (0 on success)
 *   query [--uid N] <x> rename <old> <new>           prints the errno of rename(2) (0 on success)
 *   query [--uid N] <mount> race <seconds>           repeats QUERY kind eq note while another
 *                                                    process changes modes, names and inodes;
 *                                                    every result must be well formed and
 *                                                    openable unless it was concurrently removed
 */
#include <time.h>
#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <inttypes.h>
#include "../tools/query-common.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s (errno=%d %s)\n", __LINE__, #c, errno, strerror(errno)); return 1; } } while (0)

static int by_ino(const void *a, const void *b)
{
	const struct moob_query_result *x = a, *y = b;
	return x->ino < y->ino ? -1 : x->ino > y->ino;
}

static int run_query(int fd, struct moob_query *q, struct moob_query_result *res, __u32 limit)
{
	q->version = MOOB_UAPI_VERSION;
	q->limit = limit;
	q->results = (__u64)(uintptr_t)res;
	return ioctl(fd, MOOB_IOC_QUERY, q);
}

int main(int argc, char **argv)
{
	int fd;
	struct moob_query q;
	struct moob_query_result *res;

	if (argc >= 3 && !strcmp(argv[1], "--uid")) {
		uid_t u = (uid_t)strtoul(argv[2], NULL, 10);
		if (setgid(u) || setuid(u)) { perror("setuid"); return 1; }
		argv += 2; argc -= 2;
	}
	if (argc >= 4 && !strcmp(argv[2], "open")) {	/* query [--uid N] <mount> open <relpath> */
		char full[8192]; char c;
		snprintf(full, sizeof full, "%s/%s", argv[1], argv[3]);
		fd = open(full, O_RDONLY);
		if (fd < 0) { printf("open %s: %s\n", argv[3], strerror(errno)); return errno == EACCES ? 3 : 2; }
		if (read(fd, &c, 1) < 0) { printf("read %s: %s\n", argv[3], strerror(errno)); return 2; }
		printf("open %s: ok\n", argv[3]);
		return 0;
	}
	if (argc >= 5 && !strcmp(argv[2], "rename")) {
		printf("rename %d\n", rename(argv[3], argv[4]) ? errno : 0);
		return 0;
	}
	if (argc < 3)
		return 64;
	fd = open(argv[1], O_RDONLY | O_DIRECTORY);
	if (fd < 0 && argc >= 3 && !strcmp(argv[2], "q2errno")) {
		printf("q2 %d\n", errno);
		return 0;
	}
	if (fd < 0) { perror("open"); return 1; }
	res = calloc(MOOB_QUERY_MAX_LIMIT, sizeof *res);
	if (!res) return 1;

	if (!strcmp(argv[2], "q2") || !strcmp(argv[2], "q2errno")) {
		struct moob_query2 q2;
		int n, r;
		__u32 i;

		memset(&q2, 0, sizeof q2);
		n = parse_terms(argc, argv, 3, q2.terms);
		if (n < 1) { fprintf(stderr, "query: invalid terms\n"); return 65; }
		q2.version = MOOB_QUERY2_VERSION;
		q2.nterms = (__u32)n;
		q2.limit = MOOB_QUERY_MAX_LIMIT;
		q2.results = (__u64)(uintptr_t)res;
		r = ioctl(fd, MOOB_IOC_QUERY2, &q2);
		if (!strcmp(argv[2], "q2errno")) { printf("q2 %d\n", r ? errno : 0); return 0; }
		if (r) { perror("MOOB_IOC_QUERY2"); return 1; }
		qsort(res, q2.count, sizeof *res, by_ino);
		for (i = 0; i < q2.count; i++)
			printf("%" PRIu64 "\t%s%s\n", (uint64_t)res[i].ino, res[i].path,
			       (res[i].flags & MOOB_RES_PATH_UNKNOWN) ? "?" : "");
		if (q2.truncated)
			fprintf(stderr, "query: results truncated\n");
		return 0;
	}
	if (!strcmp(argv[2], "q2invalid")) {
		struct moob_query2 q2;
		int sub;
#define Q2BAD(stmt) do { memset(&q2, 0, sizeof q2); q2.version = MOOB_QUERY2_VERSION; q2.nterms = 1; \
	q2.limit = 10; q2.results = (__u64)(uintptr_t)res; strcpy(q2.terms[0].name, "kind"); \
	q2.terms[0].op = MOOB_OP_EXISTS; stmt; errno = 0; \
	CHECK(ioctl(fd, MOOB_IOC_QUERY2, &q2) == -1 && errno == EINVAL); } while (0)
		memset(&q2, 0, sizeof q2); q2.version = MOOB_QUERY2_VERSION; q2.nterms = 1; q2.limit = 10;
		q2.results = (__u64)(uintptr_t)res; strcpy(q2.terms[0].name, "kind"); q2.terms[0].op = MOOB_OP_EXISTS;
		CHECK(!ioctl(fd, MOOB_IOC_QUERY2, &q2));		/* baseline is valid */
		Q2BAD(q2.version = 1);
		Q2BAD(q2.version = 3);
		Q2BAD(q2.nterms = 0);
		Q2BAD(q2.nterms = 5);
		Q2BAD(q2.limit = 0);
		Q2BAD(q2.limit = MOOB_QUERY_MAX_LIMIT + 1);
		Q2BAD(q2.flags = 1);
		Q2BAD(strcpy(q2.order_by, "created"));
		Q2BAD(q2.order = 1);
		Q2BAD(q2.pad[3] = 1);
		Q2BAD(q2.cursor[127] = 1);
		Q2BAD(q2.reserved[3] = 1);
		Q2BAD(q2.terms[0].reserved[0] = 1);			/* MISSING_OK is a later phase */
		Q2BAD(strcpy(q2.terms[0].name, "Kind"));
		/* QUERY2 answers only on a domain directory, never on a subdirectory of it */
		{
			char sub_path[4096];
			snprintf(sub_path, sizeof sub_path, "%s/q2sub", argv[1]);
			mkdir(sub_path, 0700);
			sub = open(sub_path, O_RDONLY | O_DIRECTORY); CHECK(sub >= 0);
			memset(&q2, 0, sizeof q2); q2.version = MOOB_QUERY2_VERSION; q2.nterms = 1; q2.limit = 10;
			q2.results = (__u64)(uintptr_t)res; strcpy(q2.terms[0].name, "kind"); q2.terms[0].op = MOOB_OP_EXISTS;
			errno = 0; CHECK(ioctl(sub, MOOB_IOC_QUERY2, &q2) == -1 && errno == ENOTTY);
			close(sub); rmdir(sub_path);
		}
		puts("QUERY2_INVALID_REQUESTS_REFUSED");
		return 0;
	}
	if (!strcmp(argv[2], "info")) {
		struct moob_info info;
		CHECK(!ioctl(fd, MOOB_IOC_GET_INFO, &info));
		printf("uapi=%u format=%u name_max=%u string_max=%u raw_max=%u per_file=%u\n",
		       info.uapi_version, info.format_version, info.attr_name_max, info.attr_string_max,
		       info.attr_raw_max, info.attr_max_per_file);
		printf("domain_depth=%u heterogeneous=%u fallback_domains=%u\n", info.domain_depth,
		       info.heterogeneous, info.fallback_domains);
		printf("lower=%s caps=%#x flags=%#x budget=%" PRIu64 " used=%" PRIu64 " entries=%u inodes=%u\n",
		       info.lower_fs, info.caps, info.flags, (uint64_t)info.index_budget,
		       (uint64_t)info.index_used, info.index_entries, info.index_inodes);
		return 0;
	}
	if (argc >= 4 && !strcmp(argv[2], "race")) {
		struct timespec t0, t1;
		unsigned long iters = 0, hits = 0, gone = 0, denied = 0;
		double secs = atof(argv[3]);
		clock_gettime(CLOCK_MONOTONIC, &t0);
		for (;;) {
			__u32 i;
			memset(&q, 0, sizeof q);
			q.nterms = 1; strcpy(q.terms[0].name, "kind"); q.terms[0].op = MOOB_OP_EQ;
			q.terms[0].type = MOOB_T_STRING; q.terms[0].len = 4; memcpy(q.terms[0].value, "note", 4);
			if (run_query(fd, &q, res, MOOB_QUERY_MAX_LIMIT)) { perror("MOOB_IOC_QUERY"); return 1; }
			CHECK(q.count <= MOOB_QUERY_MAX_LIMIT);
			for (i = 0; i < q.count; i++) {
				char full[8192]; int rfd;
				CHECK(memchr(res[i].path, 0, sizeof res[i].path) != NULL);
				if (res[i].flags & MOOB_RES_PATH_UNKNOWN) continue;	/* renamed directory: path unknown by contract */
				CHECK(res[i].path[0] != '/' && !strstr(res[i].path, ".."));
				snprintf(full, sizeof full, "%s/%s", argv[1], res[i].path);
				rfd = open(full, O_RDONLY);
				if (rfd >= 0) { hits++; close(rfd); continue; }
				if (errno == ENOENT) { gone++; continue; }	/* removed or renamed after the answer */
				if (errno == EACCES) { denied++; continue; }	/* chmod after the answer (TOCTOU contract) */
				fprintf(stderr, "race: open %s: %s\n", res[i].path, strerror(errno));
				return 1;
			}
			iters++;
			clock_gettime(CLOCK_MONOTONIC, &t1);
			if ((t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9 >= secs) break;
		}
		printf("race: %lu queries, %lu openable results, %lu vanished, %lu denied after answer\n", iters, hits, gone, denied);
		return iters ? 0 : 1;
	}
	if (!strcmp(argv[2], "invalid")) {
		int sub;
		memset(&q, 0, sizeof q);
		q.nterms = 1; strcpy(q.terms[0].name, "kind"); q.terms[0].op = MOOB_OP_EXISTS;
		CHECK(!run_query(fd, &q, res, 10));				/* baseline is valid */
		q.version = 99; errno = 0; CHECK(ioctl(fd, MOOB_IOC_QUERY, &q) == -1 && errno == EINVAL);
		q.version = MOOB_UAPI_VERSION; q.nterms = 5; errno = 0; CHECK(ioctl(fd, MOOB_IOC_QUERY, &q) == -1 && errno == EINVAL);
		q.nterms = 1; q.limit = 0; errno = 0; CHECK(ioctl(fd, MOOB_IOC_QUERY, &q) == -1 && errno == EINVAL);
		q.limit = 10; q.flags = 1; errno = 0; CHECK(ioctl(fd, MOOB_IOC_QUERY, &q) == -1 && errno == EINVAL);
		q.flags = 0; q.reserved[1] = 1; errno = 0; CHECK(ioctl(fd, MOOB_IOC_QUERY, &q) == -1 && errno == EINVAL);
		q.reserved[1] = 0; strcpy(q.terms[0].name, "Kind"); errno = 0; CHECK(ioctl(fd, MOOB_IOC_QUERY, &q) == -1 && errno == EINVAL);
		strcpy(q.terms[0].name, "kind"); q.terms[0].op = 42; errno = 0; CHECK(ioctl(fd, MOOB_IOC_QUERY, &q) == -1 && errno == EINVAL);
		q.terms[0].op = MOOB_OP_EQ; q.terms[0].type = MOOB_T_RAW; q.terms[0].len = 1; errno = 0; CHECK(ioctl(fd, MOOB_IOC_QUERY, &q) == -1 && errno == EINVAL);
		q.terms[0].type = MOOB_T_INT64; q.terms[0].len = 4; errno = 0; CHECK(ioctl(fd, MOOB_IOC_QUERY, &q) == -1 && errno == EINVAL);
		q.terms[0].len = 8; q.terms[0].op = MOOB_OP_PREFIX; errno = 0; CHECK(ioctl(fd, MOOB_IOC_QUERY, &q) == -1 && errno == EINVAL);
		q.terms[0].op = MOOB_OP_EQ; q.terms[0].type = MOOB_T_STRING; q.terms[0].len = 1; q.terms[0].value[0] = 0xff; errno = 0; CHECK(ioctl(fd, MOOB_IOC_QUERY, &q) == -1 && errno == EINVAL);
		q.terms[0].value[0] = 'x'; q.terms[0].pad[0] = 1; errno = 0; CHECK(ioctl(fd, MOOB_IOC_QUERY, &q) == -1 && errno == EINVAL);
		q.terms[0].pad[0] = 0; CHECK(!run_query(fd, &q, res, 10));
		errno = 0; CHECK(ioctl(fd, _IOR(MOOB_IOC_MAGIC, 9, int), &q) == -1 && errno == ENOTTY);
		/* the ioctls only answer on the mount root */
		{
			char sub_path[4096];
			snprintf(sub_path, sizeof sub_path, "%s/subdir", argv[1]);
			mkdir(sub_path, 0700);
			sub = open(sub_path, O_RDONLY | O_DIRECTORY); CHECK(sub >= 0);
			errno = 0; CHECK(ioctl(sub, MOOB_IOC_QUERY, &q) == -1 && errno == ENOTTY);
			close(sub); rmdir(sub_path);
		}
		puts("QUERY_INVALID_REQUESTS_REFUSED");
		return 0;
	}
	{
		int n;
		__u32 i;
		memset(&q, 0, sizeof q);
		n = parse_terms(argc, argv, 2, q.terms);
		if (n < 1) { fprintf(stderr, "query: invalid terms\n"); return 65; }
		q.nterms = (__u32)n;
		if (run_query(fd, &q, res, MOOB_QUERY_MAX_LIMIT)) { perror("MOOB_IOC_QUERY"); return 1; }
		qsort(res, q.count, sizeof *res, by_ino);
		for (i = 0; i < q.count; i++)
			printf("%" PRIu64 "\t%s%s\n", (uint64_t)res[i].ino, res[i].path,
			       (res[i].flags & MOOB_RES_PATH_UNKNOWN) ? "?" : "");
		if (q.truncated)
			fprintf(stderr, "query: results truncated\n");
	}
	return 0;
}
