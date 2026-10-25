#include <assert.h>
#include "../source/controls.h"
#include "../source/font.h"
#include "../source/chrome.h"
#include "../source/grid.h"
#include "../source/geometry.h"
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
            assert(fabsf(s.live.aim_x)<=1.6f);
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
    // Holding direction speeds up the subjects as well as the world; release settles both.
    reset(&a); reset(&b); a.gear=3; b.gear=3;
    for(int i=0;i<120;i++) { advance(&a,1.0f/60,0,0,0); advance(&b,1.0f/60,0,1,0); }
    float pa=a.live.phase,pb=b.live.phase;
    advance(&a,1.0f/60,0,0,0); advance(&b,1.0f/60,0,1,0);
    float idle_spin=wrap(a.live.phase-pa,6.2831853f),boost_spin=wrap(b.live.phase-pb,6.2831853f);
    assert(boost_spin>idle_spin*4 && boost_spin<0.15f);
    for(int i=0;i<120;i++) advance(&b,1.0f/60,0,0,0);
    pb=b.live.phase; advance(&b,1.0f/60,0,0,0);
    assert(closef(wrap(b.live.phase-pb,6.2831853f),idle_spin));
    hit(&a,1); pa=a.live.phase; advance(&a,1.0f/60,0,0,0);
    assert(fabsf(wrap(a.live.phase-pa,6.2831853f))>idle_spin*10);
    // Fractional-rate object axes remain continuous across phase recycling.
    for(int i=0;i<100;i++) {
        float ratio=i/100.0f,old=314.158f,next=wrap_phase(old+0.002f);
        assert(fabsf(sinf(next*ratio)-sinf((old+0.002f)*ratio))<0.0001f);
        assert(fabsf(cosf(next*ratio)-cosf((old+0.002f)*ratio))<0.0001f);
    }
    // Aim offsets actually change the viewing ray without moving the orbital eye.
    reset(&a); Vec neutral_eye=orbit_eye(&a.live),neutral_target=view_target(&a.live);
    a.live.aim_x=1;
    assert(closef(orbit_eye(&a.live).x,neutral_eye.x));
    assert(view_target(&a.live).x>neutral_target.x+13);
    reset(&a); steer(&a,1,0);
    for(int i=0;i<60;i++) advance(&a,1.0f/60,0,1,0);
    assert(a.live.aim_x>0.59f && a.live.aim_x<0.61f);
    for(int i=0;i<60;i++) advance(&a,1.0f/60,0,1,1);
    assert(a.live.aim_x>1.4f);
    for(int i=0;i<60;i++) advance(&a,1.0f/60,0,0,0);
    assert(fabsf(a.live.aim_x)<0.001f);
    for(int i=0;i<100;i++) { hit(&a,1); assert(a.mask_pattern>=0 && a.mask_pattern<4); }
    // Each shard is periodic in all axes; a recycled shard is already invisible.
    Vec particle_eye={3,4,16};
    for(int axis=0;axis<3;axis++) for(int i=0;i<192;i++) {
        Vec world={17,25,-31},shift=world;
        if(axis==0) shift.x+=96; else if(axis==1) shift.y+=96; else shift.z+=96;
        Vec p=tunnel_particle(i,world,particle_eye),q=tunnel_particle(i,shift,particle_eye);
        assert(closef(p.x,q.x) && closef(p.y,q.y) && closef(p.z,q.z));
        for(int tick=0;tick<192;tick++) {
            Vec w={0,0,0},v={0,0,0};
            if(axis==0) { w.x=tick*0.5f; v.x=w.x+0.01f; }
            else if(axis==1) { w.y=tick*0.5f; v.y=w.y+0.01f; }
            else { w.z=tick*0.5f; v.z=w.z+0.01f; }
            p=tunnel_particle(i,w,particle_eye); q=tunnel_particle(i,v,particle_eye);
            if(fabsf(p.x-q.x)+fabsf(p.y-q.y)+fabsf(p.z-q.z)>1) {
                assert(particle_fade(p,particle_eye)==0 && particle_fade(q,particle_eye)==0);
            } else assert(closef(fabsf(p.x-q.x)+fabsf(p.y-q.y)+fabsf(p.z-q.z),0.01f));
        }
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
    // The visible rail set survives both recycling and the world's 96-unit wrap.
    for(int axis=0;axis<3;axis++) for(int i=0;i<8;i++) {
        float coord=lattice_axis(i,18+axis*11,7);
        int found=0;
        for(int j=0;j<8;j++) if(closef(coord,lattice_axis(j,18+axis*11+96,7))) found=1;
        assert(found);
        assert(closef(lattice_axis(i,18+axis*11,7),lattice_axis(i,18+axis*11+192,7)));
    }
    for(int i=0;i<8;i++) for(int t=0;t<1000;t++) {
        float before=lattice_axis(i,t*0.5f,7),after=lattice_axis(i,t*0.5f+0.01f,7);
        if(fabsf(after-before)>1) {
            assert(lattice_fade((Vec){before,0,16},(Vec){7,0,16})==0);
            assert(lattice_fade((Vec){after,0,16},(Vec){7,0,16})==0);
        } else assert(closef(after-before,-0.01f));
    }
    Vec eye={0,0,16};
    assert(lattice_fade((Vec){0,0,20},eye)==0);
    assert(lattice_fade((Vec){0,0,100},eye)==0);
    assert(lattice_fade((Vec){0,0,44},eye)==1);
    assert(lattice_fade((Vec){0,0,20.01f},eye)<0.00001f);
    assert(lattice_fade((Vec){0,0,99.99f},eye)<0.00001f);
    uint16_t mip[GRID_TEXELS+1]={0};
    for(int i=0;i<4096;i++) mip[i]=0xffff;
    mip[GRID_TEXELS]=0x1234; grid_mipmaps(mip);
    assert(mip[GRID_TEXELS]==0x1234 && mip[GRID_TEXELS-16]==0xffff);
    for(int y=0;y<64;y++) for(int x=0;x<64;x++) mip[grid_index(64,x,y)]=(x+y)%2?0xffff:0;
    grid_mipmaps(mip);
    uint16_t average=(15<<11)|(31<<5)|15;
    assert(mip[4096]==average && mip[GRID_TEXELS-16]==average);
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
    // Taps commit once on release; chords consume taps in either press order.
    reset(&s); uint32_t seed=s.seed;
    assert(!controls(&s,KEY_ONE) && s.seed==seed);
    assert(controls(&s,0) && s.seed!=seed);
    seed=s.seed; assert(!controls(&s,0) && s.seed==seed);
    controls(&s,KEY_TWO); assert(s.shape==0);
    controls(&s,0); assert(s.shape==1 && s.motion_mode==1);
    const unsigned dirs[4]={KEY_UP,KEY_RIGHT,KEY_DOWN,KEY_LEFT};
    for(int d=0;d<4;d++) for(int order=0;order<3;order++) {
        reset(&s); seed=s.seed;
        if(order==0) controls(&s,KEY_ONE);
        if(order==1) controls(&s,dirs[d]);
        controls(&s,KEY_ONE|dirs[d]);
        assert(s.skin==d && s.shape==0 && s.seed==seed);
        for(int j=0;j<10;j++) controls(&s,KEY_ONE|dirs[d]);
        controls(&s,0); assert(s.seed==seed && s.skin==d);
        controls(&s,KEY_TWO|dirs[d]); controls(&s,0);
        assert(s.layout==d && s.shape==0 && s.motion_mode==0);
        controls(&s,KEY_MODS); controls(&s,KEY_MODS|dirs[d]);
        controls(&s,KEY_TWO|dirs[d]); controls(&s,0);
        assert(s.backdrop==d && s.seed==seed && !s.remix_pending);
    }
    reset(&s); controls(&s,KEY_ONE|KEY_UP|KEY_RIGHT); controls(&s,0);
    assert(s.skin==0 && s.seed==7); // Ambiguous diagonals consume the tap without choosing a bank.
    for(int i=0;i<12;i++) {
        controls(&s,KEY_B|KEY_ONE); controls(&s,KEY_B|KEY_ONE);
        assert(s.mask_pattern==(i+1)%8);
        controls(&s,0); assert(s.seed==7);
    }
    for(int i=0;i<8;i++) {
        controls(&s,KEY_B|KEY_TWO); controls(&s,0);
        assert(s.motion_mode==(i+1)%4 && s.shape==0);
        controls(&s,KEY_A|KEY_ONE); controls(&s,0);
        assert(s.reflection==(i+1)%4 && s.seed==7);
        controls(&s,KEY_A|KEY_TWO); controls(&s,0);
        assert(s.shape==1);
        s.shape=0;
    }
    for(int reverse=0;reverse<2;reverse++) {
        reset(&s);
        controls(&s,reverse?KEY_ONE:KEY_A);
        assert(controls(&s,KEY_A|KEY_ONE));
        controls(&s,reverse?KEY_A:KEY_ONE); controls(&s,0);
        assert(s.reflection==1 && s.seed==7);
        controls(&s,reverse?KEY_ONE:KEY_B); controls(&s,KEY_B|KEY_ONE);
        controls(&s,0); int mask=s.mask_pattern; hit(&s,1);
        assert(mask==(reverse?1:2) && s.mask_pattern==mask && s.seed==7);
    }
    reset(&s); controls(&s,KEY_A);
    for(int i=0;i<120;i++) advance(&s,1.0f/60,1,0,0);
    controls(&s,KEY_A|KEY_TWO); controls(&s,KEY_A);
    assert(s.shape==1 && s.motion_mode==0 && closef(s.tension,1));
    controls(&s,0); advance(&s,0,0,0,0);
    assert(s.tension==0 && closef(s.velocity.z,-2400));
    reset(&a); reset(&b);
    unsigned skins=0,layouts=0,backgrounds=0,reflections=0,shapes=0;
    for(int i=0;i<256;i++) {
        controls(&a,KEY_ONE); controls(&a,KEY_MODS); controls(&a,KEY_TWO);
        assert(controls(&a,0));
        controls(&b,KEY_MODS); assert(controls(&b,0));
        assert(a.seed==b.seed && a.shape==b.shape && a.skin==b.skin);
        assert(a.gear==1 && a.shape<5 && a.motion_mode<4 && a.mask_pattern<8);
        skins|=1u<<a.skin; layouts|=1u<<a.layout; backgrounds|=1u<<a.backdrop;
        reflections|=1u<<a.reflection; shapes|=1u<<a.shape;
        for(int j=0;j<10;j++) advance(&a,1.0f/60,0,0,0);
        Vec target=view_target(&a.draw); assert(isfinite(target.x) && isfinite(target.y) && isfinite(target.z));
    }
    assert(skins==15 && layouts==15 && backgrounds==15 && reflections==15 && shapes==31);
    controls(&a,KEY_ONE); controls(&a,KEY_ONE|KEY_PLUS|KEY_MINUS); controls(&a,0);
    assert(a.seed==7 && a.skin==0 && a.layout==0 && a.backdrop==0 && a.reflection==0);
    assert(steering_buttons(KEY_RIGHT|KEY_B) && !steering_buttons(KEY_RIGHT|KEY_ONE));
    for(int i=0;i<192;i++) {
        Vec eye={0,0,16},world={27,-13,39},p=matrix_particle(i,world,eye);
        Vec q=matrix_particle(i,(Vec){world.x+96,world.y+96,world.z+96},eye);
        assert(closef(p.x,q.x) && closef(p.y,q.y) && closef(p.z,q.z));
    }
    // Every wrap is one full checker period; neither axis reverses parity at a seam.
    for(unsigned tick=0;tick<1200;tick++) for(unsigned rate=11;rate<=17;rate+=6) {
        float before=checker_scroll(tick,rate),after=checker_scroll(tick+1,rate);
        assert(closef(wrap(after-before,1),rate/600.0f));
        assert(closef(before,checker_scroll(tick+600,rate)));
        assert(before>=0 && before<1);
    }
    assert(closef(checker_scroll(600,17),0) && closef(checker_scroll(600,11),0));
    unsigned diagonals[4]={KEY_UP|KEY_LEFT,KEY_UP|KEY_RIGHT,KEY_DOWN|KEY_RIGHT,KEY_DOWN|KEY_LEFT};
    for(int style=0;style<4;style++) {
        reset(&s); s.backdrop=3;
        controls(&s,KEY_MODS); controls(&s,KEY_MODS|diagonals[style]);
        assert(s.beam_style==style && s.backdrop==0);
        controls(&s,0); assert(s.seed==7 && !s.remix_pending);
        controls(&s,KEY_MODS|KEY_DOWN); controls(&s,0);
        assert(s.backdrop==2 && s.beam_style==style);
        controls(&s,KEY_MODS|diagonals[style]); controls(&s,0);
        assert(s.backdrop==0 && s.beam_style==style);
        for(int order=0;order<2;order++) {
            unsigned first=diagonals[style]&(~diagonals[style]+1);
            controls(&s,KEY_MODS|(order?diagonals[style]^first:first));
            controls(&s,KEY_MODS|diagonals[style]); controls(&s,0);
            assert(s.backdrop==0 && s.beam_style==style && s.seed==7);
        }
        int axes=0; for(int axis=0;axis<3;axis++) if(beam_axis(style,axis)) axes|=1<<axis;
        assert((axes==(int[]){7,3,4,0}[style]));
    }
    unsigned styles=0; reset(&s);
    for(int i=0;i<256;i++) { remix(&s); styles|=1u<<s.beam_style; }
    assert(styles==15);
    controls(&s,KEY_PLUS|KEY_MINUS); assert(s.beam_style==0);
    // A diagonal strut crossing the subject sphere must be cut, not just shaded black.
    float enter,leave;
    Vec center={0,0,16},half={11.5f,11.5f,0};
    assert(beam_gap(center,half,&enter,&leave));
    assert(enter>-1 && enter<0 && leave>0 && leave<1);
    for(int i=0;i<2;i++) {
        float t=i?leave:enter;
        float x=center.x+half.x*t,y=center.y+half.y*t,z=center.z+half.z*t;
        assert(fabsf(x*x+y*y+z*z-400)<0.001f);
    }
    assert(!beam_gap((Vec){0,0,40},half,&enter,&leave));
    assert(beam_gap((Vec){0,0,0},half,&enter,&leave) && enter<-1 && leave>1);
    // Only B opens the mask: camera cuts, palette/scene commands and release drops do not.
    reset(&s);
    const unsigned gestures[]={KEY_RIGHT,KEY_ONE,KEY_TWO,KEY_ONE|KEY_UP,KEY_A|KEY_TWO,KEY_MODS};
    for(unsigned i=0;i<sizeof(gestures)/sizeof(gestures[0]);i++) {
        controls(&s,gestures[i]); controls(&s,0); hit(&s,4);
        for(int tick=0;tick<20;tick++) advance(&s,1.0f/60,0,0,0);
        assert(s.mask_env==0);
    }
    reset(&s);
    for(int i=1;i<=12;i++) {
        controls(&s,KEY_B); controls(&s,KEY_B);
        assert(s.mask_pattern==i%8);
        advance(&s,1.0f/60,0,0,1); assert(s.mask_env>0);
        controls(&s,0);
        for(int tick=0;tick<60;tick++) advance(&s,1.0f/60,0,0,0);
        assert(s.mask_env==0);
    }
    // HUD chord consumes the palette tap and does not change gear or repeat while held.
    reset(&s); controls(&s,KEY_ONE); controls(&s,KEY_ONE|KEY_PLUS);
    assert(!s.hud_visible && s.gear==1 && s.seed==7);
    controls(&s,KEY_ONE|KEY_PLUS); controls(&s,KEY_PLUS); controls(&s,0);
    assert(!s.hud_visible && s.gear==1 && s.seed==7);
    controls(&s,KEY_ONE|KEY_PLUS); controls(&s,KEY_ONE); controls(&s,0);
    assert(s.hud_visible && s.gear==1 && s.seed==7);
    reset(&s);
    for(int i=0;i<120;i++) advance(&s,1.0f/60,1,0,0);
    assert(strobe_period(0)==6 && strobe_period(s.tension)==2);
    for(unsigned period=2;period<=6;period++) {
        s.tension=(6-period)/4.0f;
        for(unsigned tick=0;tick<period*3;tick++) {
            s.stutter_tick=tick;
            assert(strobe_lit(&s)==(tick%period<period/2));
        }
    }
    s.slow=0; assert(!strobe_lit(&s));
    for(int layout=0;layout<4;layout++) {
        reset(&s); s.layout=layout;
        assert((subject_count(layout)==(int[]){3,1,2,1}[layout]));
        for(int tick=0;tick<360;tick++) { if(tick%60==0) hit(&s,1); advance(&s,1.0f/60,0,1,0); }
        Vec eye=orbit_eye(&s.draw); assert(isfinite(eye.x) && isfinite(eye.y) && isfinite(eye.z));
        float len=s.live.tunnel_axis.x*s.live.tunnel_axis.x+s.live.tunnel_axis.y*s.live.tunnel_axis.y+s.live.tunnel_axis.z*s.live.tunnel_axis.z;
        assert(closef(len,1));
    }
    reset(&s); s.layout=1;
    for(int tick=0;tick<120;tick++) advance(&s,1.0f/60,0,0,0);
    assert(closef(s.draw.aim_x,0.3f)); // Following the sole subject must not recenter its composition.
    unsigned families=0,pairs=0; uint32_t signatures[512]; unsigned unique=0;
    for(unsigned seed=0;seed<512;seed++) {
        Sculpture g=sculpture(seed,seed%5,seed%3);
        families|=1u<<g.family;
        pairs|=1u<<(g.p==2?(g.q==3?0:3):(g.q==4?1:2));
        assert(g.family<9 && g.slices>=8 && g.slices<=12 && g.rings>=3 && g.rings<=5);
        assert(g.sides==4 || g.sides==8 || g.sides==12);
        uint32_t signature=g.family+9*(g.p+4*(g.q+6*(g.slices+13*(g.rings+6*(g.sides+13*(g.profile+3*g.glyph))))));
        unsigned found=0; for(unsigned j=0;j<unique;j++) if(signatures[j]==signature) found=1;
        if(!found) signatures[unique++]=signature;
        Vec a=knot_center(0,g.p,g.q),b=knot_center(6.2831853f,g.p,g.q),n,v;
        assert(closef(a.x,b.x) && closef(a.y,b.y) && closef(a.z,b.z));
        for(int i=0;i<96;i++) {
            knot_frame(i*6.2831853f/96,g.p,g.q,&n,&v);
            assert(closef(n.x*n.x+n.y*n.y+n.z*n.z,1));
            assert(closef(v.x*v.x+v.y*v.y+v.z*v.z,1));
            assert(closef(n.x*v.x+n.y*v.y+n.z*v.z,0));
            float r=super_radius(i*6.2831853f/96,g.symmetry,g.sharpness);
            assert(isfinite(r) && r>=0.45f && r<=1.5f);
        }
    }
    assert(families==511 && pairs==15 && unique>400);
    for(unsigned ring=0;ring<24;ring++) for(int i=0;i<400;i++) {
        float a=tunnel_depth(ring,i*0.5f),b=tunnel_depth(ring,(i+1)*0.5f);
        assert(closef(wrap(b-a,192),-0.5f));
        if(fabsf(b-a)>1) assert(tunnel_fade(a)==0 && tunnel_fade(b)==0);
        assert(closef(a,tunnel_depth(ring,i*0.5f+192)));
    }
    return 0;
}
