/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "moobfs_format.h"

struct input { char name[64]; m_u8 *bytes; size_t size; unsigned object; };

static int load(const char *path, m_u8 **data, size_t *len)
{
	struct stat st;
	int fd=open(path,O_RDONLY);
	size_t done=0;
	if(fd<0) return -1;
	if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size<0 || st.st_size>MOOB_MAX_DATA) {
		close(fd); errno=EINVAL; return -1;
	}
	*len=(size_t)st.st_size; *data=malloc(*len?*len:1);
	if(!*data) { close(fd); return -1; }
	while(done<*len) {
		ssize_t n=read(fd,*data+done,*len-done);
		if(n<0 && errno==EINTR) continue;
		if(n<=0) { close(fd); free(*data); *data=NULL; errno=EIO; return -1; }
		done+=(size_t)n;
	}
	close(fd); return 0;
}

int main(int argc, char **argv)
{
	struct input *in=NULL;
	unsigned unique[MOOB_MAX_FILES],count,objects=0,i,j;
	m_u64 object_bytes,file_bytes,data_start,total,cursor;
	m_u8 *image=NULL;
	int fd=-1,result=1,created=0;
	if(argc<3 || argc>2+(int)MOOB_MAX_FILES) {
		fprintf(stderr,"Usage: %s NEW_IMAGE NAME=SOURCE [NAME=SOURCE ...]\n",argv[0]); return 2;
	}
	count=(unsigned)argc-2;
	in=calloc(count,sizeof(*in));
	if(!in) goto out;
	for(i=0;i<count;i++) {
		const char *arg=argv[i+2], *eq=strchr(arg,'=');
		size_t len=eq?(size_t)(eq-arg):0;
		if(!moob_valid_name(arg,len)) { fprintf(stderr,"Invalid name mapping (printable ASCII, 1..63 bytes)\n"); goto out; }
		memcpy(in[i].name,arg,len);
		for(j=0;j<i;j++) if(!strcmp(in[i].name,in[j].name)) { fprintf(stderr,"Duplicate name\n"); goto out; }
		if(load(eq+1,&in[i].bytes,&in[i].size)) { perror(eq+1); goto out; }
		for(j=0;j<objects;j++) {
			struct input *old=&in[unique[j]];
			if(old->size==in[i].size && !memcmp(old->bytes,in[i].bytes,old->size)) break;
		}
		if(j==objects) unique[objects++]=i;
		in[i].object=j;
	}
	object_bytes=moob_align(objects*MOOB_OBJECT_REC);
	file_bytes=moob_align(count*MOOB_FILE_REC);
	data_start=MOOB_BLOCK+object_bytes+file_bytes; total=data_start;
	for(i=0;i<objects;i++) total+=moob_align(in[unique[i]].size?in[unique[i]].size:1);
	image=calloc(1,(size_t)total);
	if(!image) goto out;
	memcpy(image,MOOB_MAGIC,8); moob_put32(image+8,MOOB_VERSION); moob_put32(image+12,MOOB_BLOCK);
	moob_put32(image+16,count); moob_put32(image+20,objects);
	moob_put64(image+24,MOOB_BLOCK); moob_put64(image+32,MOOB_BLOCK+object_bytes);
	moob_put64(image+40,data_start); moob_put64(image+48,total);
	cursor=data_start;
	for(i=0;i<objects;i++) {
		struct input *src=&in[unique[i]];
		struct moob_sha256 hash;
		m_u8 *rec=image+MOOB_BLOCK+i*MOOB_OBJECT_REC;
		moob_put64(rec,i+1); moob_put64(rec+8,cursor); moob_put64(rec+16,src->size);
		moob_sha_init(&hash); moob_sha_update(&hash,src->bytes,src->size); moob_sha_final(&hash,rec+24);
		memcpy(image+cursor,src->bytes,src->size);
		cursor+=moob_align(src->size?src->size:1);
	}
	for(i=0;i<count;i++) {
		m_u8 *rec=image+MOOB_BLOCK+object_bytes+i*MOOB_FILE_REC;
		moob_put64(rec,i+2); moob_put64(rec+8,in[i].object+1);
		moob_put32(rec+16,(m_u32)strlen(in[i].name)); memcpy(rec+24,in[i].name,strlen(in[i].name));
	}
	moob_put32(image+MOOB_OBJECT_CRC,~moob_crc32c(~0U,image+MOOB_BLOCK,(size_t)object_bytes));
	moob_put32(image+MOOB_FILE_CRC,~moob_crc32c(~0U,image+MOOB_BLOCK+object_bytes,(size_t)file_bytes));
	moob_put32(image+MOOB_SUPER_CRC,~moob_crc32c(~0U,image,MOOB_BLOCK));
	fd=open(argv[1],O_WRONLY|O_CREAT|O_EXCL,0600);
	if(fd<0) { perror(argv[1]); goto out; }
	created=1; cursor=0;
	while(cursor<total) {
		ssize_t n=write(fd,image+cursor,(size_t)(total-cursor));
		if(n<0 && errno==EINTR) continue;
		if(n<=0) { perror("write"); goto out; }
		cursor+=(m_u64)n;
	}
	if(fsync(fd)) { perror("fsync"); goto out; }
	if(close(fd)) { fd=-1; perror("close"); goto out; }
	fd=-1; result=0;
	printf("Created %s: %u files, %u objects, %llu bytes\n",argv[1],count,objects,(unsigned long long)total);
out:
	if(fd>=0) close(fd);
	if(result && created) unlink(argv[1]);
	if(in) { for(i=0;i<count;i++) free(in[i].bytes); free(in); }
	free(image); return result;
}
