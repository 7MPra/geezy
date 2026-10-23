#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include "state.h"

static Synth synth;
static volatile int quitting;
static GXRModeObj *mode;
static GXTexObj metal, history;
static u16 metal_pixels[64*64] ATTRIBUTE_ALIGN(32);
static void *history_pixels;
static int history_valid;
static float aspect;
static void power(void) { quitting=1; }
static void reset_button(u32 irq,void *ctx) { (void)irq; (void)ctx; quitting=1; }

static void make_metal(void) {
    float phase=(synth.seed&65535)*0.000095875f;
    int bands=4+2*(synth.seed%3);
    float freq=(float[]){2.5f,6.0f,12.0f}[synth.frequency];
    for(int y=0;y<64;y++) for(int x=0;x<64;x++) {
        float u=(x-31.5f)/32,v=(y-31.5f)/32;
        float light=0.5f+0.5f*cosf(freq*(u+0.62f*v+0.77f*sqrtf(fmaxf(0,1-u*u-v*v))));
        int band=(int)(light*(bands-0.001f));
        int r,g,b;
        if(band==0 || band==bands/2) { r=0; g=0; b=0; }
        else if(band==bands-1) { r=255; g=255; b=255; }
        else {
            float p=phase+band*2;
            float cr=cosf(p),cg=cosf(p+2.094395f),cb=cosf(p+4.188790f);
            float lo=fminf(cr,fminf(cg,cb)),hi=fmaxf(cr,fmaxf(cg,cb));
            r=lroundf(255*(cr-lo)/(hi-lo));
            g=lroundf(255*(cg-lo)/(hi-lo));
            b=lroundf(255*(cb-lo)/(hi-lo));
        }
        int index=((y/4)*16+x/4)*16+(y%4)*4+x%4;
        metal_pixels[index]=((r>>3)<<11)|((g>>2)<<5)|(b>>3);
    }
    DCFlushRange(metal_pixels,sizeof(metal_pixels)); GX_InvalidateTexAll();
}
static void material(GXTexObj *texture) {
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,GX_DIRECT); GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT); GX_SetNumTexGens(1);
    GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0,GX_MODULATE); GX_LoadTexObj(texture,GX_TEXMAP0);
}
static void vertex(Vec p,float u,float v,u8 shade,u8 alpha) {
    GX_Position3f32(p.x,p.y,p.z); GX_Color4u8(shade,shade,shade,alpha); GX_TexCoord2f32(u,v);
}
static Vec add(Vec a,Vec b) { return (Vec){a.x+b.x,a.y+b.y,a.z+b.z}; }
static Vec rotate(Vec p,float a,float b) {
    float x=p.x*cosf(a)+p.z*sinf(a),z=-p.x*sinf(a)+p.z*cosf(a);
    return (Vec){x,p.y*cosf(b)-z*sinf(b),p.y*sinf(b)+z*cosf(b)};
}
static void triangle(Vec a,Vec b,Vec c,u8 shade) {
    Vec u={b.x-a.x,b.y-a.y,b.z-a.z},v={c.x-a.x,c.y-a.y,c.z-a.z};
    Vec n={u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
    float len=sqrtf(n.x*n.x+n.y*n.y+n.z*n.z);
    if(len<0.00001f) return;
    n.x/=len; n.y/=len;
    GX_Begin(GX_TRIANGLES,GX_VTXFMT0,3);
    Vec p[3]={a,b,c};
    for(int i=0;i<3;i++) vertex(p[i],0.5f+n.x*0.85f+synth.draw.uv_shift+(i==1?0.22f:-0.12f),
                              0.5f-n.y*0.85f+synth.draw.uv_shift+(i==2?0.22f:-0.12f),shade,255);
    GX_End();
}
static void quad(Vec a,Vec b,Vec c,Vec d,u8 shade) { triangle(a,b,c,shade); triangle(a,c,d,shade); }
static void box(Vec center,Vec size,float angle,u8 shade) {
    Vec p[8];
    for(int i=0;i<8;i++) p[i]=add(center,rotate((Vec){i&1?size.x:-size.x,i&2?size.y:-size.y,i&4?size.z:-size.z},angle,angle*0.37f));
    static const int faces[24]={0,2,3,1,4,5,7,6,0,1,5,4,2,6,7,3,0,4,6,2,1,3,7,5};
    for(int i=0;i<6;i++) quad(p[faces[i*4]],p[faces[i*4+1]],p[faces[i*4+2]],p[faces[i*4+3]],shade);
}
static void ortho(void) {
    Mtx m; Mtx44 p; guMtxIdentity(m); GX_LoadPosMtxImm(m,GX_PNMTX0);
    guOrtho(p,1,-1,-1,1,0,10); GX_LoadProjectionMtx(p,GX_ORTHOGRAPHIC);
    GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
}
static void feedback_echo(void) {
    ortho(); material(&history);
    float angle=synth.draw.phase*0.02f;
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
    for(int i=0;i<4;i++) {
        float x=(i==0||i==3)?-1:1,y=i<2?1:-1;
        float u=0.5f+(x*cosf(angle)-y*sinf(angle))/2.06f;
        float v=0.5f-(x*sinf(angle)+y*cosf(angle))/2.06f;
        vertex((Vec){x,y,-1},u,v,48,255);
    }
    GX_End();
}
static void projection(void) {
    Mtx44 p;
    guPerspective(p,synth.draw.fov,aspect,0.2f,180);
    // Projection offsets follow the same eased pose as FOV and roll.
    p[0][2]+=synth.draw.aim_x; p[1][2]+=synth.draw.aim_y;
    GX_LoadProjectionMtx(p,GX_PERSPECTIVE);
    Mtx m; guVector axis={0,0,1}; guMtxRotAxisDeg(m,&axis,synth.draw.roll);
    GX_LoadPosMtxImm(m,GX_PNMTX0);
}
static Vec dome_point(int row,int col) {
    float a=col*6.2831853f/16,b=row*3.14159265f/8;
    float phase=synth.draw.phase;
    float radius=fmaxf(70,fminf(140,105+18*sinf(a*3+b*4+phase*3)+synth.draw.burst*2*cosf(a*5-b*2)));
    return (Vec){radius*sinf(b)*cosf(a),radius*cosf(b),radius*sinf(b)*sinf(a)};
}
static void dome(void) {
    projection(); material(&metal); GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
    for(int row=0;row<8;row++) for(int col=0;col<16;col++) {
        if((row+col)%8!=0) continue;
        Vec p[4]={dome_point(row,col),dome_point(row+1,col),dome_point(row+1,col+1),dome_point(row,col+1)};
        GX_SetLineWidth(6,GX_TO_ZERO);
        GX_Begin(GX_LINESTRIP,GX_VTXFMT0,5);
        for(int j=0;j<5;j++) {
            int i=j%4;
            float len=sqrtf(p[i].x*p[i].x+p[i].y*p[i].y+p[i].z*p[i].z);
            // Inward radial normals: the viewer is inside this environment shell.
            vertex(p[i],0.5f-p[i].x/len*0.85f+synth.draw.uv_shift,
                   0.5f+p[i].y/len*0.85f+synth.draw.uv_shift,255,255);
        }
        GX_End();
    }
}
static Vec relative(Vec p) {
    return (Vec){wrap(p.x-synth.draw.camera.x,96),wrap(p.y-synth.draw.camera.y,96),
                 wrap(p.z-synth.draw.camera.z,96)-44};
}
static void mega(void) {
    float phase=synth.draw.phase;
    for(int layer=0;layer<8;layer++) {
        Vec center=relative((Vec){0,0,layer*12});
        if(synth.shape==0) {
            for(int j=0;j<12;j++) {
                float a=j*6.2831853f/12+phase,b=(j+1)*6.2831853f/12+phase;
                float r=9+synth.draw.burst*0.8f;
                quad(add(center,(Vec){r*cosf(a),r*sinf(a),-0.8f}),add(center,(Vec){r*cosf(b),r*sinf(b),-0.8f}),
                     add(center,(Vec){r*cosf(b),r*sinf(b),0.8f}),add(center,(Vec){r*cosf(a),r*sinf(a),0.8f}),(layer+j)%3==0?0:255);
            }
        } else if(synth.shape==1) {
            box(center,(Vec){48,0.9f,1.8f},phase,layer%3==0?0:255);
            box(center,(Vec){0.9f,48,1.8f},phase,(layer+1)%3==0?0:255);
        } else if(synth.shape==2) {
            for(int j=0;j<8;j++) {
                float a=j*6.2831853f/8+phase;
                Vec base=add(center,(Vec){2*cosf(a),2*sinf(a),0});
                Vec tip=add(center,(Vec){48*cosf(a+0.12f),48*sinf(a+0.12f),-18});
                triangle(add(base,(Vec){-1.4f,-1.4f,0}),add(base,(Vec){1.4f,1.4f,1}),tip,(layer+j)%3==0?0:255);
            }
        } else {
            for(int j=0;j<12;j++) {
                float x=(j-6)*8,a=j*0.75f+phase;
                Vec c=add(center,(Vec){x,8*sinf(a),4*cosf(a)});
                Vec d=add(center,(Vec){x+8,8*sinf(a+0.75f),4*cosf(a+0.75f)});
                quad(add(c,(Vec){0,-5,-2}),add(d,(Vec){0,-5,-2}),add(d,(Vec){0,5,2}),add(c,(Vec){0,5,2}),(layer+j)%3==0?0:255);
            }
        }
    }
}
static void swarm(void) {
    for(int i=0;i<192;i++) {
        Vec center=relative((Vec){((i%8)-3.5f)*7,(((i/8)%4)-1.5f)*10+(i%8)*2,(i/32)*16});
        float a=synth.draw.phase+(i%7)*0.9f;
        float size=0.5f+(i%5)*0.22f;
        Vec p=rotate((Vec){size,-size,0},a,a*0.7f);
        Vec q=rotate((Vec){-size,-size,size},a,a*0.7f);
        Vec r=rotate((Vec){0,size*(2+synth.draw.burst),-size},a,a*0.7f);
        triangle(add(center,p),add(center,q),add(center,r),i%3==0?0:255);
    }
}
static void mask_rect(float x,float y,float w,float h) {
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
    vertex((Vec){x,y,-1},0,0,255,255); vertex((Vec){x+w,y,-1},1,0,255,255);
    vertex((Vec){x+w,y+h,-1},1,1,255,255); vertex((Vec){x,y+h,-1},0,1,255,255);
    GX_End();
}
static void overlay(int held) {
    if(!held&&!synth.flash_ticks) return;
    ortho(); material(&metal); GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
    GX_SetBlendMode(GX_BM_LOGIC,GX_BL_ONE,GX_BL_ZERO,GX_LO_INV);
    float t=synth.tick/60.0f,burst=fminf(synth.draw.burst,4);
    float angle=0,scale=1,tx=0,ty=0;
    if(synth.cut%4==1) { tx=fmodf(t*1.7f,0.5f); ty=fmodf(t*1.1f,0.5f); }
    if(synth.cut%4==2) angle=-synth.draw.roll+synth.draw.phase*30;
    if(synth.cut%4==3) { angle=synth.draw.phase*45; scale=0.6f+fmodf(t*1.8f,1.5f)+burst*0.12f; tx=wrap(t*2,3); }
    Mtx m; guVector axis={0,0,1}; guMtxRotAxisDeg(m,&axis,angle);
    guMtxScaleApply(m,m,scale,scale,1); guMtxTransApply(m,m,tx,ty,0);
    GX_LoadPosMtxImm(m,GX_PNMTX0);
    switch(synth.cut%4) {
    case 0:
        for(int i=-1;i<6;i++) mask_rect(-1,-1+i*0.5f-fmodf(t*2,0.5f),2,0.16f+burst*0.025f);
        break;
    case 1:
        for(int y=-1;y<5;y++) for(int x=-1;x<5;x++) if((x+y)%2==0) mask_rect(-1+x*0.5f,-1+y*0.5f,0.5f,0.5f);
        break;
    case 2: {
        float width=0.06f+burst*0.045f;
        mask_rect(-2,-width,4,width*2);
        mask_rect(-width,-2,width*2,2-width);
        mask_rect(-width,width,width*2,2-width);
        break;
    }
    default:
        GX_Begin(GX_TRIANGLES,GX_VTXFMT0,3);
        vertex((Vec){-1,-1,-1},0,0,255,255); vertex((Vec){1,-1,-1},0,0,255,255); vertex((Vec){1,1,-1},0,0,255,255);
        GX_End();
    }
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
}
static void render(int accent) {
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    if(history_valid && (accent || synth.draw.burst>0.4f)) feedback_echo();
    dome();
    projection(); material(&metal); GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
    mega(); swarm(); overlay(accent);
}

int main(void) {
    VIDEO_Init(); WPAD_Init();
    SYS_SetPowerCallback(power); SYS_SetResetCallback(reset_button);
    mode=VIDEO_GetPreferredMode(NULL);
    aspect=CONF_GetAspectRatio()==CONF_ASPECT_16_9?16.0f/9:4.0f/3;
    void *fb[2]={SYS_AllocateFramebuffer(mode),SYS_AllocateFramebuffer(mode)};
    void *fifo=memalign(32,256*1024);
    u32 history_size=GX_GetTexBufferSize(mode->fbWidth,mode->efbHeight,GX_TF_RGB565,GX_FALSE,0);
    history_pixels=memalign(32,history_size);
    if(!fb[0]||!fb[1]||!fifo||!history_pixels) return 1;
    fb[0]=MEM_K0_TO_K1(fb[0]); fb[1]=MEM_K0_TO_K1(fb[1]);
    memset(fifo,0,256*1024); memset(history_pixels,0,history_size); DCFlushRange(history_pixels,history_size);
    VIDEO_Configure(mode); VIDEO_SetNextFramebuffer(fb[0]);
    VIDEO_SetBlack(TRUE); VIDEO_Flush(); VIDEO_WaitVSync();
    if(mode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
    GX_Init(fifo,256*1024);
    GX_SetCopyClear((GXColor){0,0,0,255},0x00ffffff);
    GX_SetViewport(0,0,mode->fbWidth,mode->efbHeight,0,1);
    GX_SetScissor(0,0,mode->fbWidth,mode->efbHeight);
    u32 height=GX_SetDispCopyYScale(GX_GetYScaleFactor(mode->efbHeight,mode->xfbHeight));
    GX_SetDispCopySrc(0,0,mode->fbWidth,mode->efbHeight); GX_SetDispCopyDst(mode->fbWidth,height);
    GX_SetCopyFilter(GX_FALSE,mode->sample_pattern,GX_FALSE,mode->vfilter);
    GX_SetFieldMode(mode->field_rendering,mode->viHeight==2*mode->xfbHeight);
    GX_SetPixelFmt(GX_PF_RGB8_Z24,GX_ZC_LINEAR); GX_SetDither(GX_FALSE); GX_SetCullMode(GX_CULL_NONE);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
    GX_SetNumChans(1); GX_SetNumTevStages(1);
    GX_SetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,0,GX_DF_NONE,GX_AF_NONE);
    GX_InitTexObj(&metal,metal_pixels,64,64,GX_TF_RGB565,GX_REPEAT,GX_REPEAT,GX_FALSE);
    GX_InitTexObjLOD(&metal,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    GX_InitTexObj(&history,history_pixels,mode->fbWidth,mode->efbHeight,GX_TF_RGB565,GX_CLAMP,GX_CLAMP,GX_FALSE);
    GX_InitTexObjLOD(&history,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    GX_SetTexCopySrc(0,0,mode->fbWidth,mode->efbHeight);
    GX_SetTexCopyDst(mode->fbWidth,mode->efbHeight,GX_TF_RGB565,GX_FALSE);
    WPAD_SetDataFormat(0,WPAD_FMT_BTNS_ACC);
    reset(&synth); make_metal();
    GX_CopyDisp(fb[0],GX_TRUE); GX_DrawDone();
    VIDEO_SetBlack(FALSE); VIDEO_Flush();
    int frame=1; u64 last=gettime();
#ifdef GEEZY_VISUAL_CHECK
    unsigned check_frame=0;
#endif
    while(!quitting&&SYS_MainLoop()) {
        u64 now=gettime(); float dt=ticks_to_microsecs(now-last)/1000000.0f; last=now;
        WPAD_ScanPads(); u32 down=WPAD_ButtonsDown(0),held=WPAD_ButtonsHeld(0);
#ifdef GEEZY_VISUAL_CHECK
        down=0;
        held=(check_frame%180>=90 && check_frame%180<120)?WPAD_BUTTON_A:0;
#endif
        if(down&WPAD_BUTTON_HOME) break;
        int recovery=(held&(WPAD_BUTTON_PLUS|WPAD_BUTTON_MINUS))==(WPAD_BUTTON_PLUS|WPAD_BUTTON_MINUS);
        if(recovery) { reset(&synth); history_valid=0; make_metal(); }
        else {
            int impulse=!!(down&(WPAD_BUTTON_A|WPAD_BUTTON_B|WPAD_BUTTON_1|WPAD_BUTTON_2));
            u32 arrows=WPAD_BUTTON_UP|WPAD_BUTTON_DOWN|WPAD_BUTTON_LEFT|WPAD_BUTTON_RIGHT;
            if(down&arrows) {
                int x=!!(held&WPAD_BUTTON_DOWN)-!!(held&WPAD_BUTTON_UP);
                int y=!!(held&WPAD_BUTTON_RIGHT)-!!(held&WPAD_BUTTON_LEFT);
                float len=sqrtf(x*x+y*y+1);
                synth.flow=(Vec){x/len,y/len,-1/len}; impulse=1;
            }
            if(down&WPAD_BUTTON_1) { palette_next(&synth); make_metal(); history_valid=0; }
            if(down&WPAD_BUTTON_2) { synth.shape=(synth.shape+1)%4; history_valid=0; }
            advance(&synth,dt,!!(held&WPAD_BUTTON_A));
            int shaken=0;
#ifndef GEEZY_VISUAL_CHECK
            WPADData *remote=WPAD_Data(0);
            if(remote->err==WPAD_ERR_NONE && (remote->data_present&WPAD_DATA_ACCEL)) {
                struct gforce_t gf;
                WPAD_GForce(0,&gf);
                shaken=shake(&synth,sqrtf(gf.x*gf.x+gf.y*gf.y+gf.z*gf.z));
            }
#endif
            if(impulse && !shaken) hit(&synth,1);
            if(down&WPAD_BUTTON_PLUS) synth.target_fov=55;
            if(down&WPAD_BUTTON_MINUS) synth.target_fov=105;
        }
#ifdef GEEZY_VISUAL_CHECK
        // Deterministic emulator walkthrough, without sending keys to the user's desktop.
        unsigned stage=(check_frame/180)%4, beat=check_frame%180;
        if(beat==0) {
            reset(&synth); synth.shape=stage;
            for(unsigned i=0;i<stage;i++) palette_next(&synth);
            make_metal(); history_valid=0;
        }
        if(beat==60) hit(&synth,1);
        if(beat==90) { synth.flow=(Vec){0.7f,0,-0.7f}; shake(&synth,2); }
        if(beat>=120 && beat<150) { held|=WPAD_BUTTON_B; synth.cut=stage; }
        check_frame++;
#endif
        GX_SetColorUpdate(GX_TRUE); GX_InvalidateTexAll(); render(!!(held&WPAD_BUTTON_B));
        GX_CopyTex(history_pixels,GX_FALSE); GX_PixModeSync(); GX_DrawDone(); history_valid=1;
        GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE); GX_SetColorUpdate(GX_TRUE);
        GX_CopyDisp(fb[frame],GX_TRUE); GX_Flush();
        VIDEO_SetNextFramebuffer(fb[frame]); VIDEO_Flush(); VIDEO_WaitVSync(); frame^=1;
    }
    return 0;
}
