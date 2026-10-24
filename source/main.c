#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include "state.h"
#include "font.h"
#include <stdio.h>

static Synth synth;
static volatile int quitting;
static GXRModeObj *mode;
static GXTexObj metal, history, dither_tex;
static u16 metal_pixels[64*64] ATTRIBUTE_ALIGN(32);
static u8 dither_pixels[64] ATTRIBUTE_ALIGN(32);
static void *history_pixels;
static int quantize_vertices;
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
    GX_SetNumTevStages(texture==&metal?2:1);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,GX_DIRECT); GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT); GX_SetNumTexGens(texture==&metal?2:1);
    GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0,GX_MODULATE); GX_LoadTexObj(texture,GX_TEXMAP0);
    if(texture==&metal) {
        Mtx uv; guMtxIdentity(uv); uv[0][0]=uv[1][1]=16;
        GX_LoadTexMtxImm(uv,GX_TEXMTX0,GX_MTX2x4);
        GX_SetTexCoordGen(GX_TEXCOORD1,GX_TG_MTX2x4,GX_TG_TEX0,GX_TEXMTX0);
        GX_LoadTexObj(&dither_tex,GX_TEXMAP1);
        GX_SetTevOrder(GX_TEVSTAGE1,GX_TEXCOORD1,GX_TEXMAP1,GX_COLORNULL);
        GX_SetTevColorIn(GX_TEVSTAGE1,GX_CC_ZERO,GX_CC_CPREV,GX_CC_TEXC,GX_CC_ZERO);
        GX_SetTevColorOp(GX_TEVSTAGE1,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
        GX_SetTevAlphaIn(GX_TEVSTAGE1,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_APREV);
        GX_SetTevAlphaOp(GX_TEVSTAGE1,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
    }
}
static void vertex(Vec p,float u,float v,u8 shade,u8 alpha) {
    if(quantize_vertices) p=(Vec){snap_vertex(p.x),snap_vertex(p.y),snap_vertex(p.z)};
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
    float compression=1-0.82f*synth.tension;
    float jitter=fmaxf(0,synth.tension-0.4f)*0.6f;
    center.x=center.x*compression+jitter*sinf(synth.tick*2.4f+center.z);
    center.y=center.y*compression+jitter*cosf(synth.tick*2.1f+center.z);
    Vec p[8];
    for(int i=0;i<8;i++) p[i]=add(center,rotate((Vec){i&1?size.x:-size.x,i&2?size.y:-size.y,i&4?size.z:-size.z},angle,angle*0.37f));
    static const int faces[24]={0,2,3,1,4,5,7,6,0,1,5,4,2,6,7,3,0,4,6,2,1,3,7,5};
    for(int i=0;i<6;i++) quad(p[faces[i*4]],p[faces[i*4+1]],p[faces[i*4+2]],p[faces[i*4+3]],shade);
}
static void ortho(void) {
    quantize_vertices=0;
    Mtx m; Mtx44 p; guMtxIdentity(m); GX_LoadPosMtxImm(m,GX_PNMTX0);
    guOrtho(p,1,-1,-1,1,0,10); GX_LoadProjectionMtx(p,GX_ORTHOGRAPHIC);
    GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
}
static void feedback_echo(void) {
    // GX has one RGB write enable; TEV extracts channels before additive recombination.
    ortho(); material(&history);
    float shift=fminf(0.045f,synth.draw.burst*0.004f+synth.tension*0.025f);
    for(int channel=0;channel<3;channel++) {
        GXColor color=channel==0?(GXColor){0,255,0,255}:channel==1?(GXColor){255,0,0,255}:(GXColor){0,0,255,255};
        GX_SetTevColor(GX_TEVREG0,color);
        GX_SetTevColorIn(GX_TEVSTAGE0,GX_CC_ZERO,GX_CC_TEXC,GX_CC_C0,GX_CC_ZERO);
        GX_SetTevColorOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
        GX_SetBlendMode(channel?GX_BM_BLEND:GX_BM_NONE,GX_BL_ONE,GX_BL_ONE,GX_LO_COPY);
        float offset=channel==0?0:channel==1?shift:-shift;
        GX_Begin(GX_QUADS,GX_VTXFMT0,4);
        for(int i=0;i<4;i++) {
            float x=(i==0||i==3)?-1:1,y=i<2?1:-1;
            vertex((Vec){x,y,-1},(x+1)*0.5f+offset,(1-y)*0.5f,255,255);
        }
        GX_End();
    }
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
}
static void projection(void) {
    quantize_vertices=synth.slow;
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
    int filled=synth.overdrive>0.55f || synth.gear>=2;
    for(int row=0;row<8;row++) for(int col=0;col<16;col++) {
        if(!filled && (row+col)%8!=0) continue;
        Vec p[4]={dome_point(row,col),dome_point(row+1,col),dome_point(row+1,col+1),dome_point(row,col+1)};
        GX_SetLineWidth(6,GX_TO_ZERO);
        GX_Begin(filled?GX_QUADS:GX_LINESTRIP,GX_VTXFMT0,filled?4:5);
        for(int j=0;j<(filled?4:5);j++) {
            int i=j%4;
            float len=sqrtf(p[i].x*p[i].x+p[i].y*p[i].y+p[i].z*p[i].z);
            // Inward radial normals: the viewer is inside this environment shell.
            vertex(p[i],0.5f-p[i].x/len*0.85f+synth.draw.uv_shift,
                   0.5f+p[i].y/len*0.85f+synth.draw.uv_shift,(row+col+synth.cut)%6==0?0:255,255);
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
    for(int layer=0;layer<6;layer++) {
        Vec center=relative((Vec){0,0,layer*16});
        u8 shade=layer%3==0?0:255;
        if(synth.shape==4) {
            if(layer%2) continue;
            const char *word=(const char *[]){"HYPER","VOID","WII"}[layer/2];
            int count=strlen(word);
            for(int k=0;k<count;k++) for(int row=0;row<5;row++) for(int col=0;col<5;col++)
                if(font_glyph(word[k])[row]&(1<<(4-col)))
                    box(add(center,(Vec){(k*6+col-(count*6-2)*0.5f)*2.8f,(2-row)*2.8f,0}),
                        (Vec){1.25f,1.25f,2.8f},0,255);
        } else if(synth.shape==0) {
            // Thick gate frames leave a readable opening to fly through.
            box(add(center,(Vec){-10,0,0}),(Vec){1.8f,12,2.4f},0,shade);
            box(add(center,(Vec){10,0,0}),(Vec){1.8f,12,2.4f},0,shade);
            box(add(center,(Vec){0,12,0}),(Vec){11.8f,1.8f,2.4f},0,255);
            box(add(center,(Vec){0,-12,0}),(Vec){11.8f,1.8f,2.4f},0,shade);
        } else if(synth.shape==1) {
            for(int j=0;j<10;j++) {
                float a=j*6.2831853f/10+phase+layer*0.2f;
                box(add(center,(Vec){12*cosf(a),12*sinf(a),0}),
                    (Vec){2.8f,2.8f,3.2f},phase,(layer+j)%3==0?0:255);
            }
        } else if(synth.shape==2) {
            for(int j=-1;j<=1;j++) {
                box(add(center,(Vec){0,j*12,0}),(Vec){28,1.4f,1.4f},0,shade);
                box(add(center,(Vec){j*12,0,0}),(Vec){1.4f,28,1.4f},0,255);
                box(add(center,(Vec){j*12,j*12,0}),(Vec){1.4f,1.4f,12},0,shade);
            }
        } else {
            for(int j=0;j<5;j++) {
                float a=phase+layer*0.4f+j*0.6f;
                box(add(center,(Vec){(j-2)*10,8*sinf(a),0}),
                    (Vec){2.5f,9,3.5f},a,(j+layer)%3==0?0:255);
            }
        }
    }
}
static void swarm(void) {
    int count=24+synth.gear*16;
    for(int i=0;i<count;i++) {
        Vec center=relative((Vec){((i%4)-1.5f)*15,(((i/4)%3)-1)*18,(i/12)*16});
        Vec size;
        if(i%3==0) size=(Vec){1.8f,4.5f,1.8f};
        else if(i%3==1) size=(Vec){0.9f,0.9f,9};
        else size=(Vec){2.2f,2.2f,2.2f};
        box(center,size,synth.motion_mode==2?0:synth.draw.phase+(i%7)*0.3f,i%3==0?0:255);
    }
}
static void mask_rect(float x,float y,float w,float h) {
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
    vertex((Vec){x,y,-1},0,0,255,255); vertex((Vec){x+w,y,-1},1,0,255,255);
    vertex((Vec){x+w,y+h,-1},1,1,255,255); vertex((Vec){x,y+h,-1},0,1,255,255);
    GX_End();
}
static void pixel_word(int pattern,float env) {
    const char *word=(const char *[]){"HYPER","VOID","ERR","WII","01",">>>"}[pattern-4];
    int count=strlen(word);
    float cell=1.85f/(count*6-1);
    for(int i=0;i<count;i++) {
        float part=clamp01((env-i*0.06f)/(1-i*0.06f));
        const uint8_t *glyph=font_glyph(word[i]);
        float x=-0.925f+i*6*cell;
        float y=2.5f*cell*aspect+(1-part)*(i%2?1.2f:-1.2f);
        for(int row=0;row<5;row++) for(int col=0;col<5;col++) {
            if(glyph[row]&(1<<(4-col)))
                mask_rect(x+(col+(1-part)*0.5f)*cell,y-(row+1)*cell*aspect,
                          cell*part,cell*aspect*part);
        }
    }
}
static void overlay(void) {
    float env=synth.mask_env;
    if(env==0) return;
    ortho(); material(&metal); GX_SetNumTevStages(1); GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
    GX_SetBlendMode(GX_BM_LOGIC,GX_BL_ONE,GX_BL_ZERO,GX_LO_INV);
    float t=synth.tick/60.0f,burst=fminf(synth.draw.burst,4);
    int pattern=synth.mask_pattern;
    float angle=0,scale=1,tx=0,ty=0;
    if(pattern==1) { tx=fmodf(t*1.7f,0.5f); ty=fmodf(t*1.1f,0.5f); }
    if(pattern==2) angle=-synth.draw.roll+synth.draw.phase*30;
    if(pattern==3) { angle=synth.draw.phase*45; scale=env*(0.6f+fmodf(t*1.8f,1.5f)+burst*0.12f); tx=wrap(t*2,3); }
    Mtx m; guVector axis={0,0,1}; guMtxRotAxisDeg(m,&axis,angle);
    guMtxScaleApply(m,m,scale,scale,1); guMtxTransApply(m,m,tx,ty,0);
    GX_LoadPosMtxImm(m,GX_PNMTX0);
    switch(pattern) {
    case 0:
        for(int i=-1;i<6;i++) mask_rect(-1,-1+i*0.5f-fmodf(t*2,0.5f),2,(0.16f+burst*0.025f)*env);
        break;
    case 1:
        for(int y=-1;y<5;y++) for(int x=-1;x<5;x++) if((x+y)%2==0)
            mask_rect(-1+x*0.5f+(1-env)*0.25f,-1+y*0.5f+(1-env)*0.25f,0.5f*env,0.5f*env);
        break;
    case 2: {
        float width=(0.06f+burst*0.045f)*env;
        mask_rect(-2,-width,4,width*2);
        mask_rect(-width,-2,width*2,2-width);
        mask_rect(-width,width,width*2,2-width);
        break;
    }
    case 3:
        GX_Begin(GX_TRIANGLES,GX_VTXFMT0,3);
        vertex((Vec){-1,-1,-1},0,0,255,255); vertex((Vec){1,-1,-1},0,0,255,255); vertex((Vec){1,1,-1},0,0,255,255);
        GX_End();
        break;
    default: pixel_word(pattern,env); break;
    }
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
}
static void small_text(const char *text,float x,float y) {
    float dot=0.008f;
    for(int i=0;text[i];i++) for(int row=0;row<5;row++) for(int col=0;col<5;col++)
        if(font_glyph(text[i])[row]&(1<<(4-col)))
            mask_rect(x+(i*6+col)*dot,y-(row+1)*dot*aspect,dot,dot*aspect);
}
static void telemetry(void) {
    ortho(); material(&metal); GX_SetNumTevStages(1); GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
    GX_SetBlendMode(GX_BM_LOGIC,GX_BL_ONE,GX_BL_ZERO,GX_LO_INV);
    char text[40];
    float cruise=(float[]){3,12,36,72}[synth.gear]*synth.boost;
    float speed=synth.slow?0:sqrtf(synth.velocity.x*synth.velocity.x+synth.velocity.y*synth.velocity.y+synth.velocity.z*synth.velocity.z)+cruise;
    snprintf(text,sizeof(text),"VEL:%04.0f",fminf(speed,9999)); small_text(text,-0.94f,0.94f);
    snprintf(text,sizeof(text),"GEAR:%d",synth.gear); small_text(text,-0.94f,0.84f);
    snprintf(text,sizeof(text),"FOV:%03.0f",synth.draw.fov); small_text(text,0.94f-strlen(text)*0.048f,0.94f);
    snprintf(text,sizeof(text),"SEED:%04X",(unsigned)(synth.seed&65535)); small_text(text,0.94f-strlen(text)*0.048f,0.84f);
    if(synth.tension<=0.7f || (synth.tick/6)%2==0) {
        char bar[11]; for(int i=0;i<10;i++) bar[i]=i<(int)(synth.tension*10)?'|':' '; bar[10]=0;
        snprintf(text,sizeof(text),"TNS:[%s]",bar); small_text(text,-0.94f,-0.82f);
        snprintf(text,sizeof(text),"%03.0f%%",synth.tension*100); small_text(text,0.94f-strlen(text)*0.048f,-0.82f);
    }
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
}
static void render(void) {
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    dome();
    projection(); material(&metal); GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
    mega(); swarm();
    // Capture the clean scene before masks/UI, avoiding recursive color contamination.
    if(synth.draw.burst>0.5f || synth.tension>0.3f) {
        GX_CopyTex(history_pixels,GX_FALSE); GX_PixModeSync(); GX_DrawDone(); GX_InvalidateTexAll();
        feedback_echo();
    }
    overlay(); telemetry();
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
    for(int y=0;y<8;y++) for(int x=0;x<8;x++) dither_pixels[y*8+x]=y%4==0?0:255;
    DCFlushRange(dither_pixels,sizeof(dither_pixels));
    GX_InitTexObj(&dither_tex,dither_pixels,8,8,GX_TF_I8,GX_REPEAT,GX_REPEAT,GX_FALSE);
    GX_InitTexObjLOD(&dither_tex,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
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
        // Exercise button-only gestures without desktop key injection.
        unsigned stage=(check_frame/300)%6, beat=check_frame%300;
        down=0; held=0;
        if(beat==0) {
            reset(&synth);
            for(unsigned i=0;i<stage%5;i++) next_scene(&synth);
            synth.gear=stage%4;
            for(unsigned i=0;i<stage;i++) palette_next(&synth);
            make_metal();
        }
        if(beat==30) down=WPAD_BUTTON_DOWN;
        if(beat>=30 && beat<90) held|=WPAD_BUTTON_DOWN;
        if(beat>=90 && beat<210) held|=WPAD_BUTTON_A;
        if(beat>=240 && beat<280) { held|=WPAD_BUTTON_B; synth.mask_pattern=4+stage; }
        check_frame++;
#endif
        if(down&WPAD_BUTTON_HOME) break;
        int recovery=(held&(WPAD_BUTTON_PLUS|WPAD_BUTTON_MINUS))==(WPAD_BUTTON_PLUS|WPAD_BUTTON_MINUS);
        if(recovery) { reset(&synth); make_metal(); }
        else {
            int impulse=!!(down&(WPAD_BUTTON_B|WPAD_BUTTON_1|WPAD_BUTTON_2));
            u32 arrows=WPAD_BUTTON_UP|WPAD_BUTTON_DOWN|WPAD_BUTTON_LEFT|WPAD_BUTTON_RIGHT;
            if(held&arrows) {
                int x=!!(held&WPAD_BUTTON_DOWN)-!!(held&WPAD_BUTTON_UP);
                int y=!!(held&WPAD_BUTTON_RIGHT)-!!(held&WPAD_BUTTON_LEFT);
                steer(&synth,x,y);
                if(down&arrows) impulse=1;
            }
            if(down&WPAD_BUTTON_1) { palette_next(&synth); make_metal(); }
            if(down&WPAD_BUTTON_2) { next_scene(&synth); }
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
            if(down&WPAD_BUTTON_PLUS) change_gear(&synth,1);
            if(down&WPAD_BUTTON_MINUS) change_gear(&synth,-1);
            advance(&synth,dt,!!(held&WPAD_BUTTON_A),!!(held&arrows),!!(held&WPAD_BUTTON_B));
        }
        GX_SetColorUpdate(GX_TRUE); GX_InvalidateTexAll(); render();
        GX_DrawDone();
        GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE); GX_SetColorUpdate(GX_TRUE);
        GX_CopyDisp(fb[frame],GX_TRUE); GX_Flush();
        VIDEO_SetNextFramebuffer(fb[frame]); VIDEO_Flush(); VIDEO_WaitVSync(); frame^=1;
    }
    return 0;
}
