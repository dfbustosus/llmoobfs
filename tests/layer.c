/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Phase 2a acceptance test for the MoobFS memory layer: exercises the
 * pass-through operations on a mounted layer and checks that the lower
 * directory reflects every change. Usage: layer <mount> <lower>
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/statfs.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s (errno=%d %s)\n", __LINE__, #c, errno, strerror(errno)); return 1; } } while (0)

static char mnt[4096], low[4096];
/* Rotating buffers so two calls can appear in one expression. */
static const char *join(const char *base, const char *n)
{
	static char ring[8][8192]; static unsigned next;
	char *b = ring[next++ % 8];
	snprintf(b, sizeof ring[0], "%s/%s", base, n);
	return b;
}
static const char *up(const char *n) { return join(mnt, n); }
static const char *lo(const char *n) { return join(low, n); }

static int same_content(const char *a, const char *b)
{
	char x[65536], y[65536];
	int fa = open(a, O_RDONLY), fb = open(b, O_RDONLY);
	ssize_t na, nb;
	if (fa < 0 || fb < 0) return 0;
	na = read(fa, x, sizeof x); nb = read(fb, y, sizeof y);
	close(fa); close(fb);
	return na == nb && na >= 0 && !memcmp(x, y, (size_t)na);
}

static int count_entries(const char *dir)
{
	DIR *d = opendir(dir); struct dirent *e; int n = 0;
	if (!d) return -1;
	while ((e = readdir(d))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) n++;
	closedir(d); return n;
}

