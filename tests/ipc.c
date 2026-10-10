/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Inter-process primitives a WAL-mode SQLite relies on, exercised on one
 * directory: POSIX fcntl locks between two processes and shared writable mmap
 * coherence between two processes. Usage: ipc <dir>
 */
#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s (errno=%d %s)\n", __LINE__, #c, errno, strerror(errno)); _exit(1); } } while (0)

static int lock(int fd, short type, int cmd, off_t start, off_t len)
{
	struct flock fl = { .l_type = type, .l_whence = SEEK_SET, .l_start = start, .l_len = len };
	return fcntl(fd, cmd, &fl);
}

int main(int argc, char **argv)
{
	char path[4096], shmpath[4096];
	int fd, shm, status, pipefd[2], ack[2];
	volatile uint32_t *map;
	pid_t child;
	char c;

	if (argc != 2) return 64;
	snprintf(path, sizeof path, "%s/ipc.lock", argv[1]);
	snprintf(shmpath, sizeof shmpath, "%s/ipc.shm", argv[1]);
	unlink(path); unlink(shmpath);
	fd = open(path, O_RDWR | O_CREAT, 0600); CHECK(fd >= 0);
	CHECK(!ftruncate(fd, 4096));
	shm = open(shmpath, O_RDWR | O_CREAT, 0600); CHECK(shm >= 0);
	CHECK(!ftruncate(shm, 32768));
	map = mmap(NULL, 32768, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0); CHECK(map != MAP_FAILED);
	map[0] = 1;
	CHECK(!pipe(pipefd) && !pipe(ack));

	/* parent holds a write lock on byte 120 (the SQLite DMS byte) before the child runs */
	CHECK(!lock(fd, F_WRLCK, F_SETLK, 120, 1));
	child = fork(); CHECK(child >= 0);
	if (!child) {
		int cfd = open(path, O_RDWR), cshm = open(shmpath, O_RDWR);
		volatile uint32_t *cmap;
		struct flock probe = { .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 120, .l_len = 1 };
		CHECK(cfd >= 0 && cshm >= 0);
		/* 1. a conflicting lock must be refused and F_GETLK must name the parent */
		errno = 0; CHECK(lock(cfd, F_WRLCK, F_SETLK, 120, 1) == -1 && (errno == EAGAIN || errno == EACCES));
		CHECK(!fcntl(cfd, F_GETLK, &probe) && probe.l_type == F_WRLCK && probe.l_pid == getppid());
		/* 2. a non-overlapping lock is fine */
		CHECK(!lock(cfd, F_WRLCK, F_SETLK, 121, 1));
		/* 3. shared mapping: the child sees the parent's value and the parent must see the child's */
		cmap = mmap(NULL, 32768, PROT_READ | PROT_WRITE, MAP_SHARED, cshm, 0); CHECK(cmap != MAP_FAILED);
		CHECK(cmap[0] == 1);
		cmap[1] = 0xC0FFEE;
		CHECK(write(ack[1], "a", 1) == 1);
		/* 4. wait until the parent releases, then F_SETLKW must succeed */
		CHECK(read(pipefd[0], &c, 1) == 1);
		CHECK(!lock(cfd, F_WRLCK, F_SETLKW, 120, 1));
		cmap[2] = 7;
		CHECK(write(ack[1], "b", 1) == 1);
		/* 5. parent's later write is visible here */
		CHECK(read(pipefd[0], &c, 1) == 1);
		CHECK(cmap[3] == 0xBEEF);
		_exit(0);
	}
	CHECK(read(ack[0], &c, 1) == 1 && c == 'a');
	CHECK(map[1] == 0xC0FFEE);
	CHECK(!lock(fd, F_UNLCK, F_SETLK, 120, 1));
	CHECK(write(pipefd[1], "x", 1) == 1);
	CHECK(read(ack[0], &c, 1) == 1 && c == 'b');
	CHECK(map[2] == 7);
	/* now the child holds byte 120: the parent must be refused */
	errno = 0; CHECK(lock(fd, F_WRLCK, F_SETLK, 120, 1) == -1 && (errno == EAGAIN || errno == EACCES));
	map[3] = 0xBEEF;
	CHECK(write(pipefd[1], "y", 1) == 1);
	CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
	/* after the child exits its locks are gone */
	CHECK(!lock(fd, F_WRLCK, F_SETLK, 120, 1));
	/* truncate + remap: a second mapping of the same file after ftruncate sees fresh data */
	CHECK(!munmap((void *)map, 32768));
	CHECK(!ftruncate(shm, 0) && !ftruncate(shm, 32768));
	map = mmap(NULL, 32768, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0); CHECK(map != MAP_FAILED);
	CHECK(map[0] == 0 && map[3] == 0);
	close(fd); close(shm);
	unlink(path); unlink(shmpath);
	puts("IPC_PRIMITIVES_PASS");
	return 0;
}
