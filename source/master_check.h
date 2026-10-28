#ifdef GEEZY_MASTER_CHECK
static u16 *master_readback;
static unsigned master_bad,master_cases,master_bad_bits,master_pixel_errors;
static int master_first_x,master_first_y,master_actual,master_expected;
static void master_read(void) {
    GX_CopyTex(master_readback,GX_FALSE); GX_PixModeSync(); GX_DrawDone();
    DCInvalidateRange(master_readback,mode->fbWidth*mode->efbHeight*2);
}
static u16 master_pixel(int x,int y) {
    return master_readback[((y/4)*(mode->fbWidth/4)+x/4)*16+(y%4)*4+x%4];
}
static u16 master_color(int x,int y) {
    return ((x/17+y/29)%2?0xf800:0)|((x/31+y/11)%2?0x07e0:0)|((x/7+y/23)%2?0x001f:0);
}
static void master_expect(int condition) { master_cases++; if(!condition) { master_bad++; master_bad_bits|=1u<<(master_cases-1); } }
static void master_check(void) {
    static int done;
    if(!done) {
        unsigned bytes=mode->fbWidth*mode->efbHeight*2;
        master_readback=memalign(32,bytes); assert(master_readback); DCInvalidateRange(master_readback,bytes);
        // Asymmetric binary RGB fixture makes wrong folds, seams and UVs unambiguous.
        u16 *pixels=history_pixels;
        for(int y=0;y<mode->efbHeight;y++) for(int x=0;x<mode->fbWidth;x++)
            pixels[((y/4)*(mode->fbWidth/4)+x/4)*16+(y%4)*4+x%4]=master_color(x,y);
        DCFlushRange(pixels,bytes); GX_InvalidateTexAll();
        for(int n=0;n<16;n++) {
            unsigned fx=((n&1)?FX_MIRROR_X:0)|((n&2)?FX_KALEIDO_4:0)|((n&4)?FX_MOSAIC:0)|((n&8)?FX_INVERT:0);
            synth.master_fx=fx; master_texture(&history,0); master_finish(render_plan(&synth,0)); master_read();
            unsigned bad=0;
            for(int y=0;y<mode->efbHeight;y++) for(int x=0;x<mode->fbWidth;x++) {
                float u=(x+0.5f)/mode->fbWidth,v=(y+0.5f)/mode->efbHeight;
                if(fx&FX_MOSAIC) { u=(floorf(u*40)+0.5f)/40; v=(floorf(v*30)+0.5f)/30; }
                master_uv(fx,&u,&v);
                float epsilon=(fx&FX_MOSAIC)?0.001f:0;
                u16 expected=master_color((int)(u*mode->fbWidth+epsilon),(int)(v*mode->efbHeight+epsilon));
                if(fx&FX_INVERT) expected^=0xffff;
                if(master_pixel(x,y)!=expected) { if(!master_pixel_errors) { master_first_x=x; master_first_y=y; master_actual=master_pixel(x,y); master_expected=expected; } bad++; master_pixel_errors++; }
            }
            master_expect(bad==0);
        }
        reset(&synth); make_metal(); synth.hud_visible=1;
        for(int i=0;i<4;i++) {
            synth.master_fx=i==0?255:i==1?254:FX_STROBE_KILL; mesh_frame=i;
            master_scene_calls=master_copy_calls=0; trail_valid=1; render_scene(); master_read();
            u16 expected=i==1||i==3?0xffff:0; unsigned bad=0;
            for(unsigned p=0;p<bytes/2;p++) if(master_readback[p]!=expected) bad++;
            master_expect(!bad && !master_scene_calls && !master_copy_calls && !trail_valid);
        }
        synth.master_fx=FX_FREEZE; freeze_valid=0; master_scene_calls=0;
        render_scene(); master_read(); master_expect(freeze_valid && master_scene_calls==1);
        u16 *frozen=malloc(bytes); assert(frozen); memcpy(frozen,master_readback,bytes);
        unsigned tick=synth.tick;
        synth.world_mode=4; synth.theme=1; synth.shape=7; palette_next(&synth); make_metal();
        advance(&synth,1,0,0,0); master_expect(synth.tick==tick);
        master_scene_calls=master_copy_calls=0; render_scene(); master_read();
        master_expect(!memcmp(frozen,master_readback,bytes) && !master_scene_calls && !master_copy_calls);
        set_master_fx(&synth,0); render_scene(); master_read();
        master_expect(memcmp(frozen,master_readback,bytes)!=0 && !freeze_valid && master_scene_calls==1);
        free(frozen); done=1;
    }
    ortho(); material(NULL); GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    tint=(GXColor){0,0,0,255}; mask_rect(-1,-1,2,2);
    tint=master_bad?(GXColor){255,0,0,255}:(GXColor){0,255,0,255}; mask_rect(-1,-1,0.1f,0.1f);
    tint=(GXColor){255,255,255,255}; char text[64];
    snprintf(text,sizeof(text),"MASTER CHECK %u ERR %u",master_cases,master_bad); small_text(text,-0.9f,0.5f);
    snprintf(text,sizeof(text),"BITS %X PIX %u",master_bad_bits,master_pixel_errors); small_text(text,-0.9f,0.3f);
    snprintf(text,sizeof(text),"XY %d %d GOT %X WANT %X",master_first_x,master_first_y,master_actual,master_expected); small_text(text,-0.9f,0.1f);
}
#endif
