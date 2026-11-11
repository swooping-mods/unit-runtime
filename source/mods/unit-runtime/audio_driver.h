#ifndef MOON_UNIT_AUDIO_DRIVER_H
#define MOON_UNIT_AUDIO_DRIVER_H
#include "../../runtime/native/update_driver.h"
#include "../../runtime/native/services.h"
static int unit_audio_parent;
static void unit_audio_update(PTR walker){
    if(!__atomic_load_n(&unit_started,__ATOMIC_ACQUIRE))return;
    Context context={0};context.rcx=(U64)walker;unit_dispatch(0,&context);
}
__declspec(dllexport) int MoonLab_AttachSoundFrame_v1(unsigned abi,MoonUpdateFrame callback){
    if(moon_runtime_loader_present()||abi!=MOON_UPDATE_DRIVER_ABI||!callback||!__atomic_load_n(&unit_started,__ATOMIC_ACQUIRE)||
       GetCurrentThreadId()!=unit_registration_thread||unit_audio_parent||__atomic_load_n(&unit_audio_frame,__ATOMIC_ACQUIRE))return 0;
    __atomic_store_n(&unit_audio_frame,callback,__ATOMIC_RELEASE);return 1;
}
static int unit_attach_audio_driver(void){
    if(moon_runtime_loader_present()||unit_audio_parent||__atomic_load_n(&unit_audio_frame,__ATOMIC_ACQUIRE))return 0;
    if(!moon_update_attach("sound-framework",MOON_ATTACH_UNIT_FRAME,unit_audio_update))return 0;
    unit_audio_parent=1;return 1;
}
#endif
