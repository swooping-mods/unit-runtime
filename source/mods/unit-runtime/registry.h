#ifndef MOON_UNIT_REGISTRY_H
#define MOON_UNIT_REGISTRY_H
#include "spatial.h"
/* Stable individual slots shared across providers. Sparse game dock IDs and
 * provider IDs are keys, never array offsets or caller-selected authority. */
typedef struct {
    MUSpatial spatial;
    unsigned table[MU_BUCKETS],provider[MU_MAX],free_slots[MU_MAX];
    unsigned allocated,free_count,tombstones;
} MURegistry;
static inline unsigned mur_hash(unsigned provider,unsigned id){return (provider*2246822519u^id*2654435761u)&(MU_BUCKETS-1);}
static inline int mur_lookup(const MURegistry *r,unsigned provider,unsigned id,int *insertion){
    unsigned at=mur_hash(provider,id);int tomb=-1;if(insertion)*insertion=-1;
    for(unsigned probe=0;probe<MU_BUCKETS;probe++,at=(at+1)&(MU_BUCKETS-1)){
        unsigned entry=r->table[at];
        if(!entry){if(insertion)*insertion=tomb>=0?tomb:(int)at;return -1;}
        if(entry==~0u){if(tomb<0)tomb=(int)at;continue;}
        unsigned slot=entry-1;if(r->provider[slot]==provider&&r->spatial.items[slot].id==id){if(insertion)*insertion=(int)at;return (int)slot;}
    }
    if(insertion)*insertion=tomb;return -1;
}
static inline void mur_rehash(MURegistry *r){
    mu_zero(r->table,sizeof(r->table));r->tombstones=0;
    for(unsigned slot=0;slot<r->allocated;slot++)if(r->spatial.items[slot].active){
        unsigned at=mur_hash(r->provider[slot],r->spatial.items[slot].id);
        while(r->table[at])at=(at+1)&(MU_BUCKETS-1);r->table[at]=slot+1;
    }
}
static inline int mur_publish(MURegistry *r,unsigned provider,unsigned id,unsigned kind,MUVec p,float radius){
    if(!r||!provider||!id||!mu_valid(p)||!mu_finite(radius)||radius<0||radius>1000)return 0;
    if(r->tombstones>MU_BUCKETS/4)mur_rehash(r);
    int insertion=-1,slot=mur_lookup(r,provider,id,&insertion);
    if(slot<0){
        if(insertion<0||(!r->free_count&&r->allocated==MU_MAX))return 0;
        slot=(int)(r->free_count?r->free_slots[--r->free_count]:r->allocated++);
        if(r->table[insertion]==~0u)r->tombstones--;
        r->table[insertion]=(unsigned)slot+1;r->provider[slot]=provider;
    }
    return mu_publish(&r->spatial,(unsigned)slot,id,kind,p,radius);
}
static inline void mur_remove(MURegistry *r,unsigned provider,unsigned id){
    if(!r)return;int at=-1,slot=mur_lookup(r,provider,id,&at);if(slot<0)return;
    mu_remove(&r->spatial,(unsigned)slot);r->provider[slot]=0;r->free_slots[r->free_count++]=(unsigned)slot;
    r->table[at]=~0u;r->tombstones++;
}
#endif
