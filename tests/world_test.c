#include <assert.h>
#include "../source/world_math.h"
static int close_vec(Vec a,Vec b) {
    return fabsf(a.x-b.x)<0.002f && fabsf(a.y-b.y)<0.002f && fabsf(a.z-b.z)<0.002f;
}
int main(void) {
    Synth s; reset(&s);
    assert(CLOUD_POINTS>=2000 && CLOUD_POINTS<=2500);
    for(int axis=0;axis<3;axis++) for(int shape=0;shape<5;shape++) {
        s.shape=shape; s.draw.phase=2.1f; s.draw.world_cam=(Vec){47.99f,-32.01f,12.5f};
        Pose repeated=s.draw;
        if(axis==0) repeated.world_cam.x+=96;
        if(axis==1) repeated.world_cam.y+=96;
        if(axis==2) repeated.world_cam.z+=96;
        Vec cells[125]; int count=cloud_glyph_cells(cells,shape); assert(count>0 && count<=125);
        for(int i=0;i<CLOUD_POINTS;i++) {
            Vec p=cloud_free(i,&s.draw,s.seed);
            assert(close_vec(p,cloud_free(i,&repeated,s.seed)));
            assert(isfinite(p.x) && isfinite(p.y) && isfinite(p.z));
            assert(fabsf(p.x)<=48 && fabsf(p.y)<=48 && fabsf(p.z)<=48);
            assert(cloud_fade(p)>=0 && cloud_fade(p)<=1);
            Vec text=cloud_glyph(i,cells,count); assert(text.z>=-42.01f && text.z<=-18);
            Vec shell=add(cloud_shell(i,0),(Vec){0,0,28});
            float radius=sqrtf(shell.x*shell.x+shell.y*shell.y+shell.z*shell.z);
            assert(fabsf(radius-(i%2?22:16))<0.001f);
            s.motion_mode=2; s.layout=(3-shape%3)%3;
            assert(close_vec(cloud_position(i,&s,cells,count),cloud_lattice(i,s.draw.world_cam)));
            s.motion_mode=0; s.draw.burst=0; s.slow=0;
            assert(close_vec(cloud_position(i,&s,cells,count),p));
        }
    }
    for(int i=0;i<12;i++) for(int cam=-200;cam<=200;cam++)
        assert(fabsf(world_cell(i,cam,8)-world_cell(i,cam+96,8))<0.001f);
    s.draw.roll=400; s.bank=20; assert(flight_bank(&s)==70);
    s.draw.roll=-400; assert(flight_bank(&s)==-70);
    for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) {
        s.flow=(Vec){x,y,-1}; s.draw.aim_x=x*1.6f; s.draw.aim_y=y*1.6f;
        Vec t=flight_target(&s); assert(t.z<0 && fabsf(t.y)<0.8f);
        assert(fabsf(t.x*t.x+t.y*t.y+t.z*t.z-1)<0.001f);
    }
    reset(&s); s.slow=1; s.tension=0.4f; assert(crystal_amount(&s)==1);
    s.slow=0; s.draw.burst=0.81f; assert(crystal_amount(&s)==1);
    s.draw.burst=0; assert(crystal_amount(&s)==0);
    unsigned kinds=0;
    for(int x=-6;x<=6;x++) for(int y=-6;y<=6;y++) for(int z=-6;z<=6;z++) {
        uint32_t h=atrium_hash(x,y,z,7);
        assert(h==atrium_hash(x+2,y-2,z+4,7));
        Vec cam={41,-37,19},moved={137,-133,211};
        assert(close_vec(atrium_cell(x,y,z,cam),atrium_cell(x+2,y-2,z+4,moved)));
        Vec before=atrium_cell(x,y,z,cam),after=atrium_cell(x,y,z,add(cam,(Vec){2,3,-4}));
        assert(close_vec(after,add(before,(Vec){-2,-3,4})));
        for(int layout=0;layout<4;layout++) for(int backdrop=0;backdrop<4;backdrop++) {
            int kind=atrium_kind(h,layout,backdrop); assert(kind>=0 && kind<4); kinds|=1u<<kind;
        }
    }
    assert(kinds==15);
    for(int step=-20;step<=20;step++) {
        float phase=step*1.57079633f;
        float a=atrium_piston(phase-0.0001f,7,0),b=atrium_piston(phase+0.0001f,7,0);
        assert(fabsf(a-b)<0.02f); // The discrete target changes without teleporting the piston head.
        for(int i=0;i<20;i++) {
            float p=atrium_piston(phase+i*0.1f,7,3); assert(isfinite(p) && p>=0 && p<=14.001f);
        }
    }
    reset(&s);
    AtriumCamera shots[3];
    for(int cut=0;cut<3;cut++) {
        s.cut=cut; shots[cut]=atrium_camera(&s);
        Vec d=add(shots[cut].target,scale_vec(shots[cut].eye,-1));
        assert(d.y>0 && d.z<0 && fabsf(d.x*d.x+d.y*d.y+d.z*d.z-1)<0.001f);
    }
    assert(!close_vec(shots[0].target,shots[1].target) && !close_vec(shots[1].eye,shots[2].eye));
    s.cut=1; s.draw.roll=400; assert(atrium_camera(&s).roll==65);
    s.cut=2;
    s.draw.roll=-400; assert(atrium_camera(&s).roll==-65);
    return 0;
}
