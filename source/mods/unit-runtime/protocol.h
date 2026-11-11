#ifndef MOON_UNIT_PROTOCOL_H
#define MOON_UNIT_PROTOCOL_H
/* Explicit little-endian encoding; never serialize compiler padding/pointers. */
#include "sdk.h"
#define MUP_HEADER 48u
#define MUP_MAX (MUP_HEADER+MU_PAYLOAD_MAX)
enum {MUP_HELLO=1,MUP_WELCOME,MUP_SNAPSHOT,MUP_COMMAND,MUP_RESULT};
typedef unsigned long long MU64;
typedef struct {
    unsigned type,provider,length;
    MU64 session,sequence,nonce;
    const unsigned char *payload;
} MUPacket;
static inline void mup_w32(unsigned char *p,unsigned n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(i*8));}
static inline unsigned mup_r32(const unsigned char *p){unsigned n=0;for(unsigned i=0;i<4;i++)n|=(unsigned)p[i]<<(i*8);return n;}
static inline void mup_w64(unsigned char *p,MU64 n){for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(n>>(i*8));}
static inline MU64 mup_r64(const unsigned char *p){MU64 n=0;for(unsigned i=0;i<8;i++)n|=(MU64)p[i]<<(i*8);return n;}
static inline int mup_read(MUPacket *p,const unsigned char *b,unsigned n){
    if(!p||!b||n<MUP_HEADER||n>MUP_MAX||mup_r32(b)!=0x31554d4du||mup_r32(b+4)!=1||mup_r32(b+20))return 0;
    MUPacket q={mup_r32(b+8),mup_r32(b+12),mup_r32(b+16),mup_r64(b+24),mup_r64(b+32),mup_r64(b+40),b+MUP_HEADER};
    if(q.length!=n-MUP_HEADER||q.type<MUP_HELLO||q.type>MUP_RESULT)return 0;
    if(q.type==MUP_HELLO&&(q.provider||q.length||q.session||q.sequence||!q.nonce))return 0;
    if(q.type==MUP_WELCOME&&(q.provider||q.length||!q.session||q.sequence||!q.nonce))return 0;
    if(q.type==MUP_SNAPSHOT&&(!q.provider||q.provider>MU_PROVIDER_MAX||!q.session||!q.sequence||q.nonce))return 0;
    if(q.type>=MUP_COMMAND&&(!q.provider||q.provider>MU_PROVIDER_MAX||!q.session||!q.sequence||!q.nonce||!q.length||q.length>MU_COMMAND_MAX+12u))return 0;
    if(q.type==MUP_COMMAND&&q.length>MU_COMMAND_MAX)return 0;
    *p=q;return 1;
}
static inline unsigned mup_write(unsigned char *b,unsigned capacity,unsigned type,unsigned provider,MU64 session,MU64 sequence,MU64 nonce,const unsigned char *data,unsigned n){
    if(!b||n>MU_PAYLOAD_MAX||capacity<MUP_HEADER+n||(!data&&n))return 0;
    mup_w32(b,0x31554d4du);mup_w32(b+4,1);mup_w32(b+8,type);mup_w32(b+12,provider);mup_w32(b+16,n);mup_w32(b+20,0);
    mup_w64(b+24,session);mup_w64(b+32,sequence);mup_w64(b+40,nonce);
    for(unsigned i=0;i<n;i++)b[MUP_HEADER+i]=data[i];MUPacket check;
    return mup_read(&check,b,MUP_HEADER+n)?MUP_HEADER+n:0;
}
typedef struct {MU64 host,nonce,session,last[MU_PROVIDER_MAX];unsigned established,suspended;} MUReplica;
static inline int mup_accept(MUReplica *r,MU64 authenticated_peer,const MUPacket *p){
    if(!r||!p||r->suspended||!r->host||authenticated_peer!=r->host)return 0;
    if(p->type==MUP_WELCOME){
        if(p->nonce!=r->nonce)return 0;
        if(r->established)return p->session==r->session;
        r->session=p->session;r->established=1;return 1;
    }
    if(p->type!=MUP_SNAPSHOT||!r->established||p->session!=r->session||p->provider<1||p->provider>MU_PROVIDER_MAX)return 0;
    return p->sequence>r->last[p->provider-1];
}
/* Commit sequence only after the provider validates the complete payload. */
static inline void mup_commit(MUReplica *r,const MUPacket *p){r->last[p->provider-1]=p->sequence;}
#endif
