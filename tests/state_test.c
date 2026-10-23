#include <assert.h>
#include "../source/state.h"
static int closef(float a,float b) { return fabsf(a-b)<0.001f; }
int main(void) {
    Synth s; reset(&s);
    for(int i=0;i<60;i++) advance(&s,1.0f/60,0);
    assert(s.live.camera.z==0 && s.live.phase==0);
    hit(&s); assert(s.draw.camera.z==-5 && s.draw.fov==135 && s.flash_ticks==7);
    float first=s.draw.camera.z;
    for(int i=0;i<4;i++) advance(&s,1.0f/60,0);
    assert(s.draw.camera.z==first && s.live.camera.z<first);
    advance(&s,1.0f/60,0); assert(s.draw.camera.z<first);
    for(int i=0;i<55;i++) advance(&s,1.0f/60,0);
    assert(fabsf(s.velocity.z)<0.001f && s.live.burst<0.001f && s.flash_ticks==0);
    hit(&s); advance(&s,0,1); first=s.draw.camera.z;
    for(int i=0;i<14;i++) advance(&s,1.0f/60,1);
    assert(s.draw.camera.z==first);
    advance(&s,1.0f/60,1); assert(s.draw.camera.z!=first);
    Synth a,b; reset(&a); reset(&b); hit(&a); hit(&b);
    for(int i=0;i<60;i++) advance(&a,1.0f/60,0);
    for(int i=0;i<30;i++) advance(&b,1.0f/30,0);
    assert(closef(a.live.camera.z,b.live.camera.z));
    for(int i=-300;i<=300;i++) {
        float w=wrap(i*1.37f,96); assert(w>=-48 && w<48);
        assert(closef(w,wrap(i*1.37f+96,96)));
    }
    for(int i=0;i<10000;i++) { hit(&s); advance(&s,0.1f,0); }
    assert(isfinite(s.live.camera.z) && fabsf(s.live.camera.z)<=48);
    reset(&s); assert(s.live.fov==95 && s.charge==0 && s.shape==0);
    return 0;
}
