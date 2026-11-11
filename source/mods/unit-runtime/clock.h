#ifndef MOON_UNIT_CLOCK_H
#define MOON_UNIT_CLOCK_H
#define MU_STEP .05f
#define MU_CATCHUP 4u
typedef struct {unsigned long long frame,ticks;double pending;unsigned initialized;} MUClock;
static inline unsigned muc_steps(MUClock *c,unsigned long long frame,float dt){
    if(!c||!(dt>0)||dt>10||dt!=dt||(c->initialized&&frame<=c->frame))return 0;
    c->initialized=1;c->frame=frame;c->pending+=(double)dt;
    unsigned n=0;while(c->pending+1e-9>=.05&&n<MU_CATCHUP){c->pending-=.05;n++;}
    c->ticks+=n;return n;
}
#endif
