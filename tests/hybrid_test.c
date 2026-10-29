#include <assert.h>
#include "../source/controls.h"
#include "../source/world_math.h"
int main(void) {
    Synth s; DualInput d={0}; reset(&s);
    dual_controls(&s,&d,0,1); unsigned cut=s.cut;
    dual_controls(&s,&d,KEY_A,1); assert((unsigned)s.cut!=cut); cut=s.cut;
    dual_controls(&s,&d,KEY_A,1); assert((unsigned)s.cut==cut);
    dual_controls(&s,&d,0,1); dual_controls(&s,&d,KEY_A,1); assert((unsigned)s.cut!=cut);
    const unsigned keys[]={KEY_UP,KEY_RIGHT,KEY_DOWN,KEY_LEFT,KEY_ONE,KEY_TWO,KEY_MINUS,KEY_PLUS};
    const unsigned fx[]={FX_KALEIDO_4,FX_MIRROR_X,FX_FREEZE,FX_INVERT,FX_MOSAIC,FX_STROBE_KILL,FX_BLACKOUT,FX_WHITEOUT};
    for(int i=0;i<8;i++) {
        reset(&s); d=(DualInput){0}; dual_controls(&s,&d,KEY_B,1);
        dual_controls(&s,&d,KEY_B|keys[i],1); assert(right_master_fx(KEY_B|keys[i])==fx[i]);
        dual_controls(&s,&d,keys[i],1); dual_controls(&s,&d,0,1);
        assert(s.shape==0 && s.seed==7 && s.world_mode==0 && s.theme==0 && s.gear==1);
    }
    assert(right_master_fx(KEY_B|KEY_DOWN|KEY_LEFT)==(FX_FREEZE|FX_INVERT));
    dual_controls(&s,&d,KEY_UP,1); assert(s.shape==1);
    dual_controls(&s,&d,KEY_RIGHT,1); assert(s.seed!=7);
    dual_controls(&s,&d,KEY_DOWN,1); assert(s.motion_mode==2);
    dual_controls(&s,&d,KEY_LEFT,1); assert(s.layout==1);
    dual_controls(&s,&d,KEY_PLUS,1); assert(s.gear==2);
    dual_controls(&s,&d,KEY_MINUS,1); assert(s.gear==1);
    dual_controls(&s,&d,KEY_ONE,1); assert(s.world_mode==1);
    dual_controls(&s,&d,0,1); dual_controls(&s,&d,KEY_TWO,1); assert(s.theme==1);
    dual_controls(&s,&d,0,0); assert(!d.held && !d.latch && !d.connected);
    reset(&s); dual_controls(&s,&d,0,1);
    Vec gf[2]={{0,0,1},{0,0,1}}; int valid[2]={1,1},b[2]={0,0};
    remote_gestures(&s,&d,gf,valid,b,1.0f/60);
    gf[0].z=2; uint32_t seed=s.seed;
    assert(!remote_gestures(&s,&d,gf,valid,b,1.0f/60)); assert(s.seed==seed);
    gf[1].z=2; assert(remote_gestures(&s,&d,gf,valid,b,1.0f/60)); assert(s.seed!=seed);
    seed=s.seed; for(int i=0;i<60;i++) remote_gestures(&s,&d,gf,valid,b,1.0f/60);
    assert(s.seed==seed); // Sustained acceleration cannot retrigger without rearming.
    gf[0]=(Vec){1,0,0}; gf[1]=(Vec){0,0,1}; b[0]=1;
    remote_gestures(&s,&d,gf,valid,b,1.0f/60); assert(s.wrist_target==1);
    advance(&s,1.0f/60,0,0,0); assert(s.wrist>0 && s.wrist<1);
    valid[0]=valid[1]=0; remote_gestures(&s,&d,gf,valid,b,1.0f/60); assert(s.wrist_target==0);
    gf[0].x=NAN; valid[0]=1; remote_gestures(&s,&d,gf,valid,b,1.0f/60); assert(s.wrist_target==0);
    reset(&s); d=(DualInput){0}; dual_controls(&s,&d,0,1);
    gf[0]=(Vec){0,0,2}; gf[1]=(Vec){0,0,1}; valid[0]=valid[1]=b[0]=b[1]=1;
    assert(remote_gestures(&s,&d,gf,valid,b,1.0f/60)); assert(s.seed!=7);
    for(unsigned twist=1;twist<=5;twist+=2) for(int j=0;j<4;j++) {
        Vec a=mobius_point(0,j*1.57079633f,twist),c=mobius_point(1,(j+2)*1.57079633f,twist);
        assert(fabsf(a.x-c.x)+fabsf(a.y-c.y)+fabsf(a.z-c.z)<0.0001f);
    }
    for(int i=0;i<=64;i++) for(int j=0;j<=8;j++) {
        Vec p=helix_point(i/64.0f,j*0.78539816f,3,0.4f);
        assert(isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z));
        assert(fabsf(p.x)<3 && fabsf(p.z)<3 && fabsf(p.y)<5);
    }
    reset(&s); Vec eye=hybrid_eye(&s); hit(&s,2); for(int i=0;i<12;i++) advance(&s,1.0f/60,0,0,0);
    Vec next=hybrid_eye(&s); assert(fabsf(eye.x-next.x)+fabsf(eye.y-next.y)+fabsf(eye.z-next.z)>0.1f);
    return 0;
}
