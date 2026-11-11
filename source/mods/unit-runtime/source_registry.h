#ifndef MOON_UNIT_SOURCE_REGISTRY_H
#define MOON_UNIT_SOURCE_REGISTRY_H
#include "sources.h"
typedef struct {MoonUnitQuerySource entries[MU_PROVIDER_MAX];unsigned preparing;} MUSourceRegistry;
static inline int mus_register(MUSourceRegistry *registry,const MoonUnitQuerySource *source){
    if(!registry||!source||source->size!=sizeof(*source)||source->provider<1||source->provider>MU_PROVIDER_MAX||!source->prepare)return 0;
    MoonUnitQuerySource *old=&registry->entries[source->provider-1];
    int (*published)(void)=__atomic_load_n(&old->prepare,__ATOMIC_ACQUIRE);
    if(published)return published==source->prepare;
    old->size=source->size;old->provider=source->provider;
    __atomic_store_n(&old->prepare,source->prepare,__ATOMIC_RELEASE);return 1;
}
static inline int mus_prepare(MUSourceRegistry *registry){
    if(!registry||registry->preparing)return 0;
    registry->preparing=1;int ready=1;
    for(unsigned i=0;i<MU_PROVIDER_MAX;i++){
        int (*prepare)(void)=__atomic_load_n(&registry->entries[i].prepare,__ATOMIC_ACQUIRE);
        if(prepare&&!prepare()){ready=0;break;}
    }
    registry->preparing=0;return ready;
}
#endif
