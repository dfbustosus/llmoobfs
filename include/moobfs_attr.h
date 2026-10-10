/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Typed attribute encoding shared by the kernel layer and the user-space tools.
 * Phase 2c of docs/spec-m2.md. Pure functions, no allocation, no I/O.
 *
 * An attribute lives in the lower filesystem as the extended attribute
 * "user.moob.<name>" whose value is one type byte followed by the payload.
 */
#ifndef MOOBFS_ATTR_H
#define MOOBFS_ATTR_H
#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/string.h>
typedef u8 ma_u8;
typedef u64 ma_u64;
#else
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint8_t ma_u8;
typedef uint64_t ma_u64;
#endif

#define MOOB_ATTR_PREFIX "user.moob."
#define MOOB_ATTR_PREFIX_LEN 10U
#define MOOB_ATTR_NAME_MAX 64U
#define MOOB_ATTR_STRING_MAX 1024U
#define MOOB_ATTR_RAW_MAX 4096U
#define MOOB_ATTR_VALUE_MAX (1U + MOOB_ATTR_RAW_MAX)
#define MOOB_ATTR_MAX_PER_FILE 64U
#define MOOB_ATTR_FORMAT_NAME "format"	/* user.moob.format on the lower root */
#define MOOB_ATTR_FORMAT_VERSION 1U

enum moob_attr_type {
	MOOB_T_STRING = 1,	/* valid UTF-8, no NUL, up to MOOB_ATTR_STRING_MAX bytes */
	MOOB_T_INT64 = 2,	/* 8 bytes, little-endian two's complement */
	MOOB_T_DOUBLE = 3,	/* 8 bytes, IEEE 754 binary64 little-endian, not NaN */
	MOOB_T_BOOL = 4,	/* 1 byte, 0 or 1 */
	MOOB_T_TIME = 5,	/* 8 bytes, little-endian signed nanoseconds since the epoch */
	MOOB_T_RAW = 6,		/* up to MOOB_ATTR_RAW_MAX opaque bytes, not indexed */
};

enum moob_attr_error {
	MOOB_ATTR_OK = 0,
	MOOB_ATTR_EBADNAME,	/* name outside [a-z0-9_.], empty, too long, or edge dots */
	MOOB_ATTR_ERESERVED,	/* name reserved for the layer itself */
	MOOB_ATTR_EEMPTY,	/* value has no type byte */
	MOOB_ATTR_ETYPE,	/* unknown type byte */
	MOOB_ATTR_ESIZE,	/* payload size does not match the type */
	MOOB_ATTR_EUTF8,	/* string payload is not valid UTF-8 or contains NUL */
	MOOB_ATTR_EVALUE,	/* payload bytes are not a valid value for the type */
};

/* Name checks operate on the part after "user.moob.". */
int moob_attr_name_ok(const char *name, size_t len);
int moob_attr_name_reserved(const char *name, size_t len);
/* Returns MOOB_ATTR_OK or an error code; never reads past value + size. */
int moob_attr_validate(const ma_u8 *value, size_t size);
int moob_utf8_valid(const ma_u8 *p, size_t n);
const char *moob_attr_strerror(int err);
const char *moob_attr_type_name(int type);
/* Parses the decimal version stored in user.moob.format; returns 0 on bad input. */
unsigned int moob_attr_parse_format(const ma_u8 *value, size_t size);

static inline ma_u64 moob_attr_get64(const ma_u8 *p)
{
	ma_u64 v = 0;
	int i;

	for (i = 7; i >= 0; i--)
		v = (v << 8) | p[i];
	return v;
}

static inline void moob_attr_put64(ma_u8 *p, ma_u64 v)
{
	int i;

	for (i = 0; i < 8; i++) {
		p[i] = (ma_u8)(v & 0xff);
		v >>= 8;
	}
}

#endif
