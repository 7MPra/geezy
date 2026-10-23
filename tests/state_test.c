#include <assert.h>
#include "../source/state.h"
static int closef(float a,float b) { return fabsf(a-b)<0.001f; }
int main(void) {
    Synth s; reset(&s);
    for(int i=0;i<2;i++) advance(&s,1.0f/60,0);
    assert(s.draw.camera.z==0 && closef(s.live.camera.z,-0.4f));
    advance(&s,1.0f/60,0);
    assert(closef(s.draw.camera.z,-0.6f));
    reset(&s);
    for(int i=0;i<9;i++) advance(&s,1.0f/60,1);
    assert(s.draw.camera.z==0 && closef(s.live.camera.z,-0.3f));
    advance(&s,1.0f/60,1); assert(closef(s.draw.camera.z,-1.0f/3));
    reset(&s);
    for(int i=0;i<60;i++) advance(&s,1.0f/60,0);
    assert(s.velocity.z==0 && s.spin_velocity==0 && s.live.roll==0);
    assert(closef(s.live.camera.z,-12) && closef(s.live.phase,0.35f));
    Synth a,b; reset(&a); reset(&b); hit(&a,1); hit(&b,2.5f);
    assert(a.live.camera.z==0 && a.draw.camera.z==0 && a.live.fov==80);
    assert(a.live.roll==0 && a.live.aim_x==0 && a.live.burst==0);
    assert(a.target_fov==105 && fabsf(a.target_aim_x)<=0.35f && fabsf(a.target_aim_y)<=0.35f);
    assert(closef(a.velocity.z,-600) && closef(b.velocity.z,a.velocity.z*2.5f));
    advance(&a,1.0f/60,0);
    assert(a.live.fov>80 && a.live.fov<105 && a.draw.fov==80);
    assert(closef(a.velocity.z,-510) && a.live.camera.z<-10);
    float first=a.live.fov;
    for(int i=0;i<19;i++) advance(&a,1.0f/60,0);
    assert(a.live.fov>first && closef(a.live.fov,105-25*powf(0.72f,20)));
    assert(fabsf(a.velocity.z)>20 && fabsf(a.velocity.z)<25);
    reset(&a); a.live.roll=179; a.target_roll=-179;
    advance(&a,1.0f/60,0); assert(a.live.roll>179 && a.live.roll<180);
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
    reset(&s); assert(s.live.fov==80 && s.target_fov==80 && s.shape==0 && s.shake_cooldown==0);
    return 0;
}
