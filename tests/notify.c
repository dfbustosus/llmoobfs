/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Which change notifications does a change made through the layer produce?
 *   notify fanotify <path> <seconds>   filesystem mark with FAN_REPORT_DFID_NAME on the
 *                                      filesystem holding <path> (root); prints the mark
 *                                      result and a count per event type with names
 *   notify inotify <dir> <seconds>     non-recursive inotify watch on <dir>
 * Output lines: "MARK ok|<errno text>", then "EVENT <type> <name>", then "TOTAL <n>".
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>
#include <sys/fanotify.h>
#include <sys/inotify.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

static const char *fan_type(unsigned long long m)
{
	if (m & FAN_ATTRIB) return "ATTRIB";
	if (m & FAN_CREATE) return "CREATE";
	if (m & FAN_DELETE) return "DELETE";
	if (m & FAN_MOVED_FROM) return "MOVED_FROM";
	if (m & FAN_MOVED_TO) return "MOVED_TO";
	if (m & FAN_Q_OVERFLOW) return "OVERFLOW";
	return "OTHER";
}

static int run_fanotify(const char *path, double secs)
{
	char buf[16384];
	int fd = fanotify_init(FAN_CLASS_NOTIF | FAN_REPORT_DFID_NAME, O_RDONLY), total = 0;
	double end;

	if (fd < 0) { printf("INIT %s\n", strerror(errno)); return 1; }
	if (fanotify_mark(fd, FAN_MARK_ADD | FAN_MARK_FILESYSTEM,
			  FAN_ATTRIB | FAN_CREATE | FAN_DELETE | FAN_MOVED_FROM | FAN_MOVED_TO | FAN_ONDIR,
			  AT_FDCWD, path)) {
		printf("MARK %s\n", strerror(errno));
		return 1;
	}
	printf("MARK ok\n"); fflush(stdout);
	end = now() + secs;
	while (now() < end) {
		struct pollfd p = { .fd = fd, .events = POLLIN };
		ssize_t n;
		struct fanotify_event_metadata *m;

		if (poll(&p, 1, 200) <= 0) continue;
		n = read(fd, buf, sizeof buf);
		for (m = (void *)buf; FAN_EVENT_OK(m, n); m = FAN_EVENT_NEXT(m, n)) {
			const char *name = "";
			struct fanotify_event_info_header *h = (void *)(m + 1);

			if ((char *)h < (char *)m + m->event_len && h->info_type == FAN_EVENT_INFO_TYPE_DFID_NAME) {
				struct fanotify_event_info_fid *fid = (void *)h;
				struct file_handle *fh = (void *)fid->handle;
				name = (char *)fh->f_handle + fh->handle_bytes;
			}
			printf("EVENT %s %s\n", fan_type(m->mask), name);
			total++;
		}
	}
	printf("TOTAL %d\n", total);
	return 0;
}

static int run_inotify(const char *dir, double secs)
{
	char buf[16384] __attribute__((aligned(8)));
	int fd = inotify_init1(IN_NONBLOCK), total = 0;
	double end;

	if (fd < 0 || inotify_add_watch(fd, dir, IN_ATTRIB | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO) < 0) {
		printf("MARK %s\n", strerror(errno));
		return 1;
	}
	printf("MARK ok\n"); fflush(stdout);
	end = now() + secs;
	while (now() < end) {
		struct pollfd p = { .fd = fd, .events = POLLIN };
		ssize_t n, off;

		if (poll(&p, 1, 200) <= 0) continue;
		n = read(fd, buf, sizeof buf);
		for (off = 0; n > 0 && off < n; ) {
			struct inotify_event *e = (void *)(buf + off);
			const char *t = e->mask & IN_ATTRIB ? "ATTRIB" : e->mask & IN_CREATE ? "CREATE" :
					e->mask & IN_DELETE ? "DELETE" : e->mask & IN_MOVED_FROM ? "MOVED_FROM" :
					e->mask & IN_MOVED_TO ? "MOVED_TO" : "OTHER";
			printf("EVENT %s %s\n", t, e->len ? e->name : "");
			total++;
			off += (ssize_t)(sizeof *e + e->len);
		}
	}
	printf("TOTAL %d\n", total);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 4 && !strcmp(argv[1], "fanotify")) return run_fanotify(argv[2], atof(argv[3]));
	if (argc == 4 && !strcmp(argv[1], "inotify")) return run_inotify(argv[2], atof(argv[3]));
	fprintf(stderr, "usage: notify fanotify|inotify <path> <seconds>\n");
	return 64;
}
