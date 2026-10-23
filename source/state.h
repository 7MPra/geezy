#ifndef GEEZY_STATE_H
#define GEEZY_STATE_H
#include <math.h>
#include <stdint.h>
typedef struct { float x,y,z; } Vec;
typedef struct { Vec camera; float phase, roll, aim_x, aim_y, fov, burst, uv_shift; } Pose;
typedef struct {
    Pose live, draw;
    Vec velocity, flow;
    float spin_velocity, burst_velocity, accumulator, target_fov, target_roll, target_aim_x, target_aim_y;
    unsigned tick, sample_tick, flash_ticks, shake_cooldown;
    int shape, cut, slow, frequency, shake_ready;
    uint32_t seed;
} Synth;
static inline float wrap(float x,float span) { return x-span*floorf(x/span+0.5f); }
static inline void sample_pose(Synth *s) { s->draw=s->live; s->sample_tick=0; }
static inline void reset(Synth *s) {
    *s=(Synth){.seed=7,.flow={0,0,-1},.live={.fov=80},.target_fov=80,.shake_ready=1};
    sample_pose(s);
}
static inline void hit(Synth *s,float strength) {
    s->velocity.x+=s->flow.x*600*strength;
    s->velocity.y+=s->flow.y*600*strength;
    s->velocity.z+=s->flow.z*600*strength;
    s->spin_velocity+=((s->cut&1)?-22:22)*strength;
    s->burst_velocity+=32*strength;
    s->cut=(s->cut+1)%4;
    s->target_fov=(s->cut&1)?105:55;
    s->target_roll=(float[]){0,45,-60,90}[s->cut];
    s->target_aim_x=(float[]){0,0.3f,-0.3f,0.15f}[s->cut];
    s->target_aim_y=(float[]){0,-0.2f,0.25f,-0.1f}[s->cut];
    s->flash_ticks=7;

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
        s->live.fov+=(s->target_fov-s->live.fov)*0.28f;
        s->live.roll=wrap(s->live.roll+wrap(s->target_roll-s->live.roll,360)*0.28f,360);
        s->live.aim_x+=(s->target_aim_x-s->live.aim_x)*0.28f;
        s->live.aim_y+=(s->target_aim_y-s->live.aim_y)*0.28f;
        float cruise=slow?2.0f:12.0f;
        s->live.camera.x=wrap(s->live.camera.x+(s->velocity.x+s->flow.x*cruise)/60,96);
        s->live.camera.y=wrap(s->live.camera.y+(s->velocity.y+s->flow.y*cruise)/60,96);
        s->live.camera.z=wrap(s->live.camera.z+(s->velocity.z+s->flow.z*cruise)/60,96);
        s->live.phase=wrap(s->live.phase+(s->spin_velocity+(slow?0.08f:0.35f))/60,6.2831853f);
        s->live.uv_shift=wrap(s->live.uv_shift+(slow?0.02f:0.08f)/60,1);
        s->live.burst=(s->live.burst+s->burst_velocity/60)*0.85f;
        s->velocity.x*=0.85f; s->velocity.y*=0.85f; s->velocity.z*=0.85f;
        s->spin_velocity*=0.85f; s->burst_velocity*=0.85f;
        if(s->flash_ticks) s->flash_ticks--;
        if(s->shake_cooldown) s->shake_cooldown--;
        if(s->sample_tick>=(slow?10u:3u)) sample_pose(s);
    }
}
#endif
