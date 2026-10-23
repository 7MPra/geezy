#include <assert.h>
#include "../source/state.h"
static int closef(float a,float b) { return fabsf(a-b)<0.001f; }
int main(void) {
    Synth s; reset(&s);
    for(int i=0;i<2;i++) advance(&s,1.0f/60,0,0,0);
    assert(s.draw.camera.z==0 && closef(s.live.camera.z,-0.4f));
    advance(&s,1.0f/60,0,0,0);
    assert(closef(s.draw.camera.z,-0.6f));
    reset(&s);
    // A repeats the captured pose every four ticks, then releases exactly one hit.
    Pose loop[4];
    for(int i=0;i<4;i++) { advance(&s,1.0f/60,1,0,0); loop[i]=s.draw; }
    assert(loop[0].camera.z!=loop[1].camera.z && loop[1].fov!=loop[3].fov);
    for(int i=0;i<12;i++) {
        advance(&s,1.0f/60,1,0,0);
        assert(closef(s.draw.camera.z,loop[i%4].camera.z));
        assert(closef(s.draw.phase,loop[i%4].phase) && s.draw.fov==loop[i%4].fov);
    }
    advance(&s,1.0f/60,0,0,0);
    assert(closef(s.velocity.z,-1080*0.85f));
    advance(&s,1.0f/60,0,0,0);
    assert(closef(s.velocity.z,-1080*0.85f*0.85f));
    assert(closef(snap_vertex(0.5f),2.0f/3) && closef(snap_vertex(-0.5f),-2.0f/3));
    reset(&s);
    for(int i=0;i<60;i++) advance(&s,1.0f/60,0,0,0);
    assert(s.velocity.z==0 && s.spin_velocity==0 && s.live.roll==0);
    assert(closef(s.live.camera.z,-12) && closef(s.live.phase,0.35f));
    for(int i=0;i<8;i++) change_gear(&s,1);
    assert(s.gear==3);
    for(int i=0;i<8;i++) change_gear(&s,-1);
    assert(s.gear==0);
    reset(&s);
    for(int i=0;i<60;i++) advance(&s,1.0f/60,0,1,0);
    assert(s.boost>4.9f && s.boost<=5 && s.velocity.z==0);
    float boost=s.boost;
    advance(&s,1.0f/60,0,0,0); assert(s.boost<boost && s.boost>1);
    for(int i=0;i<60;i++) advance(&s,1.0f/60,0,0,0);
    assert(s.boost<1.01f && s.overdrive<0.2f);
    advance(&s,1.0f/60,0,0,1);
    assert(s.overdrive==1 && s.mask_env>0 && s.mask_env<1);
    float env=s.mask_env;
    advance(&s,1.0f/60,0,0,0); assert(s.mask_env>0 && s.mask_env<env);
    for(int i=0;i<60;i++) advance(&s,1.0f/60,0,0,0);
    assert(s.mask_env==0 && s.overdrive<0.2f);
    reset(&s); hit(&s,1.8f);
    for(int i=0;i<6;i++) advance(&s,1.0f/60,0,0,0);
    assert(s.overdrive==1);
    for(int mode=0;mode<4;mode++) {
        reset(&s);
        for(int i=0;i<mode;i++) next_scene(&s);
        assert(s.shape==mode && s.motion_mode==mode);
        steer(&s,1,1); hit(&s,1);
        int axes=0;
        for(int i=0;i<180;i++) {
            steer(&s,1,1);
            Vec before=s.live.camera;
            advance(&s,1.0f/60,0,1,0);
            int x=s.live.camera.x!=before.x,y=s.live.camera.y!=before.y,z=s.live.camera.z!=before.z;
            if(mode==2) assert(x+y+z<=1);
            axes|=x|(y<<1)|(z<<2);
            assert(isfinite(s.live.camera.x) && isfinite(s.live.camera.y) && isfinite(s.live.camera.z));
            assert(fabsf(s.live.aim_x)<=0.35f);
        }
        assert(axes==7);
    }
    Synth a,b; reset(&a); reset(&b); hit(&a,1); hit(&b,2.5f);
    assert(a.live.camera.z==0 && a.draw.camera.z==0 && a.live.fov==80);
    assert(a.live.roll==0 && a.live.aim_x==0 && a.live.burst==0);
    assert(a.target_fov==105 && fabsf(a.target_aim_x)<=0.35f && fabsf(a.target_aim_y)<=0.35f);
    assert(closef(a.velocity.z,-600) && closef(b.velocity.z,a.velocity.z*2.5f));
    advance(&a,1.0f/60,0,0,0);
    assert(a.live.fov>80 && a.live.fov<105 && a.draw.fov==80);
    assert(closef(a.velocity.z,-510) && a.live.camera.z<-10);
    float first=a.live.fov;
    for(int i=0;i<19;i++) advance(&a,1.0f/60,0,0,0);
    assert(a.live.fov>first && closef(a.live.fov,105-25*powf(0.72f,20)));
    assert(fabsf(a.velocity.z)>20 && fabsf(a.velocity.z)<25);
    reset(&a); a.live.roll=179; a.target_roll=-179;
    advance(&a,1.0f/60,0,0,0); assert(a.live.roll>179 && a.live.roll<180);
    reset(&s);
    assert(!shake(&s,1) && !shake(&s,1.64f) && !shake(&s,NAN));
    assert(shake(&s,1.7f) && s.shake_cooldown==10);
    float velocity=s.velocity.z; assert(!shake(&s,2) && s.velocity.z==velocity);
    assert(!shake(&s,1));
    for(int i=0;i<9;i++) advance(&s,1.0f/60,0,0,0);
    assert(!shake(&s,1.7f));
    advance(&s,1.0f/60,0,0,0); assert(shake(&s,1.7f));
    for(int i=0;i<15;i++) advance(&s,1.0f/60,0,0,0);
    assert(!shake(&s,2));
    assert(!shake(&s,1)); assert(shake(&s,2));
    reset(&a); reset(&b); hit(&a,1); hit(&b,1);
    for(int i=0;i<60;i++) advance(&a,1.0f/60,0,0,0);
    for(int i=0;i<30;i++) advance(&b,1.0f/30,0,0,0);
    assert(closef(a.live.camera.z,b.live.camera.z) && closef(a.live.uv_shift,b.live.uv_shift));
    for(int i=-300;i<=300;i++) {
        float w=wrap(i*1.37f,96); assert(w>=-48 && w<48);
        assert(closef(w,wrap(i*1.37f+96,96)));
    }
    for(int i=0;i<10000;i++) { hit(&s,2.5f); advance(&s,0.1f,0,0,0); }
    assert(isfinite(s.live.camera.z) && fabsf(s.live.camera.z)<=48);
    reset(&s); palette_next(&s); assert(s.frequency==1);
    palette_next(&s); assert(s.frequency==2);
    palette_next(&s); assert(s.frequency==0);
    reset(&s); assert(s.live.fov==80 && s.target_fov==80 && s.shape==0 && s.shake_cooldown==0);
    return 0;
}
