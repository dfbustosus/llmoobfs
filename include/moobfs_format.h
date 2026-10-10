/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef MOOBFS_FORMAT_H
#define MOOBFS_FORMAT_H
#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/string.h>
typedef u8 m_u8;
typedef u32 m_u32;
typedef u64 m_u64;
#else
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint8_t m_u8;
typedef uint32_t m_u32;
typedef uint64_t m_u64;
#endif

#define MOOB_MAGIC "MOOBFSR1"
#define MOOB_VERSION 1
#define MOOB_BLOCK 4096U
#define MOOB_MAX_FILES 128U
#define MOOB_MAX_DATA 65536U
#define MOOB_NAME_MAX 63U
#define MOOB_OBJECT_REC 64U
#define MOOB_FILE_REC 128U
#define MOOB_SUPER_CRC 56U
#define MOOB_OBJECT_CRC 60U
#define MOOB_FILE_CRC 64U
#define MOOB_STAT_MAGIC 0x4d4f4f42

struct moob_object { m_u64 id, offset, size; m_u8 sha256[32]; m_u32 references; };
struct moob_file { m_u64 ino; m_u32 object_index; char name[64]; };
struct moob_image {
	m_u32 file_count, object_count;
	m_u64 image_size, object_table, file_table, data_start;
	struct moob_object objects[MOOB_MAX_FILES];
	struct moob_file files[MOOB_MAX_FILES];
};
struct moob_sha256 { m_u32 h[8]; m_u64 bytes; m_u8 block[64]; size_t used; };
typedef int (*moob_reader)(void *, m_u64, void *, size_t);

static inline m_u32 moob_get32(const m_u8 *p)
{ return (m_u32)p[0] | (m_u32)p[1]<<8 | (m_u32)p[2]<<16 | (m_u32)p[3]<<24; }
static inline m_u64 moob_get64(const m_u8 *p)
{ return moob_get32(p) | (m_u64)moob_get32(p+4)<<32; }
static inline void moob_put32(m_u8 *p, m_u32 v)
{ unsigned i; for (i=0;i<4;i++) p[i]=(m_u8)(v>>(8*i)); }
static inline void moob_put64(m_u8 *p, m_u64 v)
{ unsigned i; for (i=0;i<8;i++) p[i]=(m_u8)(v>>(8*i)); }
static inline m_u64 moob_align(m_u64 n)
{ return (n + MOOB_BLOCK - 1) & ~((m_u64)MOOB_BLOCK - 1); }
static inline int moob_valid_name(const char *name, size_t len)
{
	size_t i;
	if (!len || len > MOOB_NAME_MAX || (len == 1 && name[0] == '.') ||
	    (len == 2 && name[0] == '.' && name[1] == '.')) return 0;
	for (i = 0; i < len; i++)
		if ((m_u8)name[i] < 32 || (m_u8)name[i] > 126 || name[i] == '/') return 0;
	return 1;
}

m_u32 moob_crc32c(m_u32 crc, const void *data, size_t len);
void moob_sha_init(struct moob_sha256 *ctx);
void moob_sha_update(struct moob_sha256 *ctx, const void *data, size_t len);
void moob_sha_final(struct moob_sha256 *ctx, m_u8 out[32]);
/* Caller supplies a zeroed image and a 4 KiB scratch buffer. -1 invalid, -2 I/O. */
int moob_parse(struct moob_image *im, m_u64 device_size, moob_reader read,
	       void *opaque, m_u8 *scratch, const char **why);
#endif
