#include <assert.h>
#include "../source/state.h"
#include "../source/font.h"
#include "../source/chrome.h"
static int closef(float a,float b) { return fabsf(a-b)<0.001f; }
int main(void) {
    Synth s; reset(&s);
    for(int i=0;i<2;i++) advance(&s,1.0f/60,0,0,0);
    assert(s.draw.world_cam.z==0 && closef(s.live.world_cam.z,-0.4f));
    advance(&s,1.0f/60,0,0,0);
    assert(closef(s.draw.world_cam.z,-0.6f));
    reset(&s);
    // Charge holds each pose for eight ticks and reaches full tension in two seconds.
    float fov=80;
    for(int i=1;i<=120;i++) {
        Pose before=s.draw;
        advance(&s,1.0f/60,1,0,0);
        if(i%8) assert(s.draw.fov==before.fov && s.draw.world_cam.z==before.world_cam.z);
        assert(s.draw.fov<=fov); fov=s.draw.fov;
    }
    assert(closef(s.tension,1) && closef(s.draw.fov,38));
    advance(&s,0,0,0,0);
    assert(s.tension==0 && s.overdrive==1 && s.draw.fov==125);
    assert(closef(s.velocity.z,-2400));
    advance(&s,1.0f/60,0,0,0);
    assert(closef(s.velocity.z,-2400*0.85f));
    reset(&s); advance(&s,1.0f/60,1,0,0); advance(&s,0,0,0,0);
    assert(fabsf(s.velocity.z)>720 && fabsf(s.velocity.z)<750);
    for(int c=0;c<256;c++) for(int row=0;row<5;row++) assert(font_glyph((char)c)[row]<32);
    assert(font_glyph(' ')[0]==0 && font_glyph('?')[0]==0 && font_glyph('0')[0]!=0);
    reset(&s); for(int i=0;i<4;i++) next_scene(&s);
    assert(s.shape==4 && s.motion_mode==0);
    next_scene(&s); assert(s.shape==0 && s.motion_mode==1);
    assert(closef(snap_vertex(0.5f),2.0f/3) && closef(snap_vertex(-0.5f),-2.0f/3));
    reset(&s);
    for(int i=0;i<60;i++) advance(&s,1.0f/60,0,0,0);
    assert(s.velocity.z==0 && s.spin_velocity==0 && s.live.roll==0);
    assert(closef(s.live.world_cam.z,-12) && closef(s.live.phase,0.35f));
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
            Vec before=s.live.world_cam;
            advance(&s,1.0f/60,0,1,0);
            int x=s.live.world_cam.x!=before.x,y=s.live.world_cam.y!=before.y,z=s.live.world_cam.z!=before.z;
            if(mode==2) assert(x+y+z<=1);
            axes|=x|(y<<1)|(z<<2);
            assert(isfinite(s.live.world_cam.x) && isfinite(s.live.world_cam.y) && isfinite(s.live.world_cam.z));
            assert(fabsf(s.live.aim_x)<=0.35f);
        }
        assert(axes==7);
    }
    Synth a,b; reset(&a); reset(&b);
    for(int i=0;i<180;i++) advance(&a,1.0f/60,1,0,0);
    for(int i=0;i<90;i++) advance(&b,1.0f/30,1,0,0);
    assert(a.tension==1 && b.tension==1);
    assert(closef(a.draw.fov,b.draw.fov) && closef(a.draw.world_cam.z,b.draw.world_cam.z));
    reset(&a); reset(&b); hit(&a,1); hit(&b,2.5f);
    assert(a.live.world_cam.z==0 && a.draw.world_cam.z==0 && a.live.fov==80);
    assert(a.live.roll==0 && a.live.aim_x==0 && a.live.burst==0);
    assert(a.target_fov==105 && fabsf(a.target_aim_x)<=0.35f && fabsf(a.target_aim_y)<=0.35f);
    assert(closef(a.velocity.z,-600) && closef(b.velocity.z,a.velocity.z*2.5f));
    advance(&a,1.0f/60,0,0,0);
    assert(a.live.fov>80 && a.live.fov<105 && a.draw.fov==80);
    assert(closef(a.velocity.z,-510) && a.live.world_cam.z<-10);
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
    assert(closef(a.live.world_cam.z,b.live.world_cam.z) && closef(a.live.uv_shift,b.live.uv_shift));
    // World travel must never move the local subjects or orbit camera away from the formation.
    reset(&a); reset(&b); a.live.world_cam=(Vec){41,-35,19};
    for(int i=0;i<600;i++) {
        advance(&a,1.0f/60,0,1,0); advance(&b,1.0f/60,0,1,0);
        assert(closef(a.draw.objects[0].x,b.draw.objects[0].x));
        assert(closef(orbit_eye(&a.draw).z,orbit_eye(&b.draw).z));
    }
    // Shortest-arc springs share the exact damping/stiffness on all angle axes.
    float velocity_test=0.2f;
    float next=spring(0,1,&velocity_test);
    assert(closef(velocity_test,0.27f) && closef(next,0.27f));
    velocity_test=0;
    float angle=179*0.0174532925f,travel=0;
    for(int i=0;i<60;i++) {
        float previous=angle;
        angle=spring(angle,-179*0.0174532925f,&velocity_test);
        travel+=fabsf(wrap(angle-previous,6.2831853f));
    }
    assert(travel<4*0.0174532925f);
    reset(&s); update_rig(&s);
    assert(closef(s.live.look_at.x,(1.0f/3)*0.42f));
    assert(closef(s.live.look_at.z,(-4.0f/3)*0.42f));
    reset(&s); s.bank=10; s.target_roll=20; s.live.burst=1;
    advance(&s,1.0f/60,0,0,0);
    assert(closef(s.bank,8.6f) && closef(s.target_roll,17.2f) && closef(s.live.burst,0.86f));
    for(int f=38;f<=125;f++) assert(closef(dolly_radius(f)*tanf(f*0.008726646f),16*tanf(80*0.008726646f)));
    ChromeColor sky={0,1,0.4f},ground={1,0.4f,0};
    for(int i=-280;i<0;i++) {
        ChromeColor c=chrome_profile(i/1000.0f,sky,ground);
        assert(c.r==0 && c.g==0 && c.b==0);
    }
    for(int i=0;i<=140;i++) {
        ChromeColor c=chrome_profile(i/1000.0f,sky,ground);
        assert(c.r==1 && c.g==1 && c.b==1);
    }
    ChromeColor peak=chrome_profile(-0.65f,sky,ground),zenith=chrome_profile(1,sky,ground);
    assert(closef(peak.r,1) && closef(peak.g,0.4f) && peak.b==0);
    assert(zenith.r==0 && zenith.g==0 && zenith.b==0);
    int rings=0,lit=0;
    for(int i=-1000;i<-280;i++) {
        ChromeColor c=chrome_profile(i/1000.0f,sky,ground);
        int on=c.r>0;
        if(on&&!lit) rings++;
        lit=on;
    }
    assert(rings==1);
    for(int y=0;y<128;y++) for(int x=0;x<128;x++) {
        float ny=chrome_normal_y(x,y);
        assert(ny>=-1 && ny<=1);
        assert(closef(ny,-chrome_normal_y(x,127-y)));
        ChromeColor c=chrome_profile(ny,sky,ground);
        assert(c.r>=0 && c.r<=1 && c.g>=0 && c.g<=1 && c.b>=0 && c.b<=1);
    }
    float pos=0,vel=0,maximum=0; int overshot=0;
    for(int i=0;i<100;i++) { pos=spring(pos,1,&vel); if(pos>1) overshot=1; maximum=fmaxf(maximum,pos); }
    assert(overshot && maximum<1.10f && closef(pos,1));
    reset(&a); hit(&a,1);
    for(int i=0;i<120;i++) advance(&a,1.0f/60,0,0,0);
    assert(a.focus_index==0);
    assert(fabsf(a.live.look_at.x-a.live.objects[0].x)<0.08f);
    assert(fabsf(wrap(a.live.azimuth-a.target_azimuth,6.2831853f))<0.001f);
    assert(closef(a.live.radius,dolly_radius(a.live.fov)));
    assert(closef(dolly_radius(80),16));
    assert(dolly_radius(125)<dolly_radius(80) && dolly_radius(80)<dolly_radius(38));
    for(int i=0;i<2000;i++) {
        if(i%41==0) next_scene(&a);
        if(i%19==0) hit(&a,2.5f);
        steer(&a,(i%3)-1,((i/3)%3)-1);
        advance(&a,1.0f/60,(i%240)<120,1,0);
        Vec eye=orbit_eye(&a.draw);
        assert(isfinite(eye.x) && isfinite(eye.y) && isfinite(eye.z));
        assert(a.draw.radius>0 && fabsf(a.draw.elevation)<=1.3f);
        assert(closef(a.draw.radius*tanf(a.draw.fov*0.008726646f),16*tanf(80*0.008726646f)));
        for(int j=0;j<3;j++) {
            Vec object=a.draw.objects[j];
            assert(fabsf(object.x)<=5.01f && fabsf(object.y)<=3.01f && fabsf(object.z)<=5.01f);

        }
    }
    Vec cell=lattice_relative((Vec){24,48,72},(Vec){18,-3,42});
    Vec repeat=lattice_relative((Vec){24,48,72},(Vec){18+96,-3-96,42+96});
    assert(closef(cell.x,repeat.x) && closef(cell.y,repeat.y) && closef(cell.z,repeat.z));
    reset(&a);
    for(int i=1;i<=12;i++) { palette_next(&a); assert(a.palette_count==(unsigned)i); }
    for(int i=-300;i<=300;i++) {
        float w=wrap(i*1.37f,96); assert(w>=-48 && w<48);
        assert(closef(w,wrap(i*1.37f+96,96)));
    }
    for(int i=0;i<10000;i++) { hit(&s,2.5f); advance(&s,0.1f,0,0,0); }
    assert(isfinite(s.live.world_cam.z) && fabsf(s.live.world_cam.z)<=48);
    reset(&s); palette_next(&s); assert(s.frequency==1);
    palette_next(&s); assert(s.frequency==2);
    palette_next(&s); assert(s.frequency==0);
    reset(&s); assert(s.live.fov==80 && s.target_fov==80 && s.shape==0 && s.shake_cooldown==0);
    return 0;
}