int main(int argc, char **argv)
{
	struct stat st, lst;
	struct statfs sfs;
	char buf[8192], *map;
	int fd, i;
	ssize_t n;

	CHECK(argc == 3);
	snprintf(mnt, sizeof mnt, "%s", argv[1]);
	snprintf(low, sizeof low, "%s", argv[2]);

	/* statfs passes through, with the layer's own magic */
	CHECK(!statfs(mnt, &sfs) && sfs.f_type == 0x4d4f4f42);

	/* create + write + read, visible in lower with the same bytes */
	fd = open(up("a.txt"), O_WRONLY | O_CREAT | O_EXCL, 0640); CHECK(fd >= 0);
	for (i = 0; i < (int)sizeof buf; i++) buf[i] = (char)('a' + i % 26);
	CHECK(write(fd, buf, sizeof buf) == (ssize_t)sizeof buf);
	CHECK(!fsync(fd));
	CHECK(!fstat(fd, &st) && st.st_size == (off_t)sizeof buf);
	close(fd);
	CHECK(!stat(lo("a.txt"), &lst) && lst.st_size == (off_t)sizeof buf);
	CHECK(!stat(up("a.txt"), &st) && st.st_ino == lst.st_ino && st.st_mode == lst.st_mode);
	CHECK(same_content(up("a.txt"), lo("a.txt")));
	errno = 0; CHECK(open(up("a.txt"), O_WRONLY | O_CREAT | O_EXCL, 0640) == -1 && errno == EEXIST);

	/* pread across a page boundary and EOF */
	fd = open(up("a.txt"), O_RDONLY); CHECK(fd >= 0);
	CHECK(pread(fd, buf, 100, 4050) == 100);
	for (i = 0; i < 100; i++) CHECK(buf[i] == (char)('a' + (4050 + i) % 26));
	CHECK(pread(fd, buf, 10, 8192) == 0);
	CHECK(lseek(fd, 8000, SEEK_SET) == 8000 && read(fd, buf, 1000) == 192);
	CHECK(lseek(fd, 0, SEEK_END) == 8192);
	close(fd);

	/* append, truncate, ftruncate */
	fd = open(up("a.txt"), O_WRONLY | O_APPEND); CHECK(fd >= 0);
	CHECK(write(fd, "tail", 4) == 4); close(fd);
	CHECK(!stat(lo("a.txt"), &lst) && lst.st_size == 8196);
	CHECK(!truncate(up("a.txt"), 100));
	CHECK(!stat(lo("a.txt"), &lst) && lst.st_size == 100);
	CHECK(!stat(up("a.txt"), &st) && st.st_size == 100);
	fd = open(up("a.txt"), O_RDWR); CHECK(fd >= 0);
	CHECK(!ftruncate(fd, 12000));
	CHECK(!fstat(fd, &st) && st.st_size == 12000);
	CHECK(pread(fd, buf, 16, 11990) == 10);
	for (i = 0; i < 10; i++) CHECK(buf[i] == 0);
	close(fd);

	/* shared writable mmap reaches the lower file; private mmap reads */
	fd = open(up("a.txt"), O_RDWR); CHECK(fd >= 0);
	map = mmap(NULL, 12000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	CHECK(map != MAP_FAILED);
	memcpy(map + 5000, "MMAPPED", 7);
	CHECK(!msync(map, 12000, MS_SYNC));
	CHECK(!munmap(map, 12000));
	CHECK(pread(fd, buf, 7, 5000) == 7 && !memcmp(buf, "MMAPPED", 7));
	close(fd);
	fd = open(lo("a.txt"), O_RDONLY); CHECK(fd >= 0);
	CHECK(pread(fd, buf, 7, 5000) == 7 && !memcmp(buf, "MMAPPED", 7));
	close(fd);
	fd = open(up("a.txt"), O_RDONLY); CHECK(fd >= 0);
	map = mmap(NULL, 12000, PROT_READ, MAP_PRIVATE, fd, 0);
	CHECK(map != MAP_FAILED && !memcmp(map + 5000, "MMAPPED", 7));
	CHECK(!munmap(map, 12000)); close(fd);

	/* directories: mkdir, nested create, readdir, rmdir ordering */
	CHECK(!mkdir(up("d"), 0750));
	CHECK(!stat(lo("d"), &lst) && S_ISDIR(lst.st_mode));
	fd = open(up("d/inner"), O_WRONLY | O_CREAT, 0600); CHECK(fd >= 0);
	CHECK(write(fd, "x", 1) == 1); close(fd);
	CHECK(count_entries(up("d")) == 1 && count_entries(lo("d")) == 1);
	CHECK(count_entries(up("")) == count_entries(lo("")));
	errno = 0; CHECK(rmdir(up("d")) == -1 && errno == ENOTEMPTY);
	CHECK(!unlink(up("d/inner")));
	errno = 0; CHECK(stat(lo("d/inner"), &lst) == -1 && errno == ENOENT);
	CHECK(!rmdir(up("d")));
	errno = 0; CHECK(stat(lo("d"), &lst) == -1 && errno == ENOENT);
	errno = 0; CHECK(stat(up("d"), &st) == -1 && errno == ENOENT);

	/* chmod and utimes pass through */
	CHECK(!chmod(up("a.txt"), 0600));
	CHECK(!stat(lo("a.txt"), &lst) && (lst.st_mode & 07777) == 0600);

	/* unsupported in phase 2a: links and renames are refused, not faked */
	errno = 0; CHECK(link(up("a.txt"), up("b.txt")) == -1 && errno == EPERM);
	errno = 0; CHECK(symlink("a.txt", up("s")) == -1 && errno == EPERM);
	errno = 0; CHECK(rename(up("a.txt"), up("c.txt")) == -1 && errno == EPERM);
	errno = 0; CHECK(stat(lo("a.txt"), &lst) == 0);

	/* unlink removes from lower; open descriptor keeps reading */
	fd = open(up("a.txt"), O_RDONLY); CHECK(fd >= 0);
	CHECK(!unlink(up("a.txt")));
	errno = 0; CHECK(stat(lo("a.txt"), &lst) == -1 && errno == ENOENT);
	n = pread(fd, buf, 7, 5000); CHECK(n == 7 && !memcmp(buf, "MMAPPED", 7));
	close(fd);
	CHECK(count_entries(up("")) == count_entries(lo("")));

	puts("LAYER_PASSTHROUGH_TESTS_PASS");
	return 0;
}
