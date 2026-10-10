/* SPDX-License-Identifier: GPL-2.0-only */
#include "moobfs_format.h"

m_u32 moob_crc32c(m_u32 crc, const void *data, size_t len)
{
	const m_u8 *p = data;
	unsigned i;
	while (len--) {
		crc ^= *p++;
		for (i=0;i<8;i++) crc = (crc>>1) ^ (0x82f63b78U & (0U-(crc&1)));
	}
	return crc;
}

static m_u32 ror(m_u32 x, unsigned n) { return (x>>n)|(x<<(32-n)); }
static void transform(struct moob_sha256 *c, const m_u8 *p)
{
	static const m_u32 k[64] = {
		0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
		0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
		0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
		0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
		0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
		0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
		0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
		0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
	};
	m_u32 w[64],a,b,d,e,f,g,h,t1,t2,cc;
	unsigned i;
	for(i=0;i<16;i++) w[i]=(m_u32)p[i*4]<<24|(m_u32)p[i*4+1]<<16|(m_u32)p[i*4+2]<<8|p[i*4+3];
	for(i=16;i<64;i++) {
		m_u32 x=w[i-15], y=w[i-2];
		w[i]=(ror(x,7)^ror(x,18)^(x>>3))+w[i-16]+(ror(y,17)^ror(y,19)^(y>>10))+w[i-7];
	}
	a=c->h[0]; b=c->h[1]; cc=c->h[2]; d=c->h[3];
	e=c->h[4]; f=c->h[5]; g=c->h[6]; h=c->h[7];
	for(i=0;i<64;i++) {
		t1=h+(ror(e,6)^ror(e,11)^ror(e,25))+((e&f)^((~e)&g))+k[i]+w[i];
		t2=(ror(a,2)^ror(a,13)^ror(a,22))+((a&b)^(a&cc)^(b&cc));
		h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
	}
	c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
	c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}
void moob_sha_init(struct moob_sha256 *c)
{
	static const m_u32 initial[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
	memcpy(c->h,initial,sizeof(initial)); c->bytes=0; c->used=0;
}
void moob_sha_update(struct moob_sha256 *c, const void *data, size_t len)
{
	const m_u8 *p=data;
	c->bytes+=len;
	while(len) {
		size_t n=64-c->used;
		if(n>len) n=len;
		memcpy(c->block+c->used,p,n); c->used+=n; p+=n; len-=n;
		if(c->used==64) { transform(c,c->block); c->used=0; }
	}
}
void moob_sha_final(struct moob_sha256 *c, m_u8 out[32])
{
	m_u64 bits=c->bytes*8;
	unsigned i;
	c->block[c->used++]=0x80;
	if(c->used>56) { memset(c->block+c->used,0,64-c->used); transform(c,c->block); c->used=0; }
	memset(c->block+c->used,0,56-c->used);
	for(i=0;i<8;i++) c->block[63-i]=(m_u8)(bits>>(i*8));
	transform(c,c->block);
	for(i=0;i<32;i++) out[i]=(m_u8)(c->h[i/4]>>(24-(i%4)*8));
}
