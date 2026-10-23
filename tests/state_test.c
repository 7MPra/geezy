#include <assert.h>
#include "../source/state.h"
static int closef(float a,float b) { return fabsf(a-b)<0.001f; }
int main(void) {
    Synth s; reset(&s);
    for(int i=0;i<3;i++) advance(&s,1.0f/60,0);
    assert(s.draw.camera.z==0 && closef(s.live.camera.z,-6.75f));
    advance(&s,1.0f/60,0);
    assert(closef(s.draw.camera.z,-9) && s.draw.uv_shift>0 && s.draw.phase>0);
    reset(&s);
    for(int i=0;i<11;i++) advance(&s,1.0f/60,1);
    assert(s.draw.camera.z==0 && closef(s.live.camera.z,-25.0f*11/60));
    advance(&s,1.0f/60,1); assert(closef(s.draw.camera.z,-5));
    reset(&s);
    for(int i=0;i<19;i++) advance(&s,1.0f/60,0);
    assert(s.velocity.z==0 && s.live.roll==0);
    advance(&s,1.0f/60,0);
    assert(s.velocity.z<0 && s.live.roll!=0 && s.draw.uv_shift==s.live.uv_shift);
    Synth a,b; reset(&a); reset(&b); hit(&a,1); hit(&b,2.5f);
    assert(closef(b.velocity.z,a.velocity.z*2.5f));
    assert(closef(b.draw.camera.z,a.draw.camera.z*2.5f));
    assert(closef(b.draw.burst,a.draw.burst*2.5f));
    reset(&s);
    assert(!shake(&s,1) && !shake(&s,1.64f) && !shake(&s,NAN));
    assert(shake(&s,1.7f) && s.shake_cooldown==10);
    float velocity=s.velocity.z; assert(!shake(&s,2) && s.velocity.z==velocity);
    assert(!shake(&s,1));
    for(int i=0;i<9;i++) advance(&s,1.0f/60,0);
    assert(!shake(&s,1.7f));
    advance(&s,1.0f/60,0); assert(shake(&s,1.7f));
    for(int i=0;i<15;i++) advance(&s,1.0f/60,0);
    assert(!shake(&s,2));
    assert(!shake(&s,1)); assert(shake(&s,2));
    reset(&a); reset(&b); hit(&a,1); hit(&b,1);
    for(int i=0;i<60;i++) advance(&a,1.0f/60,0);
    for(int i=0;i<30;i++) advance(&b,1.0f/30,0);
    assert(closef(a.live.camera.z,b.live.camera.z) && closef(a.live.uv_shift,b.live.uv_shift));
    for(int i=-300;i<=300;i++) {
        float w=wrap(i*1.37f,96); assert(w>=-48 && w<48);
        assert(closef(w,wrap(i*1.37f+96,96)));
    }
    for(int i=0;i<10000;i++) { hit(&s,2.5f); advance(&s,0.1f,0); }
    assert(isfinite(s.live.camera.z) && fabsf(s.live.camera.z)<=48);
    reset(&s); palette_next(&s); assert(s.frequency==1);
    palette_next(&s); assert(s.frequency==2);
    palette_next(&s); assert(s.frequency==0);
    reset(&s); assert(s.live.fov==95 && s.charge==0 && s.shape==0 && s.shake_cooldown==0);
    return 0;
}
