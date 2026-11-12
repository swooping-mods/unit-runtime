#ifndef MOON_UNIT_SERVICE_H
#define MOON_UNIT_SERVICE_H
#include "sdk.h"
#include "../../runtime/sdk/update_driver.h"
#include "spatial.h"
#include "registry.h"
#include "protocol.h"
#include "commands.h"
#include "clock.h"
#include "source_registry.h"
_Static_assert(MU_UNIT_MAX==MU_MAX,"public contact capacity matches the spatial registry");
static MoonUnitProvider unit_providers[MU_PROVIDER_MAX];
static MUClock unit_clocks[MU_PROVIDER_MAX];
static MURegistry unit_registry;
static unsigned unit_modded_live;
static unsigned unit_native_live;
static unsigned unit_modded_counts[MU_PROVIDER_MAX];
static MUSourceRegistry unit_sources;
static void (*unit_audio_frame)(void *walker);
static MUReplica unit_replica;
static MUCommandPending unit_pending[MU_PROVIDER_MAX];
static struct {U64 id,nonce;float ttl;MUCommandCache commands[MU_PROVIDER_MAX];} unit_peers[ST_BATCH];
static SURoot unit_world;
static PTR unit_world_field,unit_restoring,unit_frame,unit_real_delta;
static DWORD unit_thread,unit_registration_thread;
static int unit_authoritative,unit_suspended,unit_started;
static int unit_world_joined;
static U64 unit_world_local,unit_world_host;
static U64 unit_session,unit_sequence,unit_frame_seen;
static float unit_send_time,unit_hello_time;
static float unit_stats_time,unit_hello_age,unit_snapshot_age;
static unsigned unit_rtt_ms;static U64 unit_commands_applied,unit_commands_replayed;
static U64 unit_nonce(void){
    U64 value=0;BOOL (*counter)(U64*)=(BOOL(*)(U64*))GetProcAddress(GetModuleHandleA("kernel32.dll"),"QueryPerformanceCounter");
    if(counter)counter(&value);value^=(U64)GetCurrentProcessId()<<32;return value?value:1;
}
static int unit_provider_live(unsigned i){return __atomic_load_n(&unit_providers[i].id,__ATOMIC_ACQUIRE)!=0;}
static int unit_main(void){DWORD thread=__atomic_load_n(&unit_thread,__ATOMIC_ACQUIRE);return thread&&thread==GetCurrentThreadId();}
static int unit_host(void){return unit_main()&&unit_world&&unit_authoritative&&!unit_suspended;}
static U64 unit_session_value(void){return unit_main()&&!unit_suspended?unit_session:0;}
static int unit_register(const MoonUnitProvider *p){
    if(!p||p->size!=sizeof(*p)||p->id<1||p->id>MU_PROVIDER_MAX||!p->snapshot||!p->receive||!p->reset||!p->frame||GetCurrentThreadId()!=unit_registration_thread)return 0;
    MoonUnitProvider *old=&unit_providers[p->id-1];
    if((!p->command)!=(!p->result))return 0;
    if(unit_provider_live(p->id-1))return old->snapshot==p->snapshot&&old->receive==p->receive&&old->reset==p->reset&&old->frame==p->frame&&old->command==p->command&&old->result==p->result;
    /* A standalone dependency's hook can start before its consumer ModStart
     * finishes. Publish the immutable callback table only after initialization. */
    old->size=p->size;old->snapshot=p->snapshot;old->receive=p->receive;old->reset=p->reset;old->frame=p->frame;old->command=p->command;old->result=p->result;
    __atomic_store_n(&old->id,p->id,__ATOMIC_RELEASE);return 1;
}
static int unit_slot(unsigned provider,unsigned id){
    if(provider<1||provider>MU_PROVIDER_MAX||!id||!unit_provider_live(provider-1))return -1;
    return mur_lookup(&unit_registry,provider,id,0);
}
static int unit_publish(unsigned provider,unsigned id,unsigned kind,MoonUnitVector position,float radius){
    if(!unit_host()||provider<1||provider>MU_PROVIDER_MAX||!unit_provider_live(provider-1))return 0;
    int fresh=mur_lookup(&unit_registry,provider,id,0)<0;
    int result=mur_publish(&unit_registry,provider,id,kind,(MUVec){position.x,position.y,position.z},radius);
    if(result&&fresh){if(provider==1)unit_native_live++;else{unit_modded_live++;unit_modded_counts[provider-1]++;}}return result;
}
static void unit_remove(unsigned provider,unsigned id){if(unit_main()){
    if(provider==1&&mur_lookup(&unit_registry,provider,id,0)>=0)unit_native_live--;
    if(provider>1&&provider<=MU_PROVIDER_MAX&&mur_lookup(&unit_registry,provider,id,0)>=0){unit_modded_live--;unit_modded_counts[provider-1]--;}
    mur_remove(&unit_registry,provider,id);
}}
static int unit_nearby(MoonUnitVector position,float radius,unsigned provider,unsigned id,MoonUnitNearby *out,unsigned capacity,unsigned *required){
    if(required)*required=0;if(!unit_host()||!required||(!out&&capacity)||!mu_valid((MUVec){position.x,position.y,position.z})||!mu_finite(radius)||radius<0||radius>1000)return 0;
    if(!mus_prepare(&unit_sources)||!unit_host())return 0;
    static unsigned slots[MU_MAX];unsigned count=mu_query(&unit_registry.spatial,(MUVec){position.x,position.y,position.z},radius,unit_slot(provider,id),0,slots);
    *required=count;if(count>capacity)return -1;
    for(unsigned i=0;i<count;i++){MUItem *p=&unit_registry.spatial.items[slots[i]];out[i]=(MoonUnitNearby){unit_registry.provider[slots[i]],p->id,p->kind,{p->point.x,p->point.y,p->point.z},p->radius};}
    return 1;
}
static int unit_foreign_nearby(MoonUnitVector position,float radius,unsigned own_provider,MoonUnitNearby *out,unsigned capacity,unsigned *required){
    if(required)*required=0;
    if(!unit_host()||!required||(!out&&capacity)||own_provider<1||own_provider>MU_PROVIDER_MAX||!unit_provider_live(own_provider-1)||
        !mu_valid((MUVec){position.x,position.y,position.z})||!mu_finite(radius)||radius<0||radius>1000)return 0;
    if(unit_modded_live==unit_modded_counts[own_provider-1])return 1;
    static unsigned slots[MU_MAX];unsigned total=mu_query(&unit_registry.spatial,(MUVec){position.x,position.y,position.z},radius,-1,0,slots),count=0;
    for(unsigned i=0;i<total;i++){unsigned provider=unit_registry.provider[slots[i]];if(provider!=1&&provider!=own_provider)count++;}
    *required=count;if(count>capacity)return -1;
    unsigned next=0;for(unsigned i=0;i<total;i++){
        unsigned slot=slots[i],provider=unit_registry.provider[slot];if(provider==1||provider==own_provider)continue;
        MUItem *p=&unit_registry.spatial.items[slot];out[next++]=(MoonUnitNearby){provider,p->id,p->kind,{p->point.x,p->point.y,p->point.z},p->radius};
    }return 1;
}
static const MoonUnitContacts unit_contacts_api={sizeof(MoonUnitContacts),MU_CONTACT_ABI,unit_foreign_nearby};
__declspec(dllexport) const MoonUnitContacts *MoonLab_UnitContacts_v1(unsigned abi,unsigned size){
    return __atomic_load_n(&unit_started,__ATOMIC_ACQUIRE)&&abi==MU_CONTACT_ABI&&size==sizeof(unit_contacts_api)?&unit_contacts_api:0;
}
static unsigned unit_steps(unsigned provider,U64 frame,float dt){
    if(!unit_host()||provider<1||provider>MU_PROVIDER_MAX||!unit_provider_live(provider-1))return 0;
    return muc_steps(&unit_clocks[provider-1],frame,dt);
}
static int unit_command(unsigned provider,const unsigned char *bytes,unsigned length){
    if(!unit_main()||unit_authoritative||unit_suspended||!unit_replica.established||provider<1||provider>MU_PROVIDER_MAX||!unit_providers[provider-1].result)return 0;
    return mur_begin(&unit_pending[provider-1],bytes,length);
}
static PTR unit_peer_actor(U64 peer){
    if(!unit_host()||!st_member(peer))return 0;
    PTR list=0;su.static_field(st.shown,&list);if(!list)return 0;
    PTR cls=object_class(list),count_method=get_method(cls,"get_Count",0),item=get_method(cls,"get_Item",1);int ok=1,count=integer(count_method,list,&ok);
    if(!ok||count<0||count>ST_BATCH||!item)return 0;
    for(int i=0;i<count;i++){void *args[]={&i};PTR actor=call(item,list,args,&ok);if(ok&&su_alive(actor)&&st_u64(st.remote_id,actor)==peer)return actor;}
    return 0;
}
static const MoonUnits unit_api={sizeof(MoonUnits),MU_ABI,unit_register,unit_publish,unit_remove,unit_nearby,unit_steps,unit_host,unit_session_value,unit_command,unit_peer_actor};
__declspec(dllexport) const MoonUnits *MoonLab_Units_v1(unsigned abi,unsigned size){return __atomic_load_n(&unit_started,__ATOMIC_ACQUIRE)&&abi==MU_ABI&&size==sizeof(unit_api)?&unit_api:0;}
static int unit_register_source(const MoonUnitQuerySource *source){
    if(!__atomic_load_n(&unit_started,__ATOMIC_ACQUIRE)||GetCurrentThreadId()!=unit_registration_thread
       ||!source||source->provider<1||source->provider>MU_PROVIDER_MAX||!unit_provider_live(source->provider-1))return 0;
    return mus_register(&unit_sources,source);
}
static PTR unit_source_world(void){return unit_main()&&!unit_suspended?su_get(unit_world):0;}
static unsigned unit_source_count(unsigned provider){
    if(!unit_main()||unit_suspended||provider>MU_PROVIDER_MAX)return 0;
    return !provider?unit_modded_live:provider==1?unit_native_live:unit_modded_counts[provider-1];
}
static int unit_suspend_session(const char *reason){
    if(!unit_main())return 0;
    if(unit_suspended)return 1;
    unit_suspended=1;unit_replica.suspended=1;
    for(unsigned i=0;i<MU_PROVIDER_MAX;i++)if(unit_provider_live(i))unit_providers[i].reset();
    mu_zero(&unit_registry,sizeof(unit_registry));unit_native_live=unit_modded_live=0;mu_zero(unit_modded_counts,sizeof(unit_modded_counts));
    event(reason);return 1;
}
static int unit_suspend(void){return unit_suspend_session("query-source-suspended-rejoin-required");}
/* A loaded world's role is immutable. A temporary loss of crew/local identity
 * cannot turn an online host into an offline host or transfer pending commands
 * to another local account. Reentry establishes a fresh session instead. */
