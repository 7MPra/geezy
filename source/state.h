#ifndef GEEZY_STATE_H
#define GEEZY_STATE_H
#include <math.h>
#include <stdint.h>
typedef struct { float x,y,z; } Vec;
typedef struct { Vec camera; float phase, roll, aim_x, aim_y, fov, burst, uv_shift; } Pose;
typedef struct {
    Pose live, draw;
    Vec velocity, flow;
    float spin_velocity, burst_velocity, accumulator, charge;
    unsigned tick, sample_tick, flash_ticks, shake_cooldown;
    int shape, cut, slow, dirty, frequency, shake_ready;
    uint32_t seed;
} Synth;
static inline float wrap(float x,float span) { return x-span*floorf(x/span+0.5f); }
static inline void sample_pose(Synth *s) { s->draw=s->live; s->sample_tick=0; s->dirty=0; }
static inline void reset(Synth *s) {
    *s=(Synth){.seed=7,.flow={0,0,-1},.live={.fov=95},.shake_ready=1};
    sample_pose(s);
}
static inline void hit(Synth *s,float strength) {
    s->velocity.x+=s->flow.x*420*strength;
    s->velocity.y+=s->flow.y*420*strength;
    s->velocity.z+=s->flow.z*420*strength;
    s->spin_velocity+=((s->cut&1)?-22:22)*strength;
    s->burst_velocity+=32*strength;
    s->cut=(s->cut+1)%4;
    s->live.fov=(s->cut&1)?135:35;
    s->live.roll=(float[]){0,67,-105,180}[s->cut];
    s->live.aim_x=(float[]){0,0.9f,-0.8f,0.35f}[s->cut];
    s->live.aim_y=(float[]){0,-0.55f,0.6f,-0.2f}[s->cut];
    s->flash_ticks=7;
    // Advance one impulse-sized distance immediately; the remaining tail uses fixed steps.
    s->live.camera.x=wrap(s->live.camera.x+s->flow.x*5*strength,96);
    s->live.camera.y=wrap(s->live.camera.y+s->flow.y*5*strength,96);
    s->live.camera.z=wrap(s->live.camera.z+s->flow.z*5*strength,96);
    s->live.burst=2*strength;
    sample_pose(s);
}
static inline void palette_next(Synth *s) {
    s->seed=s->seed*1664525u+1013904223u;
    s->frequency=(s->frequency+1)%3;
}
static inline int shake(Synth *s,float magnitude) {
    if(!isfinite(magnitude)) return 0;
    // Rearm below the trigger threshold so one sustained acceleration is one gesture.
    if(magnitude-1.0f<0.3f) s->shake_ready=1;
    if(magnitude-1.0f<=0.65f || !s->shake_ready || s->shake_cooldown) return 0;
    s->shake_ready=0; s->shake_cooldown=10;
    hit(s,2.5f);
    return 1;
}
static inline void advance(Synth *s,float dt,int slow) {
    s->accumulator+=fmaxf(0,fminf(dt,0.25f));
    if(s->slow!=slow) { s->slow=slow; sample_pose(s); }
    while(s->accumulator>=1.0f/60) {
        s->accumulator-=1.0f/60; s->tick++; s->sample_tick++;
        if(s->tick%20==0) {
            s->velocity.x+=s->flow.x*55;
            s->velocity.y+=s->flow.y*55;
            s->velocity.z+=s->flow.z*55;
            s->spin_velocity+=(s->tick/20&1)?4:-4;
            s->live.roll=wrap(s->live.roll+((s->tick/20&1)?12:-19),360);
            s->live.uv_shift=wrap(s->live.uv_shift+0.31f,1);
        }
        float cruise=slow?25:135;
        s->live.camera.x=wrap(s->live.camera.x+(s->velocity.x+s->flow.x*cruise)/60,96);
        s->live.camera.y=wrap(s->live.camera.y+(s->velocity.y+s->flow.y*cruise)/60,96);
        s->live.camera.z=wrap(s->live.camera.z+(s->velocity.z+s->flow.z*cruise)/60,96);
        s->live.phase=wrap(s->live.phase+(s->spin_velocity+(slow?0.4f:1.8f))/60,6.2831853f);
        s->live.uv_shift=wrap(s->live.uv_shift+(slow?0.2f:1.1f)/60,1);
        s->live.burst=(s->live.burst+s->burst_velocity/60)*0.72f;
        s->velocity.x*=0.72f; s->velocity.y*=0.72f; s->velocity.z*=0.72f;
        s->spin_velocity*=0.72f; s->burst_velocity*=0.72f;
        s->charge+=((slow?1.0f:0.0f)-s->charge)*0.08f;
        if(s->flash_ticks) s->flash_ticks--;
        if(s->shake_cooldown) s->shake_cooldown--;
        if(s->sample_tick>=(slow?12u:4u)||s->dirty) sample_pose(s);
    }
}
#endif
