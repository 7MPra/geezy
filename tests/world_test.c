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
    return 0;
}
