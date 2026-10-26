#ifdef GEEZY_GPU_CHECK
enum { GPU_CASES=104 };
static void gpu_case(int test,int fast) {
    gpu_mesh_enabled=fast; primary_surface=0; shell_surface=0;
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    ortho(); material(NULL); tint=clear_color();
    if(synth.slow) tint=strobe_lit(&synth)?(GXColor){255,255,255,255}:(GXColor){0,0,0,255};
    mask_rect(-1,-1,2,2);
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
    static float worst_error;
    static unsigned worst_pixels,min_coverage=~0u,calls,builds;
    static u64 cpu[2],total[2];
    if(test<GPU_CASES) {
        reset(&synth); synth.theme=test>=100?0:test<64?test/16:test<80?0:(test-80)/5;
        synth.skin=test>=100?test%4:(test/4)%4; synth.slow=test>=64 && test<80; synth.tension=synth.slow?0.6f:0;
        synth.world_mode=test>=80 && test<100?(test-80)%5:0;
        synth.layout=1; synth.backdrop=test%4; synth.gear=2;
        synth.live.phase=0.73f; synth.live.world_cam=(Vec){7,3,11};
        synth.live.azimuth=0.31f; synth.live.elevation=0.15f;
        synth.live.uv_shift=0.13f; synth.live.roll=11; sample_pose(&synth); make_metal();
        gpu_case(test,1); gpu_copy(trail_pixels); // Warm shape caches before measuring steady-state work.
        for(int fast=0;fast<2;fast++) {
            u64 start=gettime(); gpu_case(test,fast); u64 submitted=gettime(); GX_DrawDone(); u64 done=gettime();
            cpu[fast]+=ticks_to_microsecs(submitted-start); total[fast]+=ticks_to_microsecs(done-start);
            gpu_copy(fast?trail_pixels:history_pixels);
        }
        calls+=mesh_calls; builds+=mesh_builds;
        unsigned pixels=mode->fbWidth*mode->efbHeight;
        DCInvalidateRange(history_pixels,pixels*2); DCInvalidateRange(trail_pixels,pixels*2);
        const u16 *a=history_pixels,*b=trail_pixels; unsigned bad=0,coverage=0; double error=0;
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
        if(mean>1.5f || bad>pixels/100 || coverage<500) { failed++; if(first_bad<0) first_bad=test; }
        test++;
        if(test==GPU_CASES && calls==0) failed++;
    }
    gpu_mesh_enabled=1; quantize_vertices=0;
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
}
#endif
