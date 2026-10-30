#include <assert.h>
#include "../source/controls.h"
#include "../source/world_math.h"
int main(void) {
    Synth s; DualInput d={0}; reset(&s);
    dual_controls(&s,&d,0,0,1); unsigned cut=s.cut;
    dual_controls(&s,&d,0,KEY_A,1); assert((unsigned)s.cut!=cut); cut=s.cut;
    dual_controls(&s,&d,0,KEY_A,1); assert((unsigned)s.cut==cut);
    dual_controls(&s,&d,0,0,1); dual_controls(&s,&d,0,KEY_A,1); assert((unsigned)s.cut!=cut);
    const unsigned left_fx[]={0,0,0,0,0,KEY_B,KEY_B,KEY_B};
    const unsigned right_fx[]={KEY_B|KEY_UP,KEY_B|KEY_RIGHT,KEY_B|KEY_DOWN,KEY_B|KEY_LEFT,
        KEY_B|KEY_A,KEY_ONE,KEY_TWO,KEY_PLUS};
    const unsigned fx[]={FX_KALEIDO_4,FX_MIRROR_X,FX_FREEZE,FX_INVERT,FX_MOSAIC,
        FX_BLACKOUT,FX_WHITEOUT,FX_STROBE_KILL};
    for(int i=0;i<8;i++) {
        reset(&s); d=(DualInput){0};
        dual_controls(&s,&d,left_fx[i],right_fx[i],1); assert(d.fx==fx[i]);
        dual_controls(&s,&d,0,right_fx[i]&~KEY_B,1); dual_controls(&s,&d,0,0,1);
        assert(!d.fx && s.shape==0 && s.seed==7 && s.world_mode==0 && s.theme==0 && s.gear==1 && s.cut==0);
    }
    assert(dual_master_fx(0,KEY_B|KEY_DOWN|KEY_LEFT)==(FX_FREEZE|FX_INVERT));
    // Shape changes must not touch camera targets or motion; all primary commands remain accessible.
    reset(&s); d=(DualInput){0}; float az=s.target_azimuth,el=s.target_elevation;
    dual_controls(&s,&d,0,KEY_UP,1); assert(s.shape==1 && s.motion_mode==0 && s.target_azimuth==az && s.target_elevation==el);
    dual_controls(&s,&d,0,KEY_RIGHT,1); assert(s.seed!=7);
    dual_controls(&s,&d,0,KEY_DOWN,1); assert(s.motion_mode==1);
    dual_controls(&s,&d,0,KEY_LEFT,1); assert(s.layout==1);
    dual_controls(&s,&d,0,KEY_PLUS,1); assert(s.gear==2);
    dual_controls(&s,&d,0,KEY_MINUS,1); assert(s.gear==1);
    dual_controls(&s,&d,0,KEY_ONE,1); assert(s.world_mode==0);
    dual_controls(&s,&d,0,0,1); assert(s.world_mode==1);
    dual_controls(&s,&d,0,KEY_TWO,1); dual_controls(&s,&d,0,0,1); assert(s.theme==1);
    // B+direction selects the actual backdrop without steering, shape changes or camera cuts.
    const unsigned arrows[]={KEY_UP,KEY_RIGHT,KEY_DOWN,KEY_LEFT};
    for(int i=0;i<4;i++) {
        reset(&s); d=(DualInput){0};
        dual_controls(&s,&d,KEY_B|arrows[i],0,1);
        assert(s.backdrop==(i==3?0:i) && (i!=3 || s.beam_style==3));
        assert(!(d.performance&KEY_ARROWS) && s.cut==0 && s.shape==0);
        dual_controls(&s,&d,arrows[i],0,1); assert(!(d.performance&KEY_ARROWS));
        dual_controls(&s,&d,0,0,1); dual_controls(&s,&d,arrows[i],0,1);
        assert((d.performance&arrows[i]) && s.cut==1);
    }
    // Every two-hand bank consumes both commands, even when the modifier is released first.
    for(int h=0;h<2;h++) for(int m=0;m<2;m++) for(int i=0;i<4;i++) {
        reset(&s); d=(DualInput){0}; unsigned mod=m?KEY_TWO:KEY_ONE;
        dual_controls(&s,&d,h?0:mod,h?mod:0,1);
        dual_controls(&s,&d,h?arrows[i]:mod,h?mod:arrows[i],1);
        if(!h && !m) assert(s.backdrop==0 && s.beam_style==i);
        if(!h && m) assert(s.skin==i);
        if(h && !m) assert(s.backdrop==i);
        if(h && m) assert(s.reflection==i);
        dual_controls(&s,&d,h?arrows[i]:0,h?0:arrows[i],1);
        dual_controls(&s,&d,0,0,1);
        assert(s.seed==7 && s.shape==0 && s.motion_mode==0 && s.layout==0 && s.cut==0 && s.world_mode==0 && s.theme==0);
    }
    reset(&s); d=(DualInput){0};
    dual_controls(&s,&d,KEY_B|KEY_A,0,1); assert(!(d.performance&KEY_A) && s.mask_pattern==1);
    dual_controls(&s,&d,KEY_A,0,1); assert(!(d.performance&KEY_A));
    dual_controls(&s,&d,0,0,1); dual_controls(&s,&d,KEY_A,0,1); assert(d.performance&KEY_A);
    reset(&s); d=(DualInput){0};
    dual_controls(&s,&d,KEY_A,KEY_ONE,1); assert(!s.hud_visible && !(d.performance&KEY_A));
    dual_controls(&s,&d,0,0,1); assert(s.world_mode==0);
    dual_controls(&s,&d,KEY_A,KEY_TWO,1); assert(s.mask_pattern==1 && !(d.performance&KEY_A));
    dual_controls(&s,&d,0,0,1); assert(s.theme==0);
    dual_controls(&s,&d,KEY_TWO,KEY_A,1); assert(s.palette_bank==1 && s.cut==0);
    dual_controls(&s,&d,0,0,1); assert(s.shape==0);
    uint32_t old_seed=s.seed; dual_controls(&s,&d,KEY_ONE,KEY_A,1); assert(s.seed!=old_seed);
    dual_controls(&s,&d,0,0,1);
    dual_controls(&s,&d,KEY_MINUS,KEY_MINUS,1); assert(s.seed==7 && s.gear==1);
    dual_controls(&s,&d,0,KEY_MINUS,1); assert(s.gear==1);
    // Disconnect clears FX/performance/pending taps and restores the original sideways engine.
    dual_controls(&s,&d,KEY_TWO,KEY_B|KEY_DOWN,1); assert(d.fx==FX_FREEZE);
    dual_controls(&s,&d,0,0,0);
    assert(!d.held[0] && !d.held[1] && !d.fx && !d.performance && !d.connected);
    controls(&s,KEY_TWO|KEY_PLUS); assert(s.theme==1);
    controls(&s,0); assert(s.shape==0);
    reset(&s); d=(DualInput){0}; dual_controls(&s,&d,0,0,1);
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
    reset(&s); d=(DualInput){0}; dual_controls(&s,&d,0,0,1);
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
