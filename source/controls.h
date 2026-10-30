#ifndef GEEZY_CONTROLS_H
#define GEEZY_CONTROLS_H
#include "state.h"
enum {
    KEY_A=1, KEY_B=2, KEY_ONE=4, KEY_TWO=8,
    KEY_UP=16, KEY_RIGHT=32, KEY_DOWN=64, KEY_LEFT=128,
    KEY_PLUS=256, KEY_MINUS=512,
    KEY_ARROWS=KEY_UP|KEY_RIGHT|KEY_DOWN|KEY_LEFT, KEY_MODS=KEY_ONE|KEY_TWO
};
static inline int steering_buttons(unsigned held) { return (held&KEY_ARROWS) && !(held&KEY_MODS); }
static inline unsigned remote_master_fx(unsigned held) {
    if((held&KEY_MODS)!=KEY_MODS) return 0;
    return ((held&KEY_A)?FX_FREEZE:0)|((held&KEY_B)?FX_KALEIDO_4:0);
}
static inline void remix(Synth *s) {
    palette_next(s);
    uint32_t recipe=s->seed;
    s->world_mode=(recipe>>3)%5;
    s->shape=(recipe>>1)%5; s->skin=(recipe>>5)&3; s->layout=(recipe>>8)&3;
    s->backdrop=(recipe>>12)&3; s->reflection=(recipe>>16)&3;
    s->beam_style=(recipe>>28)&3;
    s->theme=(recipe>>6)%THEME_COUNT; s->palette_bank=(recipe>>10)&15;
    s->harmony_mode=s->palette_bank/4; s->bank_seed[s->palette_bank]=s->seed;
    s->motion_mode=(recipe>>20)&3; s->mask_pattern=(recipe>>24)%8;
    s->motion_phase=s->grid_distance=0; s->grid_tick=0;
    hit(s,1.2f);
}
static inline void theme_preset(Synth *s,int preset) {
    if(preset%4>=THEME_COUNT) return;
    s->theme=preset%4;
    s->skin=preset<4?0:(int[]){3,2,1,0}[preset-4];
    s->reflection=preset<4?0:(int[]){2,3,0,1}[preset-4];
}
static inline void space_preset(Synth *s,int preset) {
    s->backdrop=(int[]){0,1,2,0,0,3,1,2}[preset];
    s->beam_style=(int[]){0,1,0,1,3,2,2,3}[preset];
}
static inline void object_preset(Synth *s,int preset) {
    s->shape=(int[]){0,1,2,3,4,0,2,4}[preset];
    s->layout=(int[]){0,1,2,3,0,2,1,3}[preset];
}
static inline void harmony_bank(Synth *s,int bank) {
    if(s->palette_bank==bank) s->bank_seed[bank]+=977u;
    s->palette_bank=bank; s->harmony_mode=bank/4; s->seed=s->bank_seed[bank];
    s->palette_count++;
}
static inline void camera_cut(Synth *s) {
    hit(s,2.2f); s->live.azimuth=s->target_azimuth; s->live.elevation=s->target_elevation; sample_pose(s);
}
enum { CLASS_A=1,CLASS_B=2,CLASS_X=4,CLASS_Y=8,CLASS_L=16,CLASS_R=32,
       CLASS_ZL=64,CLASS_ZR=128,CLASS_UP=256,CLASS_RIGHT=512,CLASS_DOWN=1024,CLASS_LEFT=2048,
       CLASS_PLUS=4096,CLASS_MINUS=8192 };
