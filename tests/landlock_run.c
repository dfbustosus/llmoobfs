/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Run a command confined by Landlock: directory listing allowed beneath <list-root>, file
 * reads allowed only beneath <read-dir> and beneath the directory of <command> (execve needs
 * read access to the binary under Landlock). Used to check whether QUERY results respect a
 * Landlock policy (path-based, enforced at open time).
 *   landlock_run <list-root> <read-dir> <command> [args...]
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <libgen.h>
#include <limits.h>
#include <sys/syscall.h>
#include <linux/landlock.h>

static int rule(int ruleset, const char *path, __u64 access)
{
	struct landlock_path_beneath_attr pb = { .allowed_access = access };
	int r;

	pb.parent_fd = open(path, O_PATH | O_CLOEXEC);
	if (pb.parent_fd < 0) return -1;
	r = (int)syscall(SYS_landlock_add_rule, ruleset, LANDLOCK_RULE_PATH_BENEATH, &pb, 0);
	close(pb.parent_fd);
	return r;
}

int main(int argc, char **argv)
{
	struct landlock_ruleset_attr ra = { .handled_access_fs = LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR };
	int abi, rs;

	if (argc < 4) { fprintf(stderr, "usage: landlock_run <list-root> <read-dir> <command> [args...]\n"); return 64; }
	abi = (int)syscall(SYS_landlock_create_ruleset, NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
	if (abi < 1) { perror("landlock unavailable"); return 2; }
	rs = (int)syscall(SYS_landlock_create_ruleset, &ra, sizeof ra, 0);
	if (rs < 0) { perror("create_ruleset"); return 2; }
	{
		char exe[PATH_MAX];

		if (!realpath(argv[3], exe)) { perror(argv[3]); return 2; }
		if (rule(rs, dirname(exe), LANDLOCK_ACCESS_FS_READ_FILE)) { perror("add_rule exe"); return 2; }
	}
	if (rule(rs, argv[1], LANDLOCK_ACCESS_FS_READ_DIR) || rule(rs, argv[2], LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR)) {
		perror("add_rule"); return 2;
	}
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) || syscall(SYS_landlock_restrict_self, rs, 0)) { perror("restrict_self"); return 2; }
	fprintf(stderr, "landlock ABI %d active: list beneath %s, read files beneath %s\n", abi, argv[1], argv[2]);
	execvp(argv[3], argv + 3);
	perror("exec");
	return 127;
}
