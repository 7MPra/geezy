#ifdef GEEZY_GPU_CHECK
enum { GPU_CASES=404 };
static void *gpu_pixels[2];
static void gpu_case(int test,int fast) {
    trail_valid=0;
    // New cases compare against stage 1; the original 104 still check the immediate reference.
    gpu_mesh_enabled=test>=104 && test<324?1:fast; gpu_stage2_enabled=fast; gpu_stage3_enabled=fast; primary_surface=0; shell_surface=0;
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    ortho(); material(NULL); tint=clear_color();
    if(synth.slow) tint=strobe_lit(&synth)?(GXColor){255,255,255,255}:(GXColor){0,0,0,255};
    mask_rect(-1,-1,2,2);
    if(test>=324) {
        projection(); GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE); primary_surface=1;
        Sculpture g=sculpture(7,0,0); g.family=9+(test-324)%5;
        sculpture_draw((Vec){0,0,0},2.2f,0.43f,g); primary_surface=0; return;
    }
    if(test>=276) { trail_valid=0; render_scene(); return; }
    if(test>=212) {
        projection(); GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE); primary_surface=1;
        Sculpture g=sculpture(7,0,0); g.symmetry=3+(test-212)%4; g.sharpness=0.5f+((test-212)%4)*0.6f;
        for(int i=0;i<3;i++) polyform((Vec){(i-1)*5.0f,0,(i-1)*2.0f},0.65f,0.3f+i*0.2f,g);
        primary_surface=0; return;
    }
    if(test>=152) {
        trail_valid=0; render_scene();
        GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
        GX_CopyTex(gpu_pixels[fast],GX_TRUE); GX_PixModeSync();
        render_scene(); return; // Includes a recursive frame, without a CPU fence between frames.
    }
    if(test>=104) { independent_world(); return; }
    if(test>=100) {
        projection(); GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE); primary_surface=1;
        for(int i=0;i<40;i++) ring((Vec){(i%8-3.5f)*2.5f,(i/8-2)*2.5f,0},
            0.7f+i*0.013f,0.15f,0.3f,0.2f,8,6.2831853f);
        primary_surface=0; return;
    }
    if(test>=80) {
        if(synth.world_mode) independent_world();
        else { projection(); GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE); swarm(); mega(); }
        return;
    }
    projection(); GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE); primary_surface=1;
    for(int i=0;i<3;i++) {
        Vec center={(i-1)*5.0f,0,(i-1)*2.0f}; float a=0.37f+i*0.4f;
        surface(CHROME);
        switch(test%4) {
        case 0: box(center,(Vec){1.2f,2.3f,0.7f},a,i==2?0:255); break;
        case 1: beam(add(center,(Vec){-1,-2,1}),add(center,(Vec){1,2,-1}),0.9f,255); break;
        case 2: ring(center,2,0.6f,a,0.3f,8,i%2?5.026548f:6.2831853f); break;
        case 3: { Sculpture g=sculpture(7,0,0); g.p=3; g.q=5; g.tube=0.4f;
            knot(center,0.8f,a,g); break; }
        }
    }
    primary_surface=0;
}
static void gpu_copy(void *destination) {
    GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE); GX_SetColorUpdate(GX_TRUE);
    GX_CopyTex(destination,GX_TRUE); GX_PixModeSync(); GX_DrawDone();
}
static void gpu_check(void) {
    static int test,failed,first_bad=-1;
    static int bad_cases[GPU_CASES];
    static float worst_error;
    static unsigned worst_pixels,min_coverage=~0u,calls,builds;
    static u64 cpu[2],total[2],stage2_cpu[2];
    if(!gpu_pixels[0]) {
        u32 bytes=GX_GetTexBufferSize(mode->fbWidth,mode->efbHeight,GX_TF_RGB565,GX_FALSE,0);
        gpu_pixels[0]=memalign(32,bytes); gpu_pixels[1]=memalign(32,bytes);
        assert(gpu_pixels[0] && gpu_pixels[1]);
        DCInvalidateRange(gpu_pixels[0],bytes); DCInvalidateRange(gpu_pixels[1],bytes);
    }
    if(test<GPU_CASES) {
        reset(&synth); synth.theme=test>=100?0:test<64?test/16:test<80?0:(test-80)/5;
        synth.skin=test>=100?test%4:(test/4)%4; synth.slow=test>=64 && test<80; synth.tension=synth.slow?0.6f:0;
        synth.world_mode=test>=80 && test<100?(test-80)%5:0;
        synth.layout=1; synth.backdrop=test%4; synth.gear=2;
        synth.live.phase=0.73f; synth.live.world_cam=(Vec){7,3,11};
        synth.live.azimuth=0.31f; synth.live.elevation=0.15f;
        synth.live.uv_shift=0.13f; synth.live.roll=11; sample_pose(&synth); make_metal();
        if(test>=104) {
            int n=test-104;
            synth.theme=(n/12)%4; synth.world_mode=3; synth.layout=0; synth.shape=n%3;
            synth.slow=n%12>=6; synth.tension=synth.slow?0.7f:0;
            synth.tick=n%12>=9?1:0; synth.motion_mode=n%12>=3?2:0;
            if(test>=152 && test<212) {
                n=test-152; synth.world_mode=n%5; synth.theme=(n/5)%4;
                synth.slow=0; synth.tension=0; synth.motion_mode=0;
                synth.live.burst=n>=20 && n<40?1.3f:0;
                synth.mask_pattern=n>=40?4:6; synth.mask_env=1; synth.mask_clock=1;
            }
            if(test>=212 && test<276) {
                n=test-212; synth.theme=n/16; synth.skin=(n/4)%4;
                synth.world_mode=0; synth.slow=0; synth.tension=0;
            }
            if(test>=276) {
                n=test-276; synth.world_mode=(int[]){0,1,3}[n%3]; synth.theme=(n/3)%4;
                synth.backdrop=2; synth.gear=3; synth.layout=1; synth.skin=0;
                synth.slow=n>=24; synth.tension=synth.slow?0.7f:0;
                synth.live.fov=n>=24?125:55; synth.live.roll=n%2?60:-50;
                synth.live.aim_x=n%2?0.35f:-0.35f; synth.live.aim_y=n%2?-0.3f:0.3f;
            }
            if(test>=324) {
                n=test-324; synth.world_mode=0; synth.theme=n/20; synth.skin=(n/5)%4;
                synth.slow=0; synth.tension=0; synth.live.fov=80; synth.live.aim_x=synth.live.aim_y=0; synth.live.roll=11;
            }
            synth.hud_visible=0; sample_pose(&synth); make_metal();
        }
        gpu_case(test,1); gpu_copy(gpu_pixels[1]); // Warm shape caches before measuring steady-state work.
        for(int fast=0;fast<2;fast++) {
            u64 start=gettime(); gpu_case(test,fast); u64 submitted=gettime(); GX_DrawDone(); u64 done=gettime();
            cpu[fast]+=ticks_to_microsecs(submitted-start); total[fast]+=ticks_to_microsecs(done-start);
            if(test>=152 && test<276) stage2_cpu[fast]+=ticks_to_microsecs(submitted-start);
            gpu_copy(gpu_pixels[fast]);
        }
        calls+=mesh_calls; builds+=mesh_builds;
        unsigned pixels=mode->fbWidth*mode->efbHeight;
        DCInvalidateRange(gpu_pixels[0],pixels*2); DCInvalidateRange(gpu_pixels[1],pixels*2);
        const u16 *a=gpu_pixels[0],*b=gpu_pixels[1]; unsigned bad=0,coverage=0; double error=0;
        GXColor bg=clear_color();
        if(synth.slow) bg=strobe_lit(&synth)?(GXColor){255,255,255,255}:(GXColor){0,0,0,255};
        u16 background=rgb565(bg.r/255.0f,bg.g/255.0f,bg.b/255.0f);
        for(unsigned i=0;i<pixels;i++) {
            if(a[i]!=background) coverage++;
            int dr=abs((a[i]>>11)-(b[i]>>11))*8,dg=abs(((a[i]>>5)&63)-((b[i]>>5)&63))*4,db=abs((a[i]&31)-(b[i]&31))*8;
            error+=dr+dg+db; if(dr+dg+db>48) bad++;
        }
        float mean=error/(pixels*3);
        if(mean>worst_error) worst_error=mean;
        if(bad>worst_pixels) worst_pixels=bad;
        if(coverage<min_coverage) min_coverage=coverage;
        if(mean>1.5f || bad>pixels/100 || coverage<500) { bad_cases[failed++]=test; if(first_bad<0) first_bad=test; }
        test++;
        if(test==GPU_CASES && calls==0) failed++;
    }
    gpu_mesh_enabled=gpu_stage2_enabled=gpu_stage3_enabled=1; quantize_vertices=0;
    ortho(); material(NULL); tint=(GXColor){0,0,0,255}; mask_rect(-1,-1,2,2);
    tint=failed?(GXColor){255,0,0,255}:test==GPU_CASES?(GXColor){0,255,0,255}:(GXColor){255,255,0,255};
    mask_rect(-1,-1,0.1f,0.1f); tint=(GXColor){255,255,255,255};
    char text[64]; snprintf(text,sizeof(text),"GPU CHECK %d/%d ERR %d",test,GPU_CASES,failed); small_text(text,-0.9f,0.7f);
    snprintf(text,sizeof(text),"FIRST:%d PIX:%u",first_bad,worst_pixels); small_text(text,-0.9f,0.5f);
    snprintf(text,sizeof(text),"MEAN:%.3f",worst_error); small_text(text,-0.9f,0.3f);
    snprintf(text,sizeof(text),"CPU US %u / %u",(unsigned)(cpu[0]/test),(unsigned)(cpu[1]/test)); small_text(text,-0.9f,0.1f);
    snprintf(text,sizeof(text),"TOTAL US %u / %u",(unsigned)(total[0]/test),(unsigned)(total[1]/test)); small_text(text,-0.9f,-0.1f);
    snprintf(text,sizeof(text),"DL %u BUILD %u",calls,builds); small_text(text,-0.9f,-0.3f);
    snprintf(text,sizeof(text),"MIN PIX %u",min_coverage); small_text(text,-0.9f,-0.5f);
    snprintf(text,sizeof(text),"V1 V2 US %u / %u",(unsigned)(stage2_cpu[0]/124),(unsigned)(stage2_cpu[1]/124)); small_text(text,-0.9f,-0.7f);
    if(test==GPU_CASES && failed) {
        tint=(GXColor){0,0,0,255}; mask_rect(-1,-0.85f,2,1.5f); tint=(GXColor){255,255,255,255};
        for(int i=0;i<failed && i<48;i+=8) {
            text[0]=0;
            for(int j=i;j<i+8 && j<failed;j++) snprintf(text+strlen(text),sizeof(text)-strlen(text),"%d ",bad_cases[j]);
            small_text(text,-0.9f,0.5f-(i/8)*0.2f);
        }
    }
}
#endif