static inline unsigned classic_master_fx(unsigned held) {
    if(!(held&CLASS_ZR)) return 0;
    const unsigned keys[8]={CLASS_A,CLASS_B,CLASS_X,CLASS_Y,CLASS_UP,CLASS_RIGHT,CLASS_DOWN,CLASS_LEFT};
    unsigned fx=0; for(int i=0;i<8;i++) if(held&keys[i]) fx|=1u<<i;
    return fx;
}
static inline int classic_controls(Synth *s,unsigned held,float x,float y) {
    unsigned previous=s->classic_held,down=held&~previous; s->classic_held=held;
    int dirty=0;
    if((held&(CLASS_PLUS|CLASS_MINUS))==(CLASS_PLUS|CLASS_MINUS)) { reset(s); s->classic_held=held; return 1; }
    s->classic_fx_latch&=held;
    if(down&CLASS_ZR) s->classic_zr_pending=!(held&CLASS_R);
    if(held&CLASS_ZR) {
        unsigned chord=held&(CLASS_A|CLASS_B|CLASS_X|CLASS_Y|CLASS_UP|CLASS_RIGHT|CLASS_DOWN|CLASS_LEFT);
        s->classic_fx_latch|=chord;
        if(chord) s->classic_zr_pending=0;
    } else if(previous&CLASS_ZR) {
        if(s->classic_zr_pending) { harmony_bank(s,(s->palette_bank+1)%16); dirty=1; }
        s->classic_zr_pending=0;
    }
    down&=~s->classic_fx_latch;
    if(down&CLASS_X) { if(held&CLASS_L) s->layout=(s->layout+1)%4; else if(held&CLASS_R) camera_cut(s); else { palette_next(s); dirty=1; } }
    if((down&CLASS_Y) && (held&(CLASS_L|CLASS_R))==(CLASS_L|CLASS_R)) { remix(s); dirty=1; }
    else if(down&CLASS_Y) {
        if(held&CLASS_L) s->world_mode=(s->world_mode+1)%5;
        else { if(held&CLASS_R) s->motion_mode=(s->motion_mode+1)%4; else next_scene(s); hit(s,1); }
    }
    if(down&CLASS_B) s->mask_pattern=(s->mask_pattern+1)%8;
    if((down&CLASS_ZL) && !(held&CLASS_R)) { s->theme=(s->theme+1)%THEME_COUNT; dirty=1; }
    if(down&CLASS_PLUS) { if(held&CLASS_L) s->hud_visible=!s->hud_visible; else change_gear(s,1); }
    if(down&CLASS_MINUS) change_gear(s,-1);
    const unsigned arrows[4]={CLASS_UP,CLASS_RIGHT,CLASS_DOWN,CLASS_LEFT};
    for(int i=0;i<4;i++) if(down&arrows[i]) {
        if(held&CLASS_L) { theme_preset(s,i+((held&CLASS_R)?4:0)); dirty=1; }
        else if(held&CLASS_R) s->mask_pattern=i+((held&CLASS_ZL)?4:0);
        else { Synth preset=*s; space_preset(&preset,i);
            space_preset(s,i+((preset.backdrop==s->backdrop && preset.beam_style==s->beam_style)?4:0)); }
    }
    if(isfinite(x) && isfinite(y) && x*x+y*y>0.04f) {
        steer(s,x,y);
        if(!(previous&0x80000000u)) hit(s,1);
        s->classic_held|=0x80000000u;
    }
    return dirty;
}
// 1/2 taps commit on release so a chord never also changes palette or scene.
static inline int controls(Synth *s,unsigned held) {
    s->remote_fx_latch&=held;
    if(remote_master_fx(held)) {
        s->remote_fx_latch|=held&(KEY_MODS|KEY_A|KEY_B);
        s->pending_taps=0; s->remix_pending=0;
    }
    held&=~s->remote_fx_latch;
    unsigned down=held&~s->held_buttons,up=s->held_buttons&~held;
    s->held_buttons=held;
    if((held&(KEY_PLUS|KEY_MINUS))==(KEY_PLUS|KEY_MINUS)) {
        reset(s); s->held_buttons=held; return 1;
    }
    if(down&KEY_B) s->mask_pattern=(s->mask_pattern+1)%8;
    s->pending_taps|=down&KEY_MODS;
    if((held&(KEY_ONE|KEY_PLUS))==(KEY_ONE|KEY_PLUS)) {
        s->pending_taps&=~KEY_ONE; s->remix_pending=0;
        if(down&(KEY_ONE|KEY_PLUS)) s->hud_visible=!s->hud_visible;
        return 0;
    }
    if((held&(KEY_TWO|KEY_PLUS))==(KEY_TWO|KEY_PLUS)) {
        s->pending_taps&=~KEY_TWO; s->remix_pending=0;
        if(down&(KEY_TWO|KEY_PLUS)) { s->theme=(s->theme+1)%THEME_COUNT; return 1; }
        return 0;
    }
    if((held&(KEY_ONE|KEY_MINUS))==(KEY_ONE|KEY_MINUS)) {
        s->pending_taps&=~KEY_ONE; s->remix_pending=0;
        if(down&(KEY_ONE|KEY_MINUS)) { harmony_bank(s,(s->palette_bank+1)%16); return 1; }
        return 0;
    }
    if((held&(KEY_TWO|KEY_MINUS))==(KEY_TWO|KEY_MINUS)) {
        s->pending_taps&=~KEY_TWO; s->remix_pending=0;
        if(down&(KEY_TWO|KEY_MINUS)) s->world_mode=(s->world_mode+1)%5;
        return 0;
    }
    unsigned mods=held&KEY_MODS,arrows=held&KEY_ARROWS;
    int direction=arrows==KEY_UP?0:arrows==KEY_RIGHT?1:arrows==KEY_DOWN?2:arrows==KEY_LEFT?3:-1;
    int dirty=0,impulse=0;
    if(mods==KEY_MODS) {
        s->pending_taps=0;
        if(down&KEY_MODS) s->remix_pending=1;
    }
    if(mods && arrows) {
        s->pending_taps&=~mods; s->remix_pending=0;
        int diagonal=arrows==(KEY_UP|KEY_LEFT)?0:arrows==(KEY_UP|KEY_RIGHT)?1:
                     arrows==(KEY_DOWN|KEY_RIGHT)?2:arrows==(KEY_DOWN|KEY_LEFT)?3:-1;
        if(mods==KEY_MODS && diagonal>=0 && (down&(mods|KEY_ARROWS))) {
            s->beam_style=diagonal;
            s->backdrop=0; // Entering the diagonal in either order must select the same background.
        } else if(direction>=0 && (down&(mods|KEY_ARROWS))) {
            if(mods==KEY_MODS) s->backdrop=direction;
            else if(mods==KEY_ONE) s->skin=direction;
            else s->layout=direction;
        }
    } else if(mods!=KEY_MODS && mods && (held&(KEY_A|KEY_B))) {
        s->pending_taps&=~mods; s->remix_pending=0;
        unsigned modifier=held&KEY_B?KEY_B:KEY_A;
        if(down&(mods|modifier)) {
            if(modifier==KEY_B && mods==KEY_ONE) {
                if(!(down&KEY_B)) s->mask_pattern=(s->mask_pattern+1)%8;
            }
            else if(modifier==KEY_B) {
                s->motion_mode=(s->motion_mode+1)%4;
                s->motion_phase=s->grid_distance=0; s->grid_tick=0;
            } else if(mods==KEY_ONE) { s->reflection=(s->reflection+1)%4; dirty=1; }
            else s->shape=(s->shape+1)%5;
        }
    }
    if(!mods && s->remix_pending) {
        remix(s); s->remix_pending=0; dirty=1;
    }
    unsigned taps=up&s->pending_taps;
    s->pending_taps&=~up;
    if(taps&KEY_ONE) { palette_next(s); dirty=1; impulse=1; }
    if(taps&KEY_TWO) { next_scene(s); impulse=1; }
    if(steering_buttons(held)) {
        steer(s,!!(held&KEY_RIGHT)-!!(held&KEY_LEFT),!!(held&KEY_UP)-!!(held&KEY_DOWN));
        if(down&KEY_ARROWS) impulse=1;
    }
    if((down&KEY_B) && !mods) impulse=1;
    if(impulse) hit(s,1);
    if(down&KEY_PLUS) change_gear(s,1);
    if(down&KEY_MINUS) change_gear(s,-1);
    return dirty;
}
// Dual-hand chords use B+A/arrows locally, or one modifier on the other hand.
typedef struct {
    unsigned held[2],latch[2],pending[2],performance,fx;
    int connected,ready[2];
    float cooldown[2],window[2];
} DualInput;
static inline unsigned dual_master_fx(unsigned left,unsigned right) {
    unsigned fx=0;
    if(right&KEY_B) {
        if(right&KEY_UP) fx|=FX_KALEIDO_4;
        if(right&KEY_RIGHT) fx|=FX_MIRROR_X;
        if(right&KEY_DOWN) fx|=FX_FREEZE;
        if(right&KEY_LEFT) fx|=FX_INVERT;
        if(right&KEY_A) fx|=FX_MOSAIC;
        if(!fx) fx=FX_KALEIDO_4;
    }
    if(left&KEY_B) {
        if(right&KEY_ONE) fx|=FX_BLACKOUT;
        if(right&KEY_TWO) fx|=FX_WHITEOUT;
        if(right&KEY_PLUS) fx|=FX_STROBE_KILL;
    }
    return fx;
}
static inline int dual_controls(Synth *s,DualInput *input,unsigned left,unsigned right,int connected) {
    if(connected!=input->connected) {
        *input=(DualInput){.connected=connected,.ready={1,1}};
        s->held_buttons=s->pending_taps=s->remix_pending=s->remote_fx_latch=0;
    }
    if(!connected) return 0;
    unsigned held[2]={left,right},down[2],up[2];
    for(int h=0;h<2;h++) {
        down[h]=held[h]&~input->held[h]; up[h]=input->held[h]&~held[h];
        input->held[h]=held[h]; input->latch[h]&=held[h];
        input->pending[h]|=down[h]&KEY_MODS;
    }
    int dirty=0;
    const unsigned arrows[4]={KEY_UP,KEY_RIGHT,KEY_DOWN,KEY_LEFT};
    if(left&KEY_B) {
        input->latch[0]|=left&(KEY_ARROWS|KEY_A);
        input->latch[1]|=right&(KEY_ONE|KEY_TWO|KEY_PLUS);
        for(int i=0;i<4;i++) if((left&arrows[i]) && (down[0]&(KEY_B|arrows[i]))) {
            s->backdrop=i==3?0:i;
            if(i==0 || i==3) s->beam_style=i;
        }
        if((left&KEY_A) && (down[0]&(KEY_B|KEY_A))) s->mask_pattern=(s->mask_pattern+1)%8;
        else if(down[0]&KEY_B) s->mask_pattern=(s->mask_pattern+1)%8;
    }
    if(right&KEY_B) input->latch[1]|=right&(KEY_ARROWS|KEY_A);
    // Each modifier lives on the opposite hand from its command button.
    for(int h=0;h<2;h++) {
        int other=1-h;
        unsigned mod=held[h]&KEY_MODS;
        if((mod!=KEY_ONE && mod!=KEY_TWO) || (held[0]&KEY_B) || (held[1]&KEY_B)) continue;
        unsigned command=held[other]&(KEY_ARROWS|KEY_A);
        if(!command) continue;
        input->latch[h]|=mod; input->latch[other]|=command;
        for(int i=0;i<4;i++) if((command&arrows[i]) && ((down[h]&mod)||(down[other]&arrows[i]))) {
            if(h==0 && mod==KEY_ONE) { s->beam_style=i; s->backdrop=0; }
            else if(h==0) s->skin=i;
            else if(mod==KEY_ONE) s->backdrop=i;
            else { s->reflection=i; dirty=1; }
        }
        if((command&KEY_A) && ((down[h]&mod)||(down[other]&KEY_A))) {
            if(h==0 && mod==KEY_ONE) { remix(s); dirty=1; }
            else if(h==0) { harmony_bank(s,(s->palette_bank+1)%16); dirty=1; }
            else if(mod==KEY_ONE) s->hud_visible=!s->hud_visible;
            else s->mask_pattern=(s->mask_pattern+1)%8;
        }
    }
    if((left&KEY_MINUS) && (right&KEY_MINUS)) {
        if((down[0]|down[1])&KEY_MINUS) { reset(s); dirty=1; }
        input->latch[0]|=KEY_MINUS; input->latch[1]|=KEY_MINUS;
    }
    for(int h=0;h<2;h++) {
        input->pending[h]&=~input->latch[h];
        unsigned taps=up[h]&input->pending[h]; input->pending[h]&=~up[h];
        if(taps&KEY_ONE) { if(h==0) { palette_next(s); dirty=1; } else s->world_mode=(s->world_mode+1)%5; }
        if(taps&KEY_TWO) { if(h==0) s->shape=(s->shape+1)%5; else { s->theme=(s->theme+1)%THEME_COUNT; dirty=1; } }
        down[h]&=~input->latch[h];
        if(down[h]&KEY_PLUS) change_gear(s,1);
        if(down[h]&KEY_MINUS) change_gear(s,-1);
    }
    if(down[1]&KEY_A) camera_cut(s);
    if(down[1]&KEY_UP) s->shape=(s->shape+1)%5;
    if(down[1]&KEY_RIGHT) { palette_next(s); dirty=1; }
    if(down[1]&KEY_DOWN) { s->motion_mode=(s->motion_mode+1)%4; s->motion_phase=s->grid_distance=0; s->grid_tick=0; s->grid_axis=2; }
    if(down[1]&KEY_LEFT) s->layout=(s->layout+1)%4;
    input->performance=left&~input->latch[0];
    if(input->performance&KEY_ARROWS) {
        steer(s,!!(input->performance&KEY_RIGHT)-!!(input->performance&KEY_LEFT),
                !!(input->performance&KEY_UP)-!!(input->performance&KEY_DOWN));
        if(down[0]&KEY_ARROWS) hit(s,1);
    }
    input->fx=dual_master_fx(left,right);
    return dirty;
}
static inline int remote_gestures(Synth *s,DualInput *input,const Vec gf[2],const int valid[2],
                                  const int held_b[2],float dt) {
    int hands=input->connected?2:1,twists=0,event[2]={0,0}; float tilt=0;
    dt=fmaxf(0,fminf(dt,0.1f));
    for(int i=0;i<hands;i++) {
        input->cooldown[i]=fmaxf(0,input->cooldown[i]-dt);
        input->window[i]=fmaxf(0,input->window[i]-dt);
        if(!valid[i] || !isfinite(gf[i].x) || !isfinite(gf[i].y) || !isfinite(gf[i].z)) continue;
        if(held_b[i]) { tilt+=fmaxf(-1,fminf(1,gf[i].x)); twists++; }
        float magnitude=sqrtf(gf[i].x*gf[i].x+gf[i].y*gf[i].y+gf[i].z*gf[i].z);
        if(hands==1) { shake(s,magnitude); continue; }
        if(magnitude<1.3f) input->ready[i]=1;
        if(magnitude>1.65f && input->ready[i] && input->cooldown[i]==0) {
            event[i]=1; input->ready[i]=0; input->cooldown[i]=0.18f; input->window[i]=0.12f;
        }
    }
    s->wrist_target=twists?tilt/twists:0;
    if(hands==2 && (event[0]||event[1])) {
        if((input->window[0]>0 && input->window[1]>0) || (held_b[0]&&held_b[1])) {
            remix(s); hit(s,3); input->window[0]=input->window[1]=0;
            input->cooldown[0]=input->cooldown[1]=0.3f; return 1;
        }
        hit(s,2.5f);
    }
    return 0;
}

#endif
