#ifndef GEEZY_STATE_H
#define GEEZY_STATE_H
#include <math.h>
#include <stdint.h>
typedef struct { float x,y,z; } Vec;
typedef struct {
    Vec world_cam, look_at, objects[3];
    float phase, roll, aim_x, aim_y, fov, burst, uv_shift, azimuth, elevation, radius;
} Pose;
typedef struct {
    Pose live, draw, stutter_anchor;
    Vec velocity, flow, stutter_flow;
    float spin_velocity, burst_velocity, accumulator, target_fov, target_roll, target_aim_x, target_aim_y;
    float target_azimuth, target_elevation, azimuth_velocity, elevation_velocity, roll_velocity;
    float bank, tension, overdrive, boost, mask_env, motion_phase, grid_distance;
    unsigned palette_count, tick, sample_tick, flash_ticks, shake_cooldown, stutter_tick, grid_tick;
    int focus_index, shape, motion_mode, gear, grid_axis, cut, mask_pattern, slow, frequency, shake_ready;
    uint32_t seed;
} Synth;
static inline float wrap(float x,float span) { return x-span*floorf(x/span+0.5f); }
static inline float clamp01(float x) { return fmaxf(0,fminf(1,x)); }
static inline float snap_vertex(float x) { return roundf(x*1.5f)/1.5f; }
static inline float dolly_radius(float fov) {
    const float base_radius=16.0f;
    const float k_dolly=base_radius*tanf(80.0f*0.008726646f);
    return k_dolly/tanf(fov*0.008726646f);
}
static inline Vec orbit_eye(const Pose *p) {
    return (Vec){p->look_at.x+p->radius*cosf(p->elevation)*sinf(p->azimuth),
                 p->look_at.y+p->radius*sinf(p->elevation),
                 p->look_at.z+p->radius*cosf(p->elevation)*cosf(p->azimuth)};
}
static inline Vec formation(int index,int variant) {
    static const Vec offsets[3]={{-5,-2,2},{5,3,-1},{1,-1,-5}};
    return offsets[(index+variant)%3];
}
static inline float lattice_axis(int index,float world,float eye) {
    return eye+wrap(index*24-world-eye,192);
}
static inline float lattice_fade(Vec p,Vec eye) {
    float near=sqrtf(p.x*p.x+p.y*p.y+p.z*p.z);
    float x=p.x-eye.x,y=p.y-eye.y,z=p.z-eye.z;
    float far=sqrtf(x*x+y*y+z*z);
    float a=clamp01((near-20)/12),b=clamp01((84-far)/24);
    return a*a*(3-2*a)*b*b*(3-2*b);
}
static inline void sample_pose(Synth *s) {
    s->live.radius=dolly_radius(s->live.fov);
    s->draw=s->live; s->sample_tick=0;
}
static inline void reset(Synth *s) {
    *s=(Synth){.seed=7,.flow={0,0,-1},.live={.fov=80},.target_fov=80,.shake_ready=1,.gear=1,.boost=1,.grid_axis=2,.focus_index=3};
    for(int i=0;i<3;i++) s->live.objects[i]=formation(i,0);
    sample_pose(s);
}
static inline void change_gear(Synth *s,int delta) {
    s->gear+=delta;
    if(s->gear<0) s->gear=0;
    if(s->gear>3) s->gear=3;
}
static inline void next_scene(Synth *s) {
    s->shape=(s->shape+1)%5; s->motion_mode=(s->motion_mode+1)%4;
    s->motion_phase=s->grid_distance=0;
    s->grid_tick=0; s->grid_axis=2;
    s->target_azimuth=wrap(s->target_azimuth+1.8f,6.2831853f);
    s->target_elevation=(s->shape&1)?0.65f:-0.5f;
}
static inline void steer(Synth *s,int x,int y) {
    float len=sqrtf(x*x+y*y+1);
    if((x||y) && (s->flow.x!=x/len || s->flow.y!=y/len)) s->grid_axis=x?0:1;
    s->flow=(Vec){x/len,y/len,-1/len};
}
static inline void hit(Synth *s,float strength) {
    s->velocity.x+=s->flow.x*600*strength;
    s->velocity.y+=s->flow.y*600*strength;
    s->velocity.z+=s->flow.z*600*strength;
    s->spin_velocity+=((s->cut&1)?-22:22)*strength;
    s->burst_velocity+=32*strength;
    s->cut=(s->cut+1)%4;
    s->mask_pattern=(s->mask_pattern+1)%10;
    s->target_fov=(s->cut&1)?105:55;
    s->target_roll=(float[]){0,45,-60,90}[s->cut];
    s->target_aim_x=(float[]){0,0.3f,-0.3f,0.15f}[s->cut];
    s->target_aim_y=(float[]){0,-0.2f,0.25f,-0.1f}[s->cut];
    s->focus_index=(s->focus_index+1)%4;
    s->target_azimuth=wrap(s->target_azimuth+1.5f+0.3f*fminf(strength,4),6.2831853f);
    s->target_elevation=(float[]){-0.65f,0.8f,0.25f,-0.3f}[s->cut];
    s->flash_ticks=12;
}
static inline void palette_next(Synth *s) {
    s->seed=s->seed*1664525u+1013904223u;
    s->frequency=(s->frequency+1)%3;
    s->palette_count++;
}
static inline int shake(Synth *s,float magnitude) {
    if(!isfinite(magnitude)) return 0;
    if(magnitude-1.0f<0.3f) s->shake_ready=1;
    if(magnitude-1.0f<=0.65f || !s->shake_ready || s->shake_cooldown) return 0;
    s->shake_ready=0; s->shake_cooldown=10;
    hit(s,2.5f);
    return 1;
}
static inline void move_camera(Synth *s,float cruise) {
    float impulse=sqrtf(s->velocity.x*s->velocity.x+s->velocity.y*s->velocity.y+s->velocity.z*s->velocity.z);
    float speed=cruise+impulse;
    Vec delta={0,0,0};
    s->motion_phase=wrap(s->motion_phase+(0.4f+speed*0.035f)/60,6.2831853f);
    float p=s->motion_phase;
    if(s->motion_mode==0) {
        delta=(Vec){(s->velocity.x+s->flow.x*cruise)/60,(s->velocity.y+s->flow.y*cruise)/60,(s->velocity.z+s->flow.z*cruise)/60};
    } else if(s->motion_mode==2) {
        s->grid_distance+=speed/60;
        if(++s->grid_tick>=6) {
            // Keep one step below a quarter of the repeating world to retain a visible direction.
            float distance=fminf(s->grid_distance,24);
            if(s->grid_axis==0) delta.x=distance*(s->flow.x<0?-1:1);
            else if(s->grid_axis==1) delta.y=distance*(s->flow.y<0?-1:1);
            else delta.z=-distance;
            s->grid_axis=(s->grid_axis+1)%3; s->grid_tick=0; s->grid_distance=0;
        }
    } else {
        Vec direction;
        if(s->motion_mode==1) direction=(Vec){cosf(p)+s->flow.x*0.5f,sinf(p)+s->flow.y*0.5f,s->flow.z*0.8f};
        else direction=(Vec){1.8f*sinf(p)+s->flow.x*0.3f,1.1f*sinf(p*2)+s->flow.y*0.3f,s->flow.z*0.25f};
        float length=sqrtf(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
        delta=(Vec){direction.x*speed/(60*length),direction.y*speed/(60*length),direction.z*speed/(60*length)};
    }
    s->live.world_cam.x=wrap(s->live.world_cam.x+delta.x,96);
    s->live.world_cam.y=wrap(s->live.world_cam.y+delta.y,96);
    s->live.world_cam.z=wrap(s->live.world_cam.z+delta.z,96);
}
static inline float spring(float pos,float target,float *velocity) {
    float delta=wrap(target-pos,6.2831853f);
    *velocity=*velocity*0.55f+delta*0.16f;
    pos+=*velocity;
    return wrap(pos,6.2831853f);
}
static inline void update_rig(Synth *s) {
    for(int i=0;i<3;i++) {
        Vec target=formation(i,s->cut+s->shape);
        float compression=1-0.65f*s->tension;
        s->live.objects[i].x+=(target.x*compression-s->live.objects[i].x)*0.12f;
        s->live.objects[i].y+=(target.y*compression-s->live.objects[i].y)*0.12f;
        s->live.objects[i].z+=(target.z*compression-s->live.objects[i].z)*0.12f;
    }
    Vec target={0,0,0};
    if(s->focus_index<3) target=s->live.objects[s->focus_index];
    else for(int i=0;i<3;i++) {
        target.x+=s->live.objects[i].x/3; target.y+=s->live.objects[i].y/3; target.z+=s->live.objects[i].z/3;
    }
    target.x+=0.06f*sinf(s->tick*0.011f); target.y+=0.04f*sinf(s->tick*0.017f);
    s->live.look_at.x+=(target.x-s->live.look_at.x)*0.42f;
    s->live.look_at.y+=(target.y-s->live.look_at.y)*0.42f;
    s->live.look_at.z+=(target.z-s->live.look_at.z)*0.42f;
    s->live.azimuth=spring(s->live.azimuth,s->target_azimuth,&s->azimuth_velocity);
    s->live.elevation=spring(s->live.elevation,s->target_elevation,&s->elevation_velocity);
    s->live.elevation=fmaxf(-1.3f,fminf(1.3f,s->live.elevation));
    float roll=s->live.roll*0.0174532925f;
    roll=spring(roll,(s->target_roll+s->bank)*0.0174532925f,&s->roll_velocity);
    s->live.roll=roll*57.2957795f;
    s->live.radius=dolly_radius(s->live.fov);
}
static inline void advance(Synth *s,float dt,int slow,int boosting,int accent) {
    s->accumulator+=fmaxf(0,fminf(dt,0.25f));
    if(slow && !s->slow) {
        s->stutter_anchor=s->draw; s->stutter_flow=s->flow; s->stutter_tick=0;
        s->live=s->draw;
    } else if(!slow && s->slow) {
        s->live=s->draw;
        hit(s,1.2f+s->tension*2.8f);
        s->tension=0; s->overdrive=1;
        s->live.fov=125; sample_pose(s);
    }
    s->slow=slow;
    while(s->accumulator>=1.0f/60) {
        s->accumulator-=1.0f/60; s->tick++; s->sample_tick++;
        s->boost+=((boosting?5.0f:1.0f)-s->boost)*(boosting?0.08f:0.12f);
        if(slow) s->tension=clamp01(s->tension+1.0f/120);
        if(!slow) {
            float whip=s->motion_mode==3?0.2f*sinf(s->motion_phase*2):0;
            float aim=fmaxf(-0.35f,fminf(0.35f,s->target_aim_x+whip));
            s->live.fov+=(s->target_fov-s->live.fov)*0.28f;

            s->live.aim_x+=(aim-s->live.aim_x)*0.28f;
            s->live.aim_y+=(s->target_aim_y-s->live.aim_y)*0.28f;
            float cruise=(float[]){3,12,36,72}[s->gear]*s->boost;
            move_camera(s,cruise);
            s->live.phase=wrap(s->live.phase+(s->spin_velocity+0.15f+s->gear*0.2f)/60,6.2831853f);
            s->live.uv_shift=wrap(s->live.uv_shift+(0.04f+s->gear*0.04f)/60,1);
        }
        s->live.burst=(s->live.burst+s->burst_velocity/60)*0.86f;
        s->velocity.x*=0.85f; s->velocity.y*=0.85f; s->velocity.z*=0.85f;
        s->spin_velocity*=0.85f; s->burst_velocity*=0.86f;
        if(accent || s->live.burst>1.2f) s->overdrive=1;
        else s->overdrive+=((float[]){0,0.18f,0.72f,1}[s->gear]-s->overdrive)*0.1f;
        float mask_target=(accent || s->flash_ticks)?1:0;
        s->mask_env+=(mask_target-s->mask_env)*(mask_target?0.3f:0.18f);
        if(s->mask_env<0.001f) s->mask_env=0;
        if(s->flash_ticks) s->flash_ticks--;
        if(s->shake_cooldown) s->shake_cooldown--;
        if(slow) {
            unsigned step=(++s->stutter_tick/8)%4;
            s->live.world_cam=s->stutter_anchor.world_cam;
            s->live.world_cam.x=wrap(s->live.world_cam.x+s->stutter_flow.x*step*0.9f,96);
            s->live.world_cam.y=wrap(s->live.world_cam.y+s->stutter_flow.y*step*0.9f,96);
            s->live.world_cam.z=wrap(s->live.world_cam.z+s->stutter_flow.z*step*0.9f,96);
            s->live.phase=wrap(s->stutter_anchor.phase+step*0.12f,6.2831853f);
            s->live.fov=80-s->tension*42;
            s->live.uv_shift=wrap(s->stutter_anchor.uv_shift+s->tension*s->stutter_tick*0.035f,1);
        }
        // The angular spring stays crisp; its bank/impact target carries the slower tail.
        s->bank=s->bank*0.86f+(boosting?-s->flow.x*22:0)*0.14f;
        update_rig(s);
        s->target_roll*=0.86f;
        if(slow ? s->stutter_tick%8==0 : s->sample_tick>=3) sample_pose(s);
    }
}
#endif
