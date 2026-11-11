#ifndef MOON_UNIT_COMMANDS_H
#define MOON_UNIT_COMMANDS_H
#include "protocol.h"
enum {MUR_APPLIED=1,MUR_STALE=2};
typedef struct {
    MU64 sequence,nonce;
    unsigned length,result_length;
    unsigned char bytes[MU_COMMAND_MAX],result[MU_COMMAND_MAX];
} MUCommandCache;
typedef struct {
    MU64 sequence;unsigned length,pending;float retry;
    unsigned char bytes[MU_COMMAND_MAX];
} MUCommandPending;
static inline int mur_equal(const unsigned char *a,const unsigned char *b,unsigned n){for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
/* A fresh hello never clears this cache. Watermarks survive disconnect/rejoin
 * for the entire host world, and a retry cannot apply a second wallet debit. */
static inline unsigned mur_command(MUCommandCache *c,const MUPacket *p,MU64 peer,
    unsigned (*execute)(MU64,const unsigned char *,unsigned,unsigned char *,unsigned),unsigned char *out){
    if(!c||!p||!execute||p->type!=MUP_COMMAND||p->length>MU_COMMAND_MAX)return 0;
    if(p->sequence==c->sequence&&p->nonce==c->nonce){
        if(p->length!=c->length||!mur_equal(p->payload,c->bytes,c->length))return 0;
    }else if(p->sequence<=c->sequence){
        mup_w32(out,MUR_STALE);mup_w64(out+4,c->sequence);return 12;
    }else{
        unsigned char result[MU_COMMAND_MAX];unsigned n=execute(peer,p->payload,p->length,result,sizeof(result));
        if(!n||n>sizeof(result))return 0;
        c->sequence=p->sequence;c->nonce=p->nonce;c->length=p->length;c->result_length=n;
        for(unsigned i=0;i<p->length;i++)c->bytes[i]=p->payload[i];
        for(unsigned i=0;i<n;i++)c->result[i]=result[i];
    }
    mup_w32(out,MUR_APPLIED);mup_w64(out+4,c->sequence);
    for(unsigned i=0;i<c->result_length;i++)out[12+i]=c->result[i];return 12+c->result_length;
}
static inline int mur_begin(MUCommandPending *c,const unsigned char *b,unsigned n){
    if(!c||c->pending||!b||!n||n>MU_COMMAND_MAX||c->sequence==~(MU64)0)return 0;
    c->sequence++;c->pending=1;c->length=n;c->retry=0;
    for(unsigned i=0;i<n;i++)c->bytes[i]=b[i];return 1;
}
static inline int mur_result(MUCommandPending *c,const MUPacket *p,MU64 session,MU64 nonce,
    int (*accept)(const unsigned char *,unsigned)){
    if(!c||!c->pending||!p||p->type!=MUP_RESULT||p->session!=session||p->nonce!=nonce||p->sequence!=c->sequence||p->length<12)return 0;
    unsigned code=mup_r32(p->payload);MU64 watermark=mup_r64(p->payload+4);
    if(code==MUR_STALE){
        if(p->length!=12||watermark<c->sequence||watermark==~(MU64)0)return 0;
        c->sequence=watermark+1;c->retry=0;return 1;
    }
    if(code!=MUR_APPLIED||watermark!=c->sequence||!accept||!accept(p->payload+12,p->length-12))return 0;
    c->pending=0;return 1;
}
#endif
