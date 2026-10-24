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
static inline void remix(Synth *s) {
    palette_next(s);
    uint32_t recipe=s->seed;
    s->shape=(recipe>>1)%5; s->skin=(recipe>>5)&3; s->layout=(recipe>>8)&3;
    s->backdrop=(recipe>>12)&3; s->reflection=(recipe>>16)&3;
    s->motion_mode=(recipe>>20)&3; s->mask_pattern=(recipe>>24)%6;
    s->motion_phase=s->grid_distance=0; s->grid_tick=0;
    hit(s,1.2f);
}
// 1/2 taps commit on release so a chord never also changes palette or scene.
static inline int controls(Synth *s,unsigned held) {
    unsigned down=held&~s->held_buttons,up=s->held_buttons&~held;
    s->held_buttons=held;
    if((held&(KEY_PLUS|KEY_MINUS))==(KEY_PLUS|KEY_MINUS)) {
        reset(s); s->held_buttons=held; return 1;
    }
    s->pending_taps|=down&KEY_MODS;
    unsigned mods=held&KEY_MODS,arrows=held&KEY_ARROWS;
    int direction=arrows==KEY_UP?0:arrows==KEY_RIGHT?1:arrows==KEY_DOWN?2:arrows==KEY_LEFT?3:-1;
    int dirty=0,impulse=0;
    if(mods==KEY_MODS) {
        s->pending_taps=0;
        if(down&KEY_MODS) s->remix_pending=1;
    }
    if(mods && arrows) {
        s->pending_taps&=~mods; s->remix_pending=0;
        if(direction>=0 && (down&(mods|KEY_ARROWS))) {
            if(mods==KEY_MODS) s->backdrop=direction;
            else if(mods==KEY_ONE) s->skin=direction;
            else s->layout=direction;
        }
    } else if(mods!=KEY_MODS && mods && (held&(KEY_A|KEY_B))) {
        s->pending_taps&=~mods; s->remix_pending=0;
        unsigned modifier=held&KEY_B?KEY_B:KEY_A;
        if(down&(mods|modifier)) {
            if(modifier==KEY_B && mods==KEY_ONE) s->mask_pattern=(s->mask_pattern+1)%6;
            else if(modifier==KEY_B) {
                s->motion_mode=(s->motion_mode+1)%4;
                s->motion_phase=s->grid_distance=0; s->grid_tick=0;
            } else if(mods==KEY_ONE) { s->reflection=(s->reflection+1)%4; dirty=1; }
            else s->shape=(s->shape+1)%5;
            s->flash_ticks=12;
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
#endif
