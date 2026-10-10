/* SPDX-License-Identifier: GPL-2.0-only */
#include "moobfs_format.h"

static int zeroes(const m_u8 *p, size_t n)
{ while(n--) if(*p++) return 0; return 1; }

int moob_parse(struct moob_image *im, m_u64 device_size, moob_reader read,
	       void *opaque, m_u8 *buf, const char **why)
{
	m_u32 i,j,crc,expected,object_crc,file_crc;
	m_u64 object_bytes,file_bytes,cursor,off,n,oid;
	m_u8 rec[MOOB_FILE_REC],digest[32];
	struct moob_sha256 hash;
#define BAD(s) do { *why=(s); return -1; } while(0)
#define READ(o,p,l) do { if(read(opaque,(o),(p),(l))) { *why="read error"; return -2; } } while(0)
	if(device_size<MOOB_BLOCK) BAD("truncated superblock");
	READ(0,buf,MOOB_BLOCK);
	if(memcmp(buf,MOOB_MAGIC,8)) BAD("bad magic");
	if(moob_get32(buf+8)!=MOOB_VERSION || moob_get32(buf+12)!=MOOB_BLOCK) BAD("unsupported format");
	expected=moob_get32(buf+MOOB_SUPER_CRC); moob_put32(buf+MOOB_SUPER_CRC,0);
	if(~moob_crc32c(~0U,buf,MOOB_BLOCK)!=expected) BAD("superblock checksum");
	if(!zeroes(buf+68,MOOB_BLOCK-68)) BAD("reserved superblock bytes");
	im->file_count=moob_get32(buf+16); im->object_count=moob_get32(buf+20);
	if(!im->file_count || im->file_count>MOOB_MAX_FILES || !im->object_count || im->object_count>im->file_count) BAD("invalid counts");
	im->object_table=moob_get64(buf+24); im->file_table=moob_get64(buf+32);
	im->data_start=moob_get64(buf+40); im->image_size=moob_get64(buf+48);
	object_crc=moob_get32(buf+MOOB_OBJECT_CRC); file_crc=moob_get32(buf+MOOB_FILE_CRC);
	object_bytes=moob_align(im->object_count*MOOB_OBJECT_REC);
	file_bytes=moob_align(im->file_count*MOOB_FILE_REC);
	if(im->object_table!=MOOB_BLOCK || im->file_table!=MOOB_BLOCK+object_bytes || im->data_start!=im->file_table+file_bytes) BAD("invalid table layout");
	if(im->image_size!=device_size || im->image_size%MOOB_BLOCK || im->image_size<im->data_start) BAD("invalid image size");
	for(j=0;j<2;j++) {
		m_u64 start=j?im->file_table:im->object_table;
		m_u64 bytes=j?file_bytes:object_bytes;
		m_u64 used=j?im->file_count*MOOB_FILE_REC:im->object_count*MOOB_OBJECT_REC;
		crc=~0U;
		for(off=0;off<bytes;off+=MOOB_BLOCK) {
			READ(start+off,buf,MOOB_BLOCK);
			crc=moob_crc32c(crc,buf,MOOB_BLOCK);
			if(off+MOOB_BLOCK>used && !zeroes(buf+(used>off?used-off:0),MOOB_BLOCK-(used>off?used-off:0))) BAD("table padding");
		}
		if(~crc!=(j?file_crc:object_crc)) BAD("table checksum");
	}
	cursor=im->data_start;
	for(i=0;i<im->object_count;i++) {
		struct moob_object *o=&im->objects[i];
		READ(im->object_table+i*MOOB_OBJECT_REC,rec,MOOB_OBJECT_REC);
		o->id=moob_get64(rec); o->offset=moob_get64(rec+8); o->size=moob_get64(rec+16);
		memcpy(o->sha256,rec+24,32);
		if(o->id!=(m_u64)i+1 || o->size>MOOB_MAX_DATA || o->offset!=cursor || !zeroes(rec+56,8)) BAD("invalid object record");
		n=moob_align(o->size?o->size:1);
		if(cursor>im->image_size || n>im->image_size-cursor) BAD("object outside image");
		moob_sha_init(&hash);
		for(off=0;off<n;off+=MOOB_BLOCK) {
			size_t take=off<o->size?(size_t)(o->size-off):0;
			if(take>MOOB_BLOCK) take=MOOB_BLOCK;
			READ(cursor+off,buf,MOOB_BLOCK);
			moob_sha_update(&hash,buf,take);
			if(!zeroes(buf+take,MOOB_BLOCK-take)) BAD("payload padding");
		}
		moob_sha_final(&hash,digest);
		if(memcmp(digest,o->sha256,32)) BAD("payload checksum");
		cursor+=n;
	}
	if(cursor!=im->image_size) BAD("unexpected trailing data");
	for(i=0;i<im->file_count;i++) {
		struct moob_file *f=&im->files[i];
		unsigned len;
		READ(im->file_table+i*MOOB_FILE_REC,rec,MOOB_FILE_REC);
		f->ino=moob_get64(rec); oid=moob_get64(rec+8); len=moob_get32(rec+16);
		if(f->ino!=(m_u64)i+2 || !oid || oid>im->object_count || !len || len>MOOB_NAME_MAX) BAD("invalid file record");
		if(!zeroes(rec+20,4) || !zeroes(rec+24+len,MOOB_FILE_REC-24-len)) BAD("reserved file bytes");
		memcpy(f->name,rec+24,len); f->name[len]=0; f->object_index=(m_u32)oid-1;
		im->objects[f->object_index].references++;
		if(!moob_valid_name(f->name,len)) BAD("invalid filename");
		for(j=0;j<i;j++) if(!strcmp(f->name,im->files[j].name)) BAD("duplicate filename");
	}
	for(i=0;i<im->object_count;i++) {
		for(j=0;j<im->file_count;j++) if(im->files[j].object_index==i) break;
		if(j==im->file_count) BAD("unreferenced object");
	}
	*why="ok";
	return 0;
#undef BAD
#undef READ
}
