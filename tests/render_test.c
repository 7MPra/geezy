#include <assert.h>
#include "../source/render_plan.h"
int main(void) {
    Synth s; reset(&s);
    for(int theme=0;theme<THEME_COUNT;theme++) for(int world=0;world<5;world++)
    for(int slow=0;slow<2;slow++) for(int pattern=0;pattern<8;pattern++) {
        s.theme=theme; s.world_mode=world; s.slow=slow; s.mask_pattern=pattern;
        s.draw.burst=s.tension=s.mask_env=0;
        RenderPlan p=render_plan(&s,0);
        assert(!p.echo && !p.history);
        assert(p.trail==(!slow && theme==0));
        s.mask_env=0.002f; p=render_plan(&s,0); assert(p.history==(pattern==4));
        s.mask_env=0.001f; p=render_plan(&s,0); assert(!p.history);
        s.draw.burst=0.5f; s.tension=0.3f; p=render_plan(&s,0); assert(!p.echo);
        s.draw.burst=0.501f; p=render_plan(&s,0); assert(p.echo && p.history);
        s.draw.burst=0; s.tension=0.301f; p=render_plan(&s,0); assert(p.echo && p.history);
    }
    RenderStats stats={0};
    for(int i=0;i<60;i++) {
        unsigned us[TIME_COUNT]={1000,2000,3000,4000,i==59?10000:5000};
        render_stats_add(&stats,us,20000,1+i%2);
        if(i<59) assert(stats.interval_ms==0);
    }
    assert(stats.count==0 && stats.interval_ms==20 && stats.peak_ms==20 && stats.copies==1.5f);
    assert(stats.ms[TIME_SCENE]==2 && fabsf(stats.ms[TIME_WAIT]-5.083333f)<0.0001f);
    for(int i=0;i<60;i++) {
        unsigned us[TIME_COUNT]={0}; render_stats_add(&stats,us,16667,0);
    }
    assert(stats.peak_ms==0 && stats.copies==0 && stats.ms[TIME_SCENE]==0);
    return 0;
}
