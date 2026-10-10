/* SPDX-License-Identifier: GPL-2.0-only */
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <linux/fiemap.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL line %d: %s (errno=%d)\n",__LINE__,#c,errno); return 1; } } while(0)
static int extent(int fd, struct fiemap_extent *out)
{
	struct fiemap *map=calloc(1,sizeof(*map)+sizeof(*out));
	int rc;
	if(!map) return -1;
	map->fm_length=~0ULL; map->fm_extent_count=1;
	rc=ioctl(fd,FS_IOC_FIEMAP,map);
	if(!rc && map->fm_mapped_extents==1) *out=map->fm_extents[0]; else rc=-1;
	free(map); return rc;
}
int main(int argc,char **argv)
{
	unsigned char expected[65536], actual[65536];
	struct stat a,b,src;
	struct fiemap_extent ea,eb;
	int x,y,source,names=0;
	size_t pos;
	DIR *dir;
	struct dirent *de;
	if(argc==3 && !strcmp(argv[1],"--reject-rw")) {
		errno=0;
		CHECK(mount(argv[2],"/mnt/moob","moobfs",0,NULL)==-1 && errno==EROFS);
		puts("WRITABLE_MOUNT_REJECTED"); return 0;
	}
	CHECK(argc==2);
	source=open(argv[1],O_RDONLY); CHECK(source>=0); CHECK(!fstat(source,&src));
	CHECK(src.st_size>4096 && src.st_size<(off_t)sizeof(expected));
	CHECK(read(source,expected,(size_t)src.st_size)==src.st_size); close(source);
	x=open("/mnt/moob/alpha.txt",O_RDONLY); y=open("/mnt/moob/beta.txt",O_RDONLY);
	CHECK(x>=0 && y>=0); CHECK(!fstat(x,&a) && !fstat(y,&b));
	CHECK(a.st_ino!=b.st_ino && a.st_nlink==1 && b.st_nlink==1);
	CHECK(a.st_size==src.st_size && b.st_size==src.st_size);
	CHECK(read(x,actual,sizeof(actual))==src.st_size);
	CHECK(!memcmp(expected,actual,(size_t)src.st_size));
	CHECK(read(x,actual,1)==0);
	for(pos=0;pos<(size_t)src.st_size;pos+=997) {
		size_t count=(size_t)src.st_size-pos;
		if(count>997) count=997;
		CHECK(pread(y,actual,count,(off_t)pos)==(ssize_t)count);
		CHECK(!memcmp(expected+pos,actual,count));
	}
	CHECK(pread(y,actual,1,src.st_size+1)==0);
	CHECK(lseek(x,4093,SEEK_SET)==4093);
	CHECK(read(x,actual,19)==19 && !memcmp(actual,expected+4093,19));
	CHECK(!extent(x,&ea) && !extent(y,&eb));
	CHECK(ea.fe_physical==eb.fe_physical && ea.fe_physical>0);
	CHECK((ea.fe_flags&FIEMAP_EXTENT_SHARED) && (eb.fe_flags&FIEMAP_EXTENT_SHARED));
	printf("SHARED: inode=%lu/%lu physical=%llu length=%llu\n",(unsigned long)a.st_ino,(unsigned long)b.st_ino,(unsigned long long)ea.fe_physical,(unsigned long long)ea.fe_length);
	close(x); close(y);
	errno=0; CHECK(open("/mnt/moob/alpha.txt",O_WRONLY)==-1 && errno==EROFS);
	errno=0; CHECK(open("/mnt/moob/new",O_WRONLY|O_CREAT,0600)==-1 && errno==EROFS);
	errno=0; CHECK(unlink("/mnt/moob/alpha.txt")==-1 && errno==EROFS);
	errno=0; CHECK(open("/mnt/moob/missing",O_RDONLY)==-1 && errno==ENOENT);
	dir=opendir("/mnt/moob"); CHECK(dir);
	while((de=readdir(dir))) if(strcmp(de->d_name,".") && strcmp(de->d_name,"..")) names++;
	CHECK(names==2); rewinddir(dir); names=0;
	while((de=readdir(dir))) if(strcmp(de->d_name,".") && strcmp(de->d_name,"..")) names++;
	CHECK(names==2); closedir(dir);
	puts("MOUNTED_READ_TESTS_PASS"); return 0;
}
