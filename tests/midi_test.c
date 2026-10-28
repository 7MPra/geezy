#include <assert.h>
#include "../source/midi.h"
#include "../source/chrome.h"
static int near(float a,float b) { return fabsf(a-b)<0.001f; }
int main(void) {
    uint8_t descriptor[]={9,2,32,0,1,1,0,0x80,50,9,4,3,1,2,1,3,0,0,7,5,0x82,2,64,0,0,7,5,3,2,64,0,0};
    MidiEndpoints ep;
    assert(midi_endpoints(descriptor,sizeof(descriptor),&ep));
    assert(ep.in==0x82 && ep.out==3 && ep.interface==3 && ep.alternate==1 && ep.in_size==64);
    for(unsigned n=0;n<sizeof(descriptor);n++) assert(!midi_endpoints(descriptor,n,&ep));
    descriptor[15]=2; assert(!midi_endpoints(descriptor,sizeof(descriptor),&ep)); descriptor[15]=3;
    descriptor[21]=3; assert(!midi_endpoints(descriptor,sizeof(descriptor),&ep)); descriptor[21]=2;
    descriptor[18]=0; assert(!midi_endpoints(descriptor,sizeof(descriptor),&ep)); descriptor[18]=7;
    // An IN and OUT belonging to different streaming interfaces cannot be paired.
    uint8_t split[]={9,2,41,0,2,1,0,0x80,50,9,4,0,0,1,1,3,0,0,7,5,0x81,2,64,0,0,9,4,1,0,1,1,3,0,0,7,5,2,2,64,0,0};
    assert(!midi_endpoints(split,sizeof(split),&ep));
    int note,on; uint8_t packet[4]={9,0x90,4,127};
    assert(midi_note(packet,&note,&on) && note==4 && on);
    packet[3]=0; assert(midi_note(packet,&note,&on) && !on);
    packet[0]=8; packet[1]=0x80; assert(midi_note(packet,&note,&on) && !on);
    packet[0]=0xb; packet[1]=0xb0; assert(!midi_note(packet,&note,&on));
    for(int status=0;status<256;status++) {
        packet[0]=status>>4; packet[1]=status; packet[2]=8; packet[3]=1;
        assert(midi_note(packet,&note,&on)==((status&0xf0)==0x80 || (status&0xf0)==0x90));
    }
    packet[0]=0x19; packet[1]=0x90; assert(!midi_note(packet,&note,&on));
    packet[0]=9; packet[2]=128; assert(!midi_note(packet,&note,&on));
    for(int mk2=0;mk2<2;mk2++) for(int n=0;n<90;n++) assert(midi_normal_note(midi_wire_note(n,mk2),mk2)==n);
    midi_led_packet(packet,56,1,0); assert(packet[0]==9 && packet[1]==0x90 && packet[2]==56 && packet[3]==1);
    midi_led_packet(packet,56,1,1); assert(packet[1]==0x96 && packet[3]==21);
    midi_led_packet(packet,8,3,1); assert(packet[3]==13);
    midi_led_packet(packet,4,5,1); assert(packet[3]==5);
    midi_led_packet(packet,82,1,1); assert(packet[1]==0x90 && packet[2]==112 && packet[3]==1);
    Synth s; MidiInput input={0}; reset(&s);
    midi_start(); assert(!midi_usb.stop && !midi_poll(&s,&input));
    assert(s.world_mode==0);
    for(int world=0;world<5;world++) {
        int theme=s.theme,skin=s.skin,reflection=s.reflection;
        midi_action(&s,&input,56+world,1);
        assert(s.world_mode==world && s.theme==theme && s.skin==skin && s.reflection==reflection);
        uint8_t lights[90]; midi_leds(&s,&input,lights);
        for(int i=0;i<5;i++) assert(lights[56+i]==(i==world));
        midi_action(&s,&input,56+world,0);
    }
    for(int cycle=0;cycle<4;cycle++) {
        int theme=s.theme,skin=s.skin,reflection=s.reflection;
        assert(midi_action(&s,&input,61,1)); midi_action(&s,&input,61,0);
        midi_action(&s,&input,62,1); midi_action(&s,&input,62,0);
        assert(midi_action(&s,&input,63,1)); midi_action(&s,&input,63,0);
        assert(s.theme==(theme+1)%4 && s.skin==(skin+1)%4 && s.reflection==(reflection+1)%4 && s.world_mode==4);
    }
    for(int i=0;i<4;i++) {
        midi_action(&s,&input,64+i,1); midi_action(&s,&input,64+i,0);
        assert(s.theme==i && s.skin==0 && s.reflection==0 && s.world_mode==4);
    }
    reset(&s);
    for(int i=1;i<=10;i++) {
        controls(&s,KEY_TWO); controls(&s,KEY_TWO|KEY_MINUS); controls(&s,KEY_TWO|KEY_MINUS);
        controls(&s,KEY_TWO); controls(&s,0);
        assert(s.world_mode==i%5 && s.gear==1 && s.shape==0 && s.motion_mode==0 && s.seed==7);
    }
    for(int i=1;i<=10;i++) {
        classic_controls(&s,CLASS_L,0,0); classic_controls(&s,CLASS_L|CLASS_Y,0,0);
        classic_controls(&s,CLASS_L|CLASS_Y,0,0); classic_controls(&s,0,0,0);
        assert(s.world_mode==i%5 && s.shape==0 && s.motion_mode==0 && s.seed==7);
    }
    for(int n=0;n<90;n++) {
        int dirty=midi_action(&s,&input,n,1); (void)dirty;
        Synth before=s; assert(!midi_action(&s,&input,n,1)); assert(memcmp(&s,&before,sizeof(s))==0);
        midi_action(&s,&input,n,0);
        assert(s.theme<4 && s.harmony_mode<4 && s.mask_pattern<8 && s.layout<4 && s.gear<4 && s.shape<5);
    }
    for(int bank=0;bank<16;bank++) {
        midi_action(&s,&input,16+bank,1); midi_action(&s,&input,16+bank,0);
        uint32_t seed=s.seed;
        midi_action(&s,&input,16+bank,1); midi_action(&s,&input,16+bank,0);
        assert(s.harmony_mode==bank/4 && s.seed==seed+977 && s.palette_bank==bank);
    }
    reset(&s); memset(&input,0,sizeof(input));
    midi_action(&s,&input,8,1); midi_action(&s,&input,11,1); assert(midi_overlay(&input)==3);
    midi_action(&s,&input,11,0); assert(midi_overlay(&input)==0);
    midi_action(&s,&input,8,0); assert(midi_overlay(&input)==-1);
    midi_action(&s,&input,0,1); assert(midi_steering(&s,&input) && s.flow.x<0);
    midi_action(&s,&input,0,0); assert(!midi_steering(&s,&input));
    midi_action(&s,&input,4,1);
    for(int i=0;i<120;i++) advance(&s,1.0f/60,input.held[4],0,0);
    assert(s.tension>0.99f);
    memset(&input,0,sizeof(input)); advance(&s,0,input.held[4],0,0); assert(s.tension==0 && s.draw.fov==125);
    reset(&s); midi_action(&s,&input,5,1); assert(near(s.live.azimuth,s.target_azimuth));
    uint8_t leds[90]; midi_leds(&s,&input,leds); assert(leds[5]==5 && leds[36+s.gear]==1);
    reset(&s); classic_controls(&s,0,1,0); float velocity=s.velocity.x;
    classic_controls(&s,0,1,0); assert(s.velocity.x==velocity);
    classic_controls(&s,0,0,0); classic_controls(&s,0,1,0); assert(s.velocity.x>velocity);
    classic_controls(&s,CLASS_L|CLASS_X,0,0); assert(s.layout==1 && s.seed==7);
    classic_controls(&s,0,0,0); classic_controls(&s,CLASS_ZL,0,0); assert(s.theme==1);
    classic_controls(&s,0,0,0); classic_controls(&s,CLASS_ZR,0,0); assert(s.palette_bank==0);
    classic_controls(&s,0,0,0); assert(s.palette_bank==1);
    classic_controls(&s,0,0,0); classic_controls(&s,CLASS_R|CLASS_X,0,0);
    assert(near(s.live.azimuth,s.target_azimuth));
    controls(&s,KEY_TWO|KEY_PLUS); controls(&s,0); assert(s.theme==2 && s.gear==1 && s.shape==0);
    controls(&s,KEY_ONE|KEY_MINUS); controls(&s,0); assert(s.palette_bank==2 && s.gear==1);
    for(int direction=0;direction<4;direction++) {
        reset(&s);
        for(int pass=0;pass<3;pass++) {
            classic_controls(&s,0,0,0); classic_controls(&s,CLASS_UP<<direction,0,0);
            Synth expected=s; space_preset(&expected,direction+((pass+(direction==0?1:0))%2)*4);
            assert(s.backdrop==expected.backdrop && s.beam_style==expected.beam_style);
        }
    }
    classic_controls(&s,0,0,0); uint32_t old_seed=s.seed;
    assert(classic_controls(&s,CLASS_L|CLASS_R|CLASS_Y,0,0) && s.seed!=old_seed);
    reset(&s); s.hud_visible=0; s.gear=3; unsigned themes=0,harmonies=0,worlds=0;
    for(int i=0;i<256;i++) {
        remix(&s); themes|=1u<<s.theme; harmonies|=1u<<s.harmony_mode;
        worlds|=1u<<s.world_mode;
        assert(!s.hud_visible && s.gear==3 && s.bank_seed[s.palette_bank]==s.seed);
    }
    assert(themes==15 && harmonies==15 && worlds==31);
    palette_next(&s); assert(s.bank_seed[s.palette_bank]==s.seed);
    // Deterministic malformed descriptors and arbitrary USB packets must remain bounded.
    uint32_t random=7; uint8_t bytes[128];
    for(int trial=0;trial<10000;trial++) {
        for(unsigned i=0;i<sizeof(bytes);i++) { random=random*1664525u+1013904223u; bytes[i]=random>>24; }
        midi_endpoints(bytes,random%sizeof(bytes),&ep);
        midi_note(bytes,&note,&on);
    }
    for(int mode=0;mode<4;mode++) for(unsigned seed=0;seed<36000;seed+=137) {
        Palette p=palette_generate(seed,mode);
        ChromeColor colors[4]={p.primary,p.secondary,p.accent,p.bg_tint};
        for(int i=0;i<4;i++) assert(colors[i].r>=0 && colors[i].r<=1 && colors[i].g>=0 && colors[i].g<=1 && colors[i].b>=0 && colors[i].b<=1);
        float difference=fabsf(p.primary.r-p.secondary.r)+fabsf(p.primary.g-p.secondary.g)+fabsf(p.primary.b-p.secondary.b);
        assert(difference>0.4f);
        ChromeColor black=chrome_palette(0,-0.1f,p,mode),white=chrome_palette(0,0.07f,p,mode);
        assert(black.r==0 && black.g==0 && black.b==0 && white.r==1 && white.g==1 && white.b==1);
    }
    Palette p=palette_generate(7000,0);
    ChromeColor face=chrome_palette(0,0.5f,p,0),side=chrome_palette(0.5f,0.5f,p,0);
    assert(fabsf(face.r-side.r)+fabsf(face.g-side.g)+fabsf(face.b-side.b)>0.1f);
    ChromeColor ground=chrome_palette(0,-0.8f,p,0);
    assert(ground.r+ground.g+ground.b>0.3f); // The lower reflection is a visible broad band.
    return 0;
}
