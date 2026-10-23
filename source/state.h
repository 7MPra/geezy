#ifndef GEEZY_STATE_H
#define GEEZY_STATE_H
#include <math.h>
#include <stdint.h>
typedef struct { float x,y,z; } Vec;
typedef struct { Vec camera; float phase, roll, aim_x, aim_y, fov, burst; } Pose;
typedef struct {
    Pose live, draw;
    Vec velocity, flow;
    float spin_velocity, burst_velocity, accumulator, charge;
    unsigned tick, sample_tick, flash_ticks;
    int shape, cut, slow, dirty;
    uint32_t seed;
} Synth;
static inline float wrap(float x,float span) { return x-span*floorf(x/span+0.5f); }
static inline void sample_pose(Synth *s) { s->draw=s->live; s->sample_tick=0; s->dirty=0; }
static inline void reset(Synth *s) {
    *s=(Synth){.seed=7,.flow={0,0,-1},.live={.fov=95}};
    sample_pose(s);
}
static inline void hit(Synth *s) {
    s->velocity.x+=s->flow.x*420;
    s->velocity.y+=s->flow.y*420;
    s->velocity.z+=s->flow.z*420;
    s->spin_velocity+=(s->cut&1)?-22:22;
    s->burst_velocity+=32;
    s->cut=(s->cut+1)%4;
    s->live.fov=(s->cut&1)?135:35;
    s->live.roll=(float[]){0,67,-105,180}[s->cut];
    s->live.aim_x=(float[]){0,0.9f,-0.8f,0.35f}[s->cut];
    s->live.aim_y=(float[]){0,-0.55f,0.6f,-0.2f}[s->cut];
    s->flash_ticks=7;
    // Advance one impulse-sized distance immediately; the remaining tail uses fixed steps.
    s->live.camera.x+=s->flow.x*5;
    s->live.camera.y+=s->flow.y*5;
    s->live.camera.z+=s->flow.z*5;
    s->live.burst=2;
    sample_pose(s);
}
static inline void palette_next(Synth *s) { s->seed=s->seed*1664525u+1013904223u; }
static inline void advance(Synth *s,float dt,int slow) {
    s->accumulator+=fmaxf(0,fminf(dt,0.25f));
    if(s->slow!=slow) { s->slow=slow; sample_pose(s); }
    while(s->accumulator>=1.0f/60) {
        s->accumulator-=1.0f/60; s->tick++; s->sample_tick++;
        s->live.camera.x=wrap(s->live.camera.x+s->velocity.x/60,96);
        s->live.camera.y=wrap(s->live.camera.y+s->velocity.y/60,96);
        s->live.camera.z=wrap(s->live.camera.z+s->velocity.z/60,96);
        s->live.phase=wrap(s->live.phase+s->spin_velocity/60,6.2831853f);
        s->live.burst=(s->live.burst+s->burst_velocity/60)*0.72f;
        s->velocity.x*=0.72f; s->velocity.y*=0.72f; s->velocity.z*=0.72f;
        s->spin_velocity*=0.72f; s->burst_velocity*=0.72f;
        s->charge+=((slow?1.0f:0.0f)-s->charge)*0.08f;
        if(s->flash_ticks) s->flash_ticks--;
        if(s->sample_tick>=(slow?15u:5u)||s->dirty) sample_pose(s);
    }
}
#endif
