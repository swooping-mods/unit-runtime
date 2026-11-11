#include "../../runtime/native/gameplay.h"
#include "../../runtime/native/local_actor.h"
#include "../../runtime/native/services.h"
#include "adapters/unity.h"
#include "adapters/transport.h"
int _fltused=0;
void *memcpy(void *to,const void *from,U64 n){su_copy(to,from,n);return to;}
void *memset(void *to,int c,U64 n){unsigned char *p=to;for(U64 i=0;i<n;i++)p[i]=(unsigned char)c;return to;}
static PTR unit_log;
static void event(const char *name){
    char b[256],*p=cat(b,"{\"event\":\"");p=cat(p,name);p=cat(p,"\"}\n");
    DWORD n;WriteFile(unit_log,b,(DWORD)(p-b),&n,0);FlushFileBuffers(unit_log);
}
#include "service.h"
static int unit_dispatch(int id,Context *context){return unit_before(id,context);}
static void unit_runtime_frame(void *walker,void *user){
    (void)user;Context context={0};context.rcx=(U64)walker;unit_dispatch(0,&context);
}
#include "audio_driver.h"
static int unit_publish_apis(void){
    const MoonRuntimeApiV1 *runtime=moon_runtime_connect();if(!runtime)return 0;
    static const MoonServiceLimitV1 unit_limits[]={{"max_units",MU_UNIT_MAX},{"max_providers",MU_PROVIDER_MAX}};
    static const MoonServiceLimitV1 contact_limits[]={{"max_units",MU_UNIT_MAX}};
    MoonServicePublicationLimitsV1 units={
        .base={.size=sizeof(units),.service_id="units",.abi=MU_ABI,.api_size=sizeof(unit_api),.api=&unit_api},
        .limit_count=2,.limits=unit_limits
    };
    MoonServicePublicationLimitsV1 contacts={
        .base={.size=sizeof(contacts),.service_id="contacts",.abi=MU_CONTACT_ABI,.api_size=sizeof(unit_contacts_api),.api=&unit_contacts_api},
        .limit_count=1,.limits=contact_limits
    };
    MoonServicePublicationV1 sources={.size=sizeof(sources),.service_id="sources",.abi=MU_SOURCES_ABI,.api_size=sizeof(unit_sources_api),.api=&unit_sources_api};
    return runtime->publish(&units.base)&&runtime->publish(&contacts.base)&&runtime->publish(&sources);
}
__declspec(dllexport) int MoonLab_ModStart(PTR domain,PTR game){
    if(__atomic_load_n(&unit_started,__ATOMIC_ACQUIRE))return 1;
    char folder[1000],path[1100];DWORD n=GetEnvironmentVariableA("MOONLAB_EVIDENCE",folder,sizeof(folder));if(!n||n>=sizeof(folder))return 0;
    cat(cat(path,folder),"\\unit-runtime.jsonl");unit_log=CreateFileA(path,0x40000000,1,0,1,0x80,0);if(unit_log==(PTR)(U64)-1)return 0;
    if(!bind_runtime(domain,game)||!su_bind(game)||!st_bind(game)||!moon_local_actor_bind()){event("bind-failed");return 0;}
    unit_world_field=get_field(find_class("MoonCleaners","MoonWalker"),"world");
    unit_restoring=su_method("MoonCleaners","MoonWorld","get_IsRestoringRendering","");unit_frame=su_method("UnityEngine","Time","get_frameCount","");
    unit_real_delta=su_method("UnityEngine","Time","get_unscaledDeltaTime","");
    if(!unit_world_field||!unit_restoring||!unit_frame||!unit_real_delta)return 0;
    unit_registration_thread=GetCurrentThreadId();
    int loader=moon_runtime_loader_present();
    if(loader){
        if(!moon_runtime_subscribe_frame(unit_runtime_frame,0)){event("frame-service-rejected");return 0;}
    }else{
        if(unit_attach_audio_driver())event("audio-update-driver");
        else if(!prepare_hook(method("MoonCleaners","MoonWalker","Update",0),100671733,0,game)){event("hook-shape-failed");return 0;}
        before_hook=unit_dispatch;if(!install_hooks())return 0;
    }
    __atomic_store_n(&unit_started,1,__ATOMIC_RELEASE);
    if(loader&&!unit_publish_apis()){event("api-publication-failed");return 0;}
    event("ready");return 1;
}
