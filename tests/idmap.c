/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Mount-contract checks for the MoobFS layer (spec-m3 §3.1):
 *   idmap <mount>          : mount_setattr with MOUNT_ATTR_IDMAP must fail with EINVAL
 *                            (the filesystem does not declare FS_ALLOW_IDMAP)
 *   userns <lower> <dir>   : mounting moobfs_layer from a non-initial user namespace must
 *                            fail with EPERM (no FS_USERNS_MOUNT)
 * Run as root.
 */
#define _GNU_SOURCE
#include <sched.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <stdint.h>

#ifndef SYS_open_tree
#define SYS_open_tree 428
#endif
#ifndef SYS_mount_setattr
#define SYS_mount_setattr 442
#endif
#ifndef OPEN_TREE_CLONE
#define OPEN_TREE_CLONE 1
#endif
#ifndef MOUNT_ATTR_IDMAP
#define MOUNT_ATTR_IDMAP 0x00100000
#endif
struct my_mount_attr { uint64_t attr_set, attr_clr, propagation, userns_fd; };

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s (errno=%d %s)\n", __LINE__, #c, errno, strerror(errno)); return 1; } } while (0)

static int write_file(const char *path, const char *text)
{
	int fd = open(path, O_WRONLY);
	if (fd < 0) return -1;
	if (write(fd, text, strlen(text)) < 0) { close(fd); return -1; }
	close(fd); return 0;
}

/* Child: new user namespace mapping root inside to uid 1001 outside; waits for a byte. */
static pid_t spawn_userns_child(int *to_child, int *from_child, const char *lower, const char *dir)
{
	int p1[2], p2[2];
	pid_t pid;
	if (pipe(p1) || pipe(p2)) return -1;
	pid = fork();
	if (pid < 0) return -1;
	if (!pid) {
		char c, opt[4096];
		close(p1[1]); close(p2[0]);
		if (unshare(CLONE_NEWUSER | CLONE_NEWNS)) _exit(10);
		if (read(p1[0], &c, 1) != 1) _exit(11);	/* parent wrote the maps */
		if (c == 'q') _exit(0);			/* idmap test: only needed as a namespace */
		snprintf(opt, sizeof opt, "lower=%s", lower);
		errno = 0;
		if (mount("none", dir, "moobfs_layer", 0, opt) == 0) { umount(dir); _exit(20); }
		if (errno != EPERM) _exit(30 + (errno & 0xff) % 70);
		_exit(0);
	}
	close(p1[0]); close(p2[1]);
	{
		char path[64];
		snprintf(path, sizeof path, "/proc/%d/setgroups", pid); write_file(path, "deny");
		snprintf(path, sizeof path, "/proc/%d/uid_map", pid); if (write_file(path, "0 1001 1")) return -1;
		snprintf(path, sizeof path, "/proc/%d/gid_map", pid); if (write_file(path, "0 1001 1")) return -1;
	}
	*to_child = p1[1]; *from_child = p2[0];
	return pid;
}

int main(int argc, char **argv)
{
	if (argc >= 3 && !strcmp(argv[1], "idmap")) {
		int to, from, status, tree, nsfd;
		char nspath[64];
		struct my_mount_attr attr = { .attr_set = MOUNT_ATTR_IDMAP };
		pid_t pid = spawn_userns_child(&to, &from, "", "");
		CHECK(pid > 0);
		snprintf(nspath, sizeof nspath, "/proc/%d/ns/user", pid);
		nsfd = open(nspath, O_RDONLY); CHECK(nsfd >= 0);
		tree = (int)syscall(SYS_open_tree, AT_FDCWD, argv[2], OPEN_TREE_CLONE); CHECK(tree >= 0);
		attr.userns_fd = (uint64_t)nsfd;
		errno = 0;
		CHECK(syscall(SYS_mount_setattr, tree, "", AT_EMPTY_PATH, &attr, sizeof attr) == -1);
		CHECK(errno == EINVAL);
		close(tree); close(nsfd);
		CHECK(write(to, "q", 1) == 1 && waitpid(pid, &status, 0) == pid);
		puts("IDMAP_REFUSED_EINVAL");
		return 0;
	}
	if (argc >= 4 && !strcmp(argv[1], "userns")) {
		int to, from, status;
		pid_t pid = spawn_userns_child(&to, &from, argv[2], argv[3]);
		CHECK(pid > 0);
		CHECK(write(to, "m", 1) == 1 && waitpid(pid, &status, 0) == pid);
		if (!WIFEXITED(status) || WEXITSTATUS(status)) {
			fprintf(stderr, "child exit %d (20 = mount unexpectedly succeeded, 30+e = other errno)\n", WEXITSTATUS(status));
			return 1;
		}
		puts("USERNS_MOUNT_REFUSED_EPERM");
		return 0;
	}
	return 64;
}
