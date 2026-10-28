#include <assert.h>
#include "../source/midi.h"
#include "../source/render_plan.h"
#include "../source/geometry.h"
int main(void) {
    Synth s; MidiInput input={0}; reset(&s);
    for(int i=0;i<8;i++) {
        int pattern=s.mask_pattern; midi_action(&s,&input,82+i,1);
        assert(s.mask_pattern==pattern && midi_overlay(&input)==-1);
        assert(midi_master_fx(&input)==((1u<<(i+1))-1));
        uint8_t leds[90]; midi_leds(&s,&input,leds); assert(leds[82+i]==1);
    }
    midi_action(&s,&input,11,1); assert(midi_overlay(&input)==3);
    for(int i=0;i<8;i++) midi_action(&s,&input,82+i,0);
    assert(!midi_master_fx(&input) && midi_overlay(&input)==3);
    const unsigned buttons[8]={CLASS_A,CLASS_B,CLASS_X,CLASS_Y,CLASS_UP,CLASS_RIGHT,CLASS_DOWN,CLASS_LEFT};
    for(int i=0;i<8;i++) {
        reset(&s); classic_controls(&s,CLASS_ZR,0,0);
        classic_controls(&s,CLASS_ZR|buttons[i],0,0);
        assert(classic_master_fx(CLASS_ZR|buttons[i])==(1u<<i));
        classic_controls(&s,buttons[i],0,0);
        assert((buttons[i]&~s.classic_fx_latch)==0);
        classic_controls(&s,0,0,0);
        assert(s.seed==7 && s.palette_bank==0 && s.mask_pattern==0 && s.shape==0 && s.backdrop==0);
    }
    for(int button=0;button<2;button++) for(int order=0;order<3;order++) {
        reset(&s); unsigned key=button?KEY_B:KEY_A;
        controls(&s,KEY_ONE); controls(&s,KEY_MODS); controls(&s,KEY_MODS|key);
        assert(remote_master_fx(KEY_MODS|key)==(unsigned)(button?FX_KALEIDO_4:FX_FREEZE));
        unsigned held=KEY_MODS|key,release[3]={KEY_ONE,KEY_TWO,key};
        for(int j=0;j<3;j++) { held&=~release[(order+j)%3]; controls(&s,held); }
        assert(s.seed==7 && s.shape==0 && s.mask_pattern==0 && !s.remix_pending && !s.pending_taps);
    }
    reset(&s); advance(&s,1.0f/60,1,1,1); set_master_fx(&s,FX_FREEZE);
    Synth before=s; advance(&s,10,0,0,0); assert(memcmp(&s,&before,sizeof(s))==0);
    memset(&input,0,sizeof(input)); midi_action(&s,&input,60,1); midi_action(&s,&input,65,1);
    midi_action(&s,&input,23,1); assert(s.world_mode==4 && s.theme==1 && s.seed!=before.seed);
    advance(&s,1,0,0,0); assert(s.tick==before.tick && s.tension==before.tension);
    s.live.fov=70; set_master_fx(&s,0); assert(s.draw.fov==70);
    advance(&s,1.0f/60,0,0,0); assert(s.tick==before.tick+1);
    for(unsigned fx=0;fx<256;fx++) for(int ready=0;ready<2;ready++) {
        reset(&s); s.master_fx=fx; s.draw.burst=2; s.mask_pattern=4; s.mask_env=1;
        RenderPlan p=render_plan(&s,ready);
        int hold=(fx&1)?HOLD_BLACK:(fx&2)?HOLD_WHITE:(fx&4)?(ready?HOLD_FREEZE:HOLD_NONE):(fx&128)?HOLD_STROBE:HOLD_NONE;
        assert(p.hold==hold);
        if(hold) assert(!p.echo && !p.history && !p.trail && !p.post_history && !p.capture_freeze);
        else {
            assert(p.echo && p.history && p.post_history==!!(fx&(8|16|64)));
            assert(p.capture_freeze==!!(fx&4) && p.trail==!(fx&4));
        }
        for(int i=0;i<=10;i++) {
            float u=i/10.0f,v=1-u; master_uv(fx,&u,&v);
            assert(u>=0 && u<=1 && v>=0 && v<=1);
            if(fx&(FX_MIRROR_X|FX_KALEIDO_4)) assert(u<=0.5f);
            if(fx&FX_KALEIDO_4) assert(v<=0.5f);
        }
    }
    for(int a=0;a<32;a++) {
        Vec n=unit((Vec){sinf(a),cosf(a*0.7f),cosf(a)});
        float row[4]={n.x,n.y,n.z,-2}; Vec half={4.34f,10.99f,4.34f};
        for(int z=-40;z<=40;z++) {
            Vec c={7,-12,z}; int culled=box_behind_view(c,half,row),all_behind=1;
            for(int corner=0;corner<8;corner++) {
                Vec p=add(c,(Vec){corner&1?half.x:-half.x,corner&2?half.y:-half.y,corner&4?half.z:-half.z});
                if(view_depth(p,row)<=0) all_behind=0;
            }
            assert(culled==all_behind);
        }
    }
    for(int row=0;row<13;row++) for(int col=0;col<24;col++) for(int i=0;i<20;i++) {
        Vec n=dome_formula(row,col); float a=n.x*5+n.z*3,b=i*15.3f;
        assert(fabsf(sinf(a+b)-(sinf(a)*cosf(b)+cosf(a)*sinf(b)))<0.00004f);
    }
    return 0;
}
