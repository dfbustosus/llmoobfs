/* Minimal stand-in for xfstests src/global.h, enough to build ltp/fsx.c standalone. */
#ifndef GLOBAL_H
#define GLOBAL_H
#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <malloc.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/param.h>
#include <libgen.h>
#include <assert.h>
#include <linux/fs.h>
#include <linux/falloc.h>
#define HAVE_ERR_H 1
#define HAVE_LINUX_FALLOC_H 1
#define HAVE_COPY_FILE_RANGE 1
#define HAVE_STATX 1
#endif
#include <getopt.h>
#ifndef roundup_64
#define roundup_64(x, y) ((((unsigned long long)(x) + ((y) - 1)) / (y)) * (y))
#define rounddown_64(x, y) (((unsigned long long)(x) / (y)) * (y))
#endif
#ifndef FALLOC_FL_WRITE_ZEROES
#define FALLOC_FL_WRITE_ZEROES 0x80
#endif
