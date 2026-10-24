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
static GXTexObj chrome_tex, grid_tex, history, dither_tex;
static u16 chrome_pixels[128*128] ATTRIBUTE_ALIGN(32);
static u16 grid_pixels[64*64] ATTRIBUTE_ALIGN(32);
static u8 dither_pixels[64] ATTRIBUTE_ALIGN(32);
static void *history_pixels;
static int quantize_vertices;
static float aspect;
static Mtx view;
static GXColor tint={255,255,255,255};
enum { CHROME, GRID, WIRE, MATTE, GHOST };
static int surface_kind;
static void power(void) { quitting=1; }
static void reset_button(u32 irq,void *ctx) { (void)irq; (void)ctx; quitting=1; }

static u16 rgb565(float r,float g,float b) {
    return ((u16)(clamp01(r)*31)<<11)|((u16)(clamp01(g)*63)<<5)|(u16)(clamp01(b)*31);
}
static void make_metal(void) {
    float phase=(synth.seed&65535)*0.000095875f;
    float nr=0.5f+0.5f*cosf(phase),ng=0.5f+0.5f*cosf(phase+2.094395f),nb=0.5f+0.5f*cosf(phase+4.188790f);
    float lo=fminf(nr,fminf(ng,nb)),hi=fmaxf(nr,fmaxf(ng,nb));
    nr=(nr-lo)/(hi-lo); ng=(ng-lo)/(hi-lo); nb=(nb-lo)/(hi-lo);
    for(int y=0;y<128;y++) for(int x=0;x<128;x++) {
        float u=x/127.0f,v=y/127.0f,r,g,b;
        if(synth.palette_count && synth.palette_count%4==0) {
            float angle=atan2f(v-0.5f,u-0.5f)*2+v*7+phase;
            r=0.5f+0.5f*cosf(angle); g=0.5f+0.5f*cosf(angle+2.094395f); b=0.5f+0.5f*cosf(angle+4.188790f);
        } else {
            float reflection=v<0.5f?powf(clamp01((v-0.20f)/0.24f),3):powf(clamp01((1-v)*1.6f),2);
            float glow=0.25f+0.75f*fabsf(v-0.52f)*2;
            r=nr*glow+(1-nr)*reflection; g=ng*glow+(1-ng)*reflection; b=nb*glow+(1-nb)*reflection;
        }
        if(v>0.44f && v<0.50f) r=g=b=1;
        if(v>=0.50f && v<0.58f) r=g=b=0;
        int i=((y/4)*32+x/4)*16+(y%4)*4+x%4;
        chrome_pixels[i]=rgb565(r,g,b);
    }
    for(int y=0;y<64;y++) for(int x=0;x<64;x++) {
        int edge=x<2||y<2||x>=62||y>=62;
        int line=(x>=31&&x<=32)||(y>=31&&y<=32);
        grid_pixels[((y/4)*16+x/4)*16+(y%4)*4+x%4]=edge?rgb565(nr,ng,nb):line?0xffff:0;
    }
    DCFlushRange(chrome_pixels,sizeof(chrome_pixels)); DCFlushRange(grid_pixels,sizeof(grid_pixels)); GX_InvalidateTexAll();
}
static void material(GXTexObj *texture) {
    tint=(GXColor){255,255,255,255};
    GX_SetNumTevStages(1); GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,GX_DIRECT); GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT); GX_SetNumTexGens(texture?1:0);
    GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GX_SetTevOrder(GX_TEVSTAGE0,texture?GX_TEXCOORD0:GX_TEXCOORDNULL,texture?GX_TEXMAP0:GX_TEXMAP_NULL,GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0,texture?GX_MODULATE:GX_PASSCLR);
    if(texture) GX_LoadTexObj(texture,GX_TEXMAP0);
}
static void surface(int kind) {
    surface_kind=kind;
    material(kind==CHROME?&chrome_tex:kind==GRID?&grid_tex:kind==GHOST?&chrome_tex:NULL);
    if(kind==MATTE) tint=(GXColor){0,0,0,255};
    if(kind==WIRE) tint=(synth.seed&1)?(GXColor){0,255,255,255}:(GXColor){255,255,255,255};
    if(kind==GHOST) {
        GX_SetNumTevStages(2); GX_SetNumTexGens(2);
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
    GX_Position3f32(p.x,p.y,p.z); GX_Color4u8(shade*tint.r/255,shade*tint.g/255,shade*tint.b/255,alpha); GX_TexCoord2f32(u,v);
}
static Vec add(Vec a,Vec b) { return (Vec){a.x+b.x,a.y+b.y,a.z+b.z}; }
static Vec rotate(Vec p,float a,float b) {
    float x=p.x*cosf(a)+p.z*sinf(a),z=-p.x*sinf(a)+p.z*cosf(a);
    return (Vec){x,p.y*cosf(b)-z*sinf(b),p.y*sinf(b)+z*cosf(b)};
}
static Vec scale_vec(Vec p,float scale) { return (Vec){p.x*scale,p.y*scale,p.z*scale}; }
static Vec unit(Vec p) {
    float len=sqrtf(p.x*p.x+p.y*p.y+p.z*p.z);
    return len>0.00001f?scale_vec(p,1/len):(Vec){0,1,0};
}
static Vec cross(Vec a,Vec b) { return (Vec){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
static void normal_vertex(Vec p,Vec n,u8 shade) {
    float nx=view[0][0]*n.x+view[0][1]*n.y+view[0][2]*n.z;
    float ny=view[1][0]*n.x+view[1][1]*n.y+view[1][2]*n.z;
    vertex(p,0.5f+nx*0.48f,0.5f-ny*0.48f+(surface_kind==GRID?synth.draw.uv_shift*6:0),shade,255);
}
static void triangle(Vec a,Vec b,Vec c,u8 shade) {
    Vec n=unit(cross(add(b,scale_vec(a,-1)),add(c,scale_vec(a,-1))));
    Vec p[3]={a,b,c};
    GX_Begin(GX_TRIANGLES,GX_VTXFMT0,3);
    for(int i=0;i<3;i++) normal_vertex(p[i],n,shade);
    GX_End();
}
static void box(Vec center,Vec size,float angle,u8 shade) {
    Vec p[8];
    for(int i=0;i<8;i++) p[i]=add(center,rotate((Vec){i&1?size.x:-size.x,i&2?size.y:-size.y,i&4?size.z:-size.z},angle,angle*0.37f));
    static const int faces[24]={0,2,3,1,4,5,7,6,0,1,5,4,2,6,7,3,0,4,6,2,1,3,7,5};
    for(int f=0;f<6;f++) {
        Vec a=p[faces[f*4]],b=p[faces[f*4+1]],c=p[faces[f*4+2]];
        Vec n=unit(cross(add(b,scale_vec(a,-1)),add(c,scale_vec(a,-1))));
        GX_Begin(GX_QUADS,GX_VTXFMT0,4);
        for(int j=0;j<4;j++) {
            Vec q=p[faces[f*4+j]];
            if(surface_kind==GRID || surface_kind==GHOST)
                vertex(q,(j==1||j==2)?1:0,(j>=2?1:0)+synth.draw.uv_shift*6,shade,255);
            else normal_vertex(q,n,shade);
        }
        GX_End();
    }
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
    Mtx44 p; guPerspective(p,synth.draw.fov,aspect,0.2f,220);
    GX_LoadProjectionMtx(p,GX_PERSPECTIVE);
    Vec eye=orbit_eye(&synth.draw),target=synth.draw.look_at;
    guVector e={eye.x,eye.y,eye.z},at={target.x,target.y,target.z},up={0,1,0};
    Mtx look,bank; guLookAt(look,&e,&up,&at);
    guVector axis={0,0,1}; guMtxRotAxisDeg(bank,&axis,synth.draw.roll);
    guMtxConcat(bank,look,view); GX_LoadPosMtxImm(view,GX_PNMTX0);
}
static void torus(Vec center,float radius,float tube,float a,float b) {
    surface(CHROME);
    for(int i=0;i<32;i++) {
        GX_Begin(GX_TRIANGLESTRIP,GX_VTXFMT0,26);
        for(int j=0;j<=12;j++) for(int k=0;k<2;k++) {
            float u=(i+k)*6.2831853f/32,v=j*6.2831853f/12;
            Vec n={cosf(u)*cosf(v),sinf(u)*cosf(v),sinf(v)};
            Vec p={(radius+tube*cosf(v))*cosf(u),(radius+tube*cosf(v))*sinf(u),tube*sinf(v)};
            normal_vertex(add(center,rotate(p,a,b)),rotate(n,a,b),255);
        }
        GX_End();
    }
}
static void octahedron(Vec center,float size,float angle,int kind,int cage) {
    const Vec axis[6]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    static const int faces[24]={0,2,4,2,1,4,1,3,4,3,0,4,2,0,5,1,2,5,3,1,5,0,3,5};
    Vec p[6],n[6];
    for(int i=0;i<6;i++) {
        n[i]=rotate(axis[i],angle,angle*0.43f);
        float spike=1+fminf(synth.draw.burst,4)*0.08f*(i%2?1:0.4f);
        p[i]=add(center,scale_vec(n[i],size*spike));
    }
    surface(kind);
    GX_Begin(GX_TRIANGLES,GX_VTXFMT0,24);
    for(int i=0;i<24;i++) normal_vertex(p[faces[i]],n[faces[i]],255);
    GX_End();
    if(!cage) return;
    surface(WIRE); GX_SetLineWidth(10,GX_TO_ZERO); GX_SetPointSize(20,GX_TO_ZERO);
    for(int shell=0;shell<2;shell++) {
        for(int i=0;i<6;i++) p[i]=add(center,scale_vec(rotate(axis[i],-angle*(1+shell*0.4f),angle*0.7f+shell*0.5f),size*(1.25f+shell*0.3f)));
        GX_Begin(GX_LINES,GX_VTXFMT0,24);
        for(int i=0;i<6;i++) for(int j=i+1;j<6;j++) if(i/2!=j/2) {
            vertex(p[i],0,0,255,255); vertex(p[j],0,0,255,255);
        }
        GX_End();
        GX_Begin(GX_POINTS,GX_VTXFMT0,6);
        for(int i=0;i<6;i++) vertex(p[i],0,0,255,255);
        GX_End();
    }
}
static void stellar(Vec center,float size,float angle) {
    octahedron(center,size*0.65f,angle,CHROME,0);
    for(int i=0;i<10;i++) {
        float a=i*2.3999632f,y=1-2*(i+0.5f)/10;
        Vec direction=rotate((Vec){sqrtf(1-y*y)*cosf(a),y,sqrtf(1-y*y)*sinf(a)},angle,angle*0.6f);
        Vec tangent=unit(cross(direction,(Vec){0,1,0})),side=cross(direction,tangent);
        Vec base=add(center,scale_vec(direction,size*0.45f));
        Vec tip=add(center,scale_vec(direction,size*(2.1f+(i%3)*0.7f)));
        Vec corner[4];
        for(int j=0;j<4;j++) corner[j]=add(base,add(scale_vec(tangent,(j&1?1:-1)*size*0.23f),scale_vec(side,(j&2?1:-1)*size*0.23f)));
        surface(i%4==0?MATTE:CHROME);
        const int order[4]={0,1,3,2};
        for(int j=0;j<4;j++) triangle(corner[order[j]],corner[order[(j+1)%4]],tip,255);
    }
}
static void typo(Vec center,const char *word,float angle) {
    surface(CHROME);
    int count=strlen(word); float cell=8.5f/(count*6-1);
    for(int k=0;k<count;k++) for(int row=0;row<5;row++) for(int col=0;col<5;col++)
        if(font_glyph(word[k])[row]&(1<<(4-col))) {
            Vec offset={(k*6+col-(count*6-2)*0.5f)*cell,(2-row)*cell,0};
            box(add(center,rotate(offset,angle,angle*0.37f)),(Vec){cell*0.44f,cell*0.44f,cell*0.8f},angle,255);
        }
}
static void mega(void) {
    static const int arrangement[5][3]={{0,1,2},{0,0,0},{1,2,2},{2,0,1},{3,0,1}};
    for(int i=0;i<3;i++) {
        Vec center=synth.draw.objects[i];
        float jitter=fmaxf(0,synth.tension-0.4f)*0.3f;
        center.x+=jitter*sinf(synth.tick*2.4f+i); center.y+=jitter*cosf(synth.tick*2.1f+i);
        float phase=synth.draw.phase*(i%2?-1:1)+i*0.8f;
        int kind=arrangement[synth.shape][i];
        float size=(i==0?1.35f:1.05f);
        if(kind==0) {
            torus(center,2.4f*size,0.85f*size,phase,phase*0.45f);
            torus(center,1.5f*size,0.55f*size,-phase*1.3f,1.57f+phase*0.6f);
        } else if(kind==1) octahedron(center,3*size,phase,i==2?MATTE:CHROME,1);
        else if(kind==2) stellar(center,1.55f*size,phase);
        else typo(center,(const char *[]){".+","HYPER","VOID"}[synth.cut%3],phase*0.3f);
    }
}
static void swarm(void) {
    Vec eye=orbit_eye(&synth.draw);
    Vec scroll=add(synth.draw.world_cam,eye);
    for(int z=0;z<4;z++) for(int y=0;y<4;y++) for(int x=0;x<4;x++) {
        Vec center=add(eye,lattice_relative((Vec){x*24,y*24,z*24},scroll));
        // A protected sphere encloses all subjects, including the longest spikes and beams.
        if(center.x*center.x+center.y*center.y+center.z*center.z<32*32) continue;
        int index=x+y*4+z*16;
        surface(index%7==0?MATTE:index%5==0?GHOST:GRID);
        float width=0.6f+synth.overdrive*0.9f;
        box(center,(Vec){12,width,width},0,255);
        box(center,(Vec){width,12,width},0,255);
        if(synth.gear>=1 || index%2==0) box(center,(Vec){width,width,12},0,255);
        if(index%4==0) octahedron(center,3,synth.draw.phase*0.2f,index%8?GHOST:GRID,0);
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
    ortho(); material(NULL); GX_SetNumTevStages(1); GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
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
    ortho(); material(NULL); GX_SetNumTevStages(1); GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
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
    projection(); GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
    swarm(); mega();
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
    GX_InitTexObj(&chrome_tex,chrome_pixels,128,128,GX_TF_RGB565,GX_CLAMP,GX_CLAMP,GX_FALSE);
    GX_InitTexObjLOD(&chrome_tex,GX_LINEAR,GX_LINEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    GX_InitTexObj(&grid_tex,grid_pixels,64,64,GX_TF_RGB565,GX_REPEAT,GX_REPEAT,GX_FALSE);
    GX_InitTexObjLOD(&grid_tex,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
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