static int unit_authority_current(void){
    if(unit_suspended||!unit_world)return 0;
    int joined=st_joined();
    if(joined!=unit_world_joined||(joined&&
       (st_local_id()!=unit_world_local||st_host_id()!=unit_world_host))){
        unit_suspend_session("authority-changed-rejoin-required");return 0;
    }
    return 1;
}
static const MoonUnitSources unit_sources_api={sizeof(MoonUnitSources),MU_SOURCES_ABI,unit_register_source,unit_source_world,unit_main,unit_source_count,unit_suspend};
__declspec(dllexport) const MoonUnitSources *MoonLab_UnitSources_v1(unsigned abi,unsigned size){
    return __atomic_load_n(&unit_started,__ATOMIC_ACQUIRE)&&abi==MU_SOURCES_ABI&&size==sizeof(unit_sources_api)?&unit_sources_api:0;
}
static void unit_reset(void){
    for(unsigned i=0;i<MU_PROVIDER_MAX;i++)if(unit_provider_live(i))unit_providers[i].reset();
    su_drop(&unit_world);mu_zero(&unit_registry,sizeof(unit_registry));unit_native_live=unit_modded_live=0;mu_zero(unit_modded_counts,sizeof(unit_modded_counts));mu_zero(unit_clocks,sizeof(unit_clocks));
    mu_zero(&unit_replica,sizeof(unit_replica));unit_session=unit_sequence=unit_frame_seen=0;unit_authoritative=unit_suspended=0;
    unit_world_joined=0;unit_world_local=unit_world_host=0;
    mu_zero(unit_peers,sizeof(unit_peers));mu_zero(unit_pending,sizeof(unit_pending));
    unit_send_time=unit_hello_time=0;
    unit_stats_time=unit_hello_age=unit_snapshot_age=0;unit_rtt_ms=0;unit_commands_applied=unit_commands_replayed=0;
}
static void unit_welcome(U64 peer,U64 nonce){
    unsigned char b[MUP_HEADER];unsigned n=mup_write(b,sizeof(b),MUP_WELCOME,0,unit_session,0,nonce,0,0);if(n)st_send(peer,b,n);
}
static void unit_receive(U64 peer,const unsigned char *b,unsigned n,void *ctx){
    (void)ctx;MUPacket p;if(!mup_read(&p,b,n)||unit_suspended)return;
    if(unit_authoritative){
        unsigned slot=ST_BATCH,empty=ST_BATCH;for(unsigned i=0;i<ST_BATCH;i++){if(unit_peers[i].id==peer)slot=i;if(!unit_peers[i].id&&empty==ST_BATCH)empty=i;}
        if(p.type==MUP_HELLO){
            if(slot==ST_BATCH)slot=empty;if(slot==ST_BATCH)return;
            if(unit_peers[slot].nonce!=p.nonce)event("peer-handshake");
            unit_peers[slot].id=peer;unit_peers[slot].nonce=p.nonce;unit_peers[slot].ttl=5;unit_welcome(peer,p.nonce);return;
        }
        if(slot==ST_BATCH||unit_peers[slot].ttl<=0||p.type!=MUP_COMMAND||p.session!=unit_session||p.nonce!=unit_peers[slot].nonce)return;
        if(!unit_provider_live(p.provider-1))return;MoonUnitProvider *provider=&unit_providers[p.provider-1];unsigned char result[MU_COMMAND_MAX+12],packet[MUP_HEADER+MU_COMMAND_MAX+12];
        MUCommandCache *cache=&unit_peers[slot].commands[p.provider-1];U64 previous=cache->sequence;
        unsigned length=mur_command(cache,&p,peer,provider->command,result);
        if(length&&mup_r32(result)==MUR_APPLIED){if(cache->sequence!=previous)unit_commands_applied++;else unit_commands_replayed++;}
        if(length){unsigned size=mup_write(packet,sizeof(packet),MUP_RESULT,p.provider,unit_session,p.sequence,p.nonce,result,length);if(size)st_send(peer,packet,size);}return;
    }
    if(peer==unit_replica.host&&unit_replica.established&&p.type==MUP_RESULT&&unit_provider_live(p.provider-1)){
        mur_result(&unit_pending[p.provider-1],&p,unit_replica.session,unit_replica.nonce,unit_providers[p.provider-1].result);return;
    }
    int established=unit_replica.established;if(!mup_accept(&unit_replica,peer,&p))return;
    if(p.type==MUP_WELCOME){unit_session=unit_replica.session;unit_rtt_ms=(unsigned)(unit_hello_age*1000);if(!established)event("host-handshake");return;}
    MoonUnitProvider *provider=&unit_providers[p.provider-1];
    if(unit_provider_live(p.provider-1)&&provider->receive(p.payload,p.length)){mup_commit(&unit_replica,&p);unit_snapshot_age=0;}
}
static void unit_network_stats(float dt){
    unit_hello_age+=dt;if(unit_hello_age>10000)unit_hello_age=10000;unit_snapshot_age+=dt;if(unit_snapshot_age>10000)unit_snapshot_age=10000;
    unit_stats_time-=dt;if(unit_stats_time>0)return;unit_stats_time=5;
    unsigned peers=0,pending=0;for(unsigned i=0;i<ST_BATCH;i++)if(unit_peers[i].ttl>0)peers++;for(unsigned i=0;i<MU_PROVIDER_MAX;i++)pending+=unit_pending[i].pending;
    if(!peers&&!unit_replica.established)return;
    char b[768],*p=cat(b,"{\"event\":\"unit-network-stats\",\"host\":");p=num(p,unit_authoritative);p=cat(p,",\"peers\":");p=num(p,peers);p=cat(p,",\"pending\":");p=num(p,pending);
    p=cat(p,",\"rtt_ms\":");p=num(p,unit_rtt_ms);p=cat(p,",\"snapshot_age_ms\":");p=num(p,(unsigned)(unit_snapshot_age*1000));p=cat(p,",\"sent\":");p=num(p,st.sent);p=cat(p,",\"received\":");p=num(p,st.delivered);p=cat(p,",\"dropped\":");p=num(p,st.dropped);p=cat(p,",\"send_failed\":");p=num(p,st.send_failed);
    p=cat(p,",\"commands_applied\":");p=num(p,unit_commands_applied);p=cat(p,",\"commands_replayed\":");p=num(p,unit_commands_replayed);p=cat(p,"}\n");DWORD n;WriteFile(unit_log,b,(DWORD)(p-b),&n,0);FlushFileBuffers(unit_log);
}
static void unit_network(float dt){
    if(!unit_authority_current())return;
    int joined=unit_world_joined;
    unit_network_stats(dt);st_receive(unit_receive,0);
    if(unit_suspended)return;
    unit_send_time-=dt;unit_hello_time-=dt;
    if(unit_authoritative){
        for(unsigned i=0;i<ST_BATCH;i++){unit_peers[i].ttl-=dt;if(unit_peers[i].ttl<0)unit_peers[i].ttl=0;}
        if(unit_send_time>0||!joined)return;unit_send_time=.1f;
        unsigned count=0;for(unsigned i=0;i<ST_BATCH;i++)if(unit_peers[i].ttl>0&&st_member(unit_peers[i].id))count++;if(!count)return;
        for(unsigned i=0;i<MU_PROVIDER_MAX;i++)if(unit_provider_live(i)){
            unsigned char payload[MU_PAYLOAD_MAX],packet[MUP_MAX];
            unsigned n=unit_providers[i].snapshot(payload,sizeof(payload));if(unit_suspended)return;if(!n||n>sizeof(payload))continue;
            if(unit_sequence==~(U64)0){unit_suspend_session("sequence-exhausted");return;}
            unsigned bytes=mup_write(packet,sizeof(packet),MUP_SNAPSHOT,i+1,unit_session,++unit_sequence,0,payload,n);
            for(unsigned peer=0;peer<ST_BATCH;peer++)if(unit_peers[peer].ttl>0)st_send_mode(unit_peers[peer].id,packet,bytes,0);
        }
    }else{
        if(unit_hello_time<=0){
            unsigned char b[MUP_HEADER];unsigned n=mup_write(b,sizeof(b),MUP_HELLO,0,0,0,unit_replica.nonce,0,0);
            if(n)st_send(unit_replica.host,b,n);unit_hello_time=2;unit_hello_age=0;
        }
        if(unit_replica.established)for(unsigned i=0;i<MU_PROVIDER_MAX;i++){
            MUCommandPending *pending=&unit_pending[i];if(!pending->pending)continue;pending->retry-=dt;if(pending->retry>0)continue;
            unsigned char b[MUP_HEADER+MU_COMMAND_MAX];unsigned n=mup_write(b,sizeof(b),MUP_COMMAND,i+1,unit_session,pending->sequence,unit_replica.nonce,pending->bytes,pending->length);
            if(n)st_send(unit_replica.host,b,n);pending->retry=.5f;
        }
    }
}
static void unit_provider_frames(PTR walker){
    MoonUpdateFrame audio=__atomic_load_n(&unit_audio_frame,__ATOMIC_ACQUIRE);if(audio)audio(walker);
    for(unsigned i=0;i<MU_PROVIDER_MAX;i++)if(unit_provider_live(i))unit_providers[i].frame(walker);
}
static int unit_before(int id,Context *context){
    (void)id;if(!__atomic_load_n(&unit_started,__ATOMIC_ACQUIRE))return 0;
    if(!__atomic_load_n(&unit_thread,__ATOMIC_ACQUIRE)){__atomic_store_n(&unit_thread,GetCurrentThreadId(),__ATOMIC_RELEASE);event("main-thread");}if(!unit_main())return 0;
    PTR local=0;if(!moon_local_actor_read(&local))return 0;
    /* Teardown has no actor and must not consume this frame's local latch. */
    if(!local){if(unit_world){unit_reset();event("world-end");}return 0;}
    if(local!=(PTR)context->rcx)return 0;
    PTR world=0;field_value((PTR)context->rcx,unit_world_field,&world);
    if(unit_world&&(!su_alive(su_get(unit_world))||world!=su_get(unit_world))){unit_reset();event("world-end");}
    if(!world){unit_provider_frames((PTR)context->rcx);return 0;}int ok=1;if(boolean(unit_restoring,world,&ok)||!ok)return 0;
    int frame=integer(unit_frame,0,&ok);if(!ok||frame<0)return 0;
    if(unit_world&&unit_frame_seen==(U64)frame)return 0;
    if(!unit_world){
        int joined=st_joined();U64 local=st_local_id(),host=st_host_id();
        if(joined&&(!local||!host))return 0;
        unit_world=su_hold(world);if(!unit_world)return 0;
        unit_world_joined=joined;unit_world_local=local;unit_world_host=host;
        unit_authoritative=!joined||(local&&host==local);
        if(unit_authoritative)unit_session=unit_nonce();else {unit_replica.host=host;unit_replica.nonce=unit_nonce();}
        event(unit_authoritative?"world-host":"world-guest");
    }
    unit_frame_seen=(U64)frame;
    /* Session liveness uses real frame time even when the scene is paused.
     * Simulation providers retain their own scaled logical clocks. */
    float dt=su_number(unit_real_delta,0);if(!mu_finite(dt)||dt<0)dt=0;if(dt>10)dt=10;
    /* Even a zero/invalid clock must revoke authority before provider frames. */
    unit_network(dt);
    unit_provider_frames((PTR)context->rcx);return 0;
}
#endif
