/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "moobfs_format.h"

static int read_at(void *opaque, m_u64 off, void *out, size_t size)
{
	int fd=*(int *)opaque;
	m_u8 *p=out;
	while(size) {
		ssize_t n=pread(fd,p,size,(off_t)off);
		if(n<0 && errno==EINTR) continue;
		if(n<=0) return -1;
		p+=n; off+=(m_u64)n; size-=(size_t)n;
	}
	return 0;
}
static void json_string(const char *s)
{
	const unsigned char *p=(const unsigned char *)s;
	putchar('"');
	for(;*p;p++) {
		if(*p=='"' || *p=='\\') { putchar('\\'); putchar(*p); }
		else if(*p<32 || *p>=127) printf("\\u%04x",*p);
		else putchar(*p);
	}
	putchar('"');
}
int main(int argc,char **argv)
{
	struct moob_image *im;
	m_u8 *scratch;
	struct stat st;
	const char *why="allocation";
	int fd,rc=1;
	unsigned i,j;
	if(argc!=2) { fprintf(stderr,"Usage: %s IMAGE\n",argv[0]); return 2; }
	fd=open(argv[1],O_RDONLY);
	if(fd<0) { perror(argv[1]); return 1; }
	im=calloc(1,sizeof(*im)); scratch=malloc(MOOB_BLOCK);
	if(fstat(fd,&st) || !S_ISREG(st.st_mode)) { fprintf(stderr,"Expected regular image file\n"); goto out; }
	if(!im || !scratch || moob_parse(im,(m_u64)st.st_size,read_at,&fd,scratch,&why)) {
		fprintf(stderr,"Invalid MoobFS image: %s\n",why); goto out;
	}
	printf("{\"format\":\"MOOBFSR1\",\"image_bytes\":%llu,\"file_count\":%u,\"object_count\":%u,\"files\":[",(unsigned long long)im->image_size,im->file_count,im->object_count);
	for(i=0;i<im->file_count;i++) {
		struct moob_file *f=&im->files[i]; struct moob_object *o=&im->objects[f->object_index];
		if(i) putchar(',');
		printf("{\"name\":"); json_string(f->name);
		printf(",\"inode\":%llu,\"object_id\":%llu,\"offset\":%llu,\"size\":%llu,\"sha256\":\"",(unsigned long long)f->ino,(unsigned long long)o->id,(unsigned long long)o->offset,(unsigned long long)o->size);
		for(j=0;j<32;j++) printf("%02x",o->sha256[j]);
		printf("\"}");
	}
	puts("]}"); rc=0;
out:
	free(im); free(scratch); close(fd); return rc;
}
