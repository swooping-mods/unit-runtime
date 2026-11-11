#ifndef MOON_UNIT_CONTACT_H
#define MOON_UNIT_CONTACT_H
#include "sdk.h"
/* Advisory marker contacts supplement the consumer's real swept physics.
 * The SDK describes spheres, not peer capsule orientations or support shapes. */
static inline MoonUnitVector muc_add(MoonUnitVector a,MoonUnitVector b){return (MoonUnitVector){a.x+b.x,a.y+b.y,a.z+b.z};}
static inline MoonUnitVector muc_sub(MoonUnitVector a,MoonUnitVector b){return (MoonUnitVector){a.x-b.x,a.y-b.y,a.z-b.z};}
static inline MoonUnitVector muc_mul(MoonUnitVector a,float n){return (MoonUnitVector){a.x*n,a.y*n,a.z*n};}
static inline float muc_dot(MoonUnitVector a,MoonUnitVector b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static inline double muc_dot64(MoonUnitVector a,MoonUnitVector b){return (double)a.x*b.x+(double)a.y*b.y+(double)a.z*b.z;}
static inline float muc_abs(float f){return f<0?-f:f;}
static inline MoonUnitVector muc_segment_point(MoonUnitVector a,MoonUnitVector b,MoonUnitVector p){
    MoonUnitVector d=muc_sub(b,a);float l=muc_dot(d,d),t=l>0?muc_dot(muc_sub(p,a),d)/l:0;
    if(t<0)t=0;if(t>1)t=1;return muc_add(a,muc_mul(d,t));
}
static inline float muc_distance2(MoonUnitVector a,MoonUnitVector b){MoonUnitVector d=muc_sub(a,b);return muc_dot(d,d);}
/* The swept capsule's axis forms a parallelogram. Its closest point is on one
 * of four edges, or the interior projection when the two spans are independent. */
static inline float muc_sweep_distance2(MoonUnitVector from,MoonUnitVector to,MoonUnitVector up,float h,MoonUnitVector p){
    MoonUnitVector u=muc_sub(to,from),v=muc_mul(up,2*h),a=muc_sub(from,muc_mul(up,h));
    MoonUnitVector b=muc_add(a,u),c=muc_add(a,v),d=muc_add(b,v);
    float best=muc_distance2(p,muc_segment_point(a,b,p)),q=muc_distance2(p,muc_segment_point(c,d,p));if(q<best)best=q;
    q=muc_distance2(p,muc_segment_point(a,c,p));if(q<best)best=q;
    q=muc_distance2(p,muc_segment_point(b,d,p));if(q<best)best=q;
    double uu=muc_dot64(u,u),vv=muc_dot64(v,v),uv=muc_dot64(u,v),den=uu*vv-uv*uv;
    if(den>0){
        MoonUnitVector w=muc_sub(p,a);double wu=muc_dot64(w,u),wv=muc_dot64(w,v);
        double t=(wu*vv-wv*uv)/den,s=(wv*uu-wu*uv)/den;
        if(t>=0&&t<=1&&s>=0&&s<=1){q=muc_distance2(p,muc_add(a,muc_add(muc_mul(u,(float)t),muc_mul(v,(float)s))));if(q<best)best=q;}
    }return best;
}
static inline int muc_marker_clear(MoonUnitVector from,MoonUnitVector to,MoonUnitVector up,float radius,float height,const MoonUnitNearby *marker){
    MoonUnitVector p=marker->position;float h=height*.5f-radius;if(h<0)h=0;
    float r=radius+marker->radius,limit=r*r;
    if(muc_sweep_distance2(from,to,up,h,p)>=limit)return 1;
    MoonUnitVector axis=muc_mul(up,h),nearest=muc_segment_point(muc_sub(from,axis),muc_add(from,axis),p);
    float before=muc_distance2(p,nearest),after=muc_distance2(p,muc_segment_point(muc_sub(to,axis),muc_add(to,axis),p));
    /* Convex distance may increase monotonically out of an existing overlap.
     * Reject travel deeper into it; do not trap an actor placed by a peer. */
    return before<limit&&after>before+.000001f&&muc_dot(muc_sub(to,from),muc_sub(nearest,p))>=0;
}
typedef struct {MoonUnitNearby nearby[MU_UNIT_MAX];MoonUnitVector center;float reach;unsigned count,valid,provider;} MUCContactCache;
/* Invalidate before each synchronous consumer continuation. Other providers
 * cannot move/reenter while its core runs on the owning game thread. */
static inline void muc_invalidate(MUCContactCache *cache){cache->valid=0;}
static inline int muc_capsule_clear(MUCContactCache *cache,const MoonUnits *units,const MoonUnitContacts *contacts,unsigned own_provider,MoonUnitVector from,MoonUnitVector to,MoonUnitVector up,float radius,float height){
    MoonUnitVector d=muc_sub(to,from),center=muc_mul(muc_add(from,to),.5f);
    float h=height*.5f-radius;if(h<0)h=0;
    /* L1 bounds the half-segment length without introducing a CRT square root. */
    float reach=(muc_abs(d.x)+muc_abs(d.y)+muc_abs(d.z))*.5f+h+radius;
    if(!units||!cache)return 0;
    MoonUnitVector offset=muc_sub(center,cache->center);
    float bound=muc_abs(offset.x)+muc_abs(offset.y)+muc_abs(offset.z)+reach;
    if(!cache->valid||cache->provider!=own_provider||bound>cache->reach){
        cache->valid=0;cache->reach=reach<16?16:reach;cache->center=center;cache->provider=own_provider;cache->count=0;
        int result=contacts?contacts->nearby(center,cache->reach,own_provider,cache->nearby,MU_UNIT_MAX,&cache->count):
            units->nearby(center,cache->reach,own_provider,0,cache->nearby,MU_UNIT_MAX,&cache->count);
        if(result!=1||cache->count>MU_UNIT_MAX)return 0;
        cache->valid=1;
    }
    for(unsigned i=0;i<cache->count;i++){
        const MoonUnitNearby *p=&cache->nearby[i];
        /* Native contacts/supports already pass through the exact physics
         * adapter. The consumer handles its own actors with its own core. */
        if(p->provider==1||p->provider==own_provider)continue;
        if(!muc_marker_clear(from,to,up,radius,height,p))return 0;
    }return 1;
}
#endif
