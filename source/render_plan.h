#ifndef GEEZY_RENDER_PLAN_H
#define GEEZY_RENDER_PLAN_H
#include "state.h"
typedef struct { int echo,history,trail; } RenderPlan;
static inline RenderPlan render_plan(const Synth *s) {
    int echo=s->draw.burst>0.5f || s->tension>0.3f;
    return (RenderPlan){echo,echo || (s->mask_pattern==4 && s->mask_env>0.001f),
        !s->slow && s->theme==0 && s->world_mode!=3};
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
