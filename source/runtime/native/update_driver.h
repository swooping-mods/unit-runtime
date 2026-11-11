#ifndef MOON_NATIVE_UPDATE_DRIVER_H
#define MOON_NATIVE_UPDATE_DRIVER_H
#include "../sdk/update_driver.h"
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(PTR,char *,DWORD);
static inline int moon_update_attach(const char *module_id,const char *symbol,MoonUpdateFrame callback){
    unsigned length=0;if(!module_id||!symbol||!callback)return 0;
    while(module_id[length]){char c=module_id[length++];if(length>64||!((c>='a'&&c<='z')||c=='-'))return 0;}
    if(!length)return 0;
    char path[1200];DWORD n=GetModuleFileNameA(0,path,1000);if(!n||n>=1000)return 0;
    while(n&&path[n-1]!='\\'&&path[n-1]!='/')n--;if(!n)return 0;
    char *p=cat(path+n,"MoonLab\\mods\\");p=cat(p,module_id);cat(p,"\\mod.dll");
    /* Never load a dependency or recurse into its ModStart from admission. */
    PTR module=GetModuleHandleA(path);if(!module)return 0;
    MoonUpdateAttach attach=(MoonUpdateAttach)GetProcAddress(module,symbol);
    return attach&&attach(MOON_UPDATE_DRIVER_ABI,callback);
}
#endif
