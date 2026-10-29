#ifndef GEEZY_RENDER_PLAN_H
#define GEEZY_RENDER_PLAN_H
#include "state.h"
enum { HOLD_NONE,HOLD_BLACK,HOLD_WHITE,HOLD_FREEZE,HOLD_STROBE };
typedef struct { int echo,history,trail,post_history,capture_freeze,hold; } RenderPlan;
static inline RenderPlan render_plan(const Synth *s,int freeze_ready) {
    unsigned fx=s->master_fx;
    int hold=(fx&FX_BLACKOUT)?HOLD_BLACK:(fx&FX_WHITEOUT)?HOLD_WHITE:
        (fx&FX_FREEZE)?(freeze_ready?HOLD_FREEZE:HOLD_NONE):(fx&FX_STROBE_KILL)?HOLD_STROBE:HOLD_NONE;
    if(hold) return (RenderPlan){.hold=hold};
    int echo=s->draw.burst>0.5f || s->tension>0.3f;
    return (RenderPlan){echo,echo || (s->mask_pattern==4 && s->mask_env>0.001f),
        !s->slow && s->theme==0 && !(fx&FX_FREEZE),
        !!(fx&(FX_MIRROR_X|FX_KALEIDO_4|FX_MOSAIC)),!!(fx&FX_FREEZE),HOLD_NONE};
}
static inline void master_uv(unsigned fx,float *u,float *v) {
    if(fx&(FX_MIRROR_X|FX_KALEIDO_4)) *u=fminf(*u,1-*u);
    if(fx&FX_KALEIDO_4) *v=fminf(*v,1-*v);
}
enum { TIME_INPUT,TIME_SCENE,TIME_EFFECTS,TIME_UI,TIME_WAIT,TIME_COUNT };
typedef struct {
    unsigned count,peak,copy_sum;
    uint64_t sum[TIME_COUNT],interval_sum;
    float ms[TIME_COUNT],interval_ms,peak_ms,copies;
} RenderStats;
static inline void render_stats_add(RenderStats *s,const unsigned us[TIME_COUNT],unsigned interval,unsigned copies) {
    unsigned total=0;
    for(int i=0;i<TIME_COUNT;i++) { s->sum[i]+=us[i]; total+=us[i]; }
    if(total>s->peak) s->peak=total;
    s->interval_sum+=interval; s->copy_sum+=copies;
    if(++s->count<60) return;
    for(int i=0;i<TIME_COUNT;i++) { s->ms[i]=s->sum[i]/60000.0f; s->sum[i]=0; }
    s->interval_ms=s->interval_sum/60000.0f; s->peak_ms=s->peak/1000.0f; s->copies=s->copy_sum/60.0f;
    s->count=s->peak=s->copy_sum=0; s->interval_sum=0;
}
#endif
