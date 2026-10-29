#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include "controls.h"
#include "midi.h"
#include "font.h"
#include "chrome.h"
#include "grid.h"
#include "geometry.h"
#include "render_plan.h"
#include "profile.h"
#include <assert.h>
#include <stdio.h>

static Synth synth;
static MidiInput midi_input;
static DualInput dual_input;
static Palette palette;
static int background_role;
#ifdef GEEZY_SCULPTURE_CHECK
static unsigned check_sculpture;
#endif
static volatile int quitting;
static GXRModeObj *mode;
static GXTexObj chrome_tex, grid_tex[3], history, dither_tex, trail_tex, halftone_tex;
static GXTexObj freeze_tex;
static u16 chrome_pixels[128*128] ATTRIBUTE_ALIGN(32);
static u16 grid_pixels[3][GRID_TEXELS] ATTRIBUTE_ALIGN(32);
static u8 dither_pixels[64] ATTRIBUTE_ALIGN(32);
static u8 halftone_pixels[512] ATTRIBUTE_ALIGN(32);
static void *history_pixels, *trail_pixels;
static void *freeze_pixels;
static int freeze_valid;
#ifdef GEEZY_MASTER_CHECK
static unsigned master_scene_calls,master_copy_calls;
#endif
static int trail_valid, shell_surface;
static int quantize_vertices, in_3d;
static float aspect;
static Mtx view;
static Vec scene_eye;
static GXColor tint={255,255,255,255};
enum { CHROME, GRID, WIRE, MATTE, GHOST, SOLID };
static int surface_kind, primary_surface, sculpture_surface;
static void power(void) { quitting=1; }
static void reset_button(u32 irq,void *ctx) { (void)irq; (void)ctx; quitting=1; }

static u16 rgb565(float r,float g,float b) {
    return ((u16)(clamp01(r)*31)<<11)|((u16)(clamp01(g)*63)<<5)|(u16)(clamp01(b)*31);
}
static GXColor gx_color(ChromeColor c) { return (GXColor){clamp01(c.r)*255,clamp01(c.g)*255,clamp01(c.b)*255,255}; }
static ChromeColor palette_role(int role) { role=(role%3+3)%3; return role==0?palette.primary:role==1?palette.secondary:palette.accent; }
static GXColor clear_color(void) {
    if(synth.theme==1 || synth.theme==3) return (GXColor){255,255,255,255};
    return synth.theme==2?gx_color(palette.bg_tint):(GXColor){0,0,0,255};
}
static void make_metal(void) {
    palette=palette_generate(synth.seed,synth.harmony_mode);
    for(int y=0;y<128;y++) for(int x=0;x<128;x++) {
        float nx=(x-63.5f)/63.5f,ny=chrome_normal_y(x,y);
        Palette local=palette;
        if(synth.reflection==2 || (synth.reflection==0 && synth.palette_count && synth.palette_count%4==0)) {
            float shift=(atan2f(ny,nx)+3.14159265f)*57.29578f;
            local=palette_generate(synth.seed+(uint32_t)(shift*100),synth.harmony_mode);
        }
        ChromeColor color=chrome_palette(nx,ny,local,synth.harmony_mode);
        float light=fmaxf(color.r,fmaxf(color.g,color.b));
        if(synth.reflection==1) color=light<0.18f?(ChromeColor){0,0,0}:light<0.5f?local.primary:light<0.8f?local.secondary:(ChromeColor){1,1,1};
        if(synth.reflection==3) color=(ChromeColor){light,light,light};
        chrome_pixels[((y/4)*32+x/4)*16+(y%4)*4+x%4]=rgb565(color.r,color.g,color.b);
    }
    for(int role=0;role<3;role++) {
        ChromeColor edge_color=palette_role(role),line_color=palette_role(role+1);
        for(int y=0;y<64;y++) for(int x=0;x<64;x++) {
            int edge=x<2||y<2||x>=62||y>=62,line=(x>=31&&x<=32)||(y>=31&&y<=32);
            ChromeColor c=edge?edge_color:line?line_color:(ChromeColor){0,0,0};
            grid_pixels[role][grid_index(64,x,y)]=rgb565(c.r,c.g,c.b);
        }
        grid_mipmaps(grid_pixels[role]);
    }
    DCFlushRange(chrome_pixels,sizeof(chrome_pixels)); DCFlushRange(grid_pixels,sizeof(grid_pixels)); GX_InvalidateTexAll();
    GX_SetCopyClear(clear_color(),0x00ffffff);
    trail_valid=0;
}
#include "tev.h"
static void material(GXTexObj *texture) {
    detail_kind=0; GX_SetNumIndStages(0);
    for(int i=0;i<4;i++) { GX_SetTevDirect(i); GX_SetTevSwapMode(i,GX_TEV_SWAP0,GX_TEV_SWAP0); }
    tint=(GXColor){255,255,255,255};
    GX_SetNumTevStages(1); GX_SetTevSwapMode(GX_TEVSTAGE0,GX_TEV_SWAP0,GX_TEV_SWAP0); GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,GX_DIRECT); GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT); GX_SetNumTexGens(texture?1:0);
    GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GX_SetTevOrder(GX_TEVSTAGE0,texture?GX_TEXCOORD0:GX_TEXCOORDNULL,texture?GX_TEXMAP0:GX_TEXMAP_NULL,GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0,texture?GX_MODULATE:GX_PASSCLR);
    if(texture) GX_LoadTexObj(texture,GX_TEXMAP0);
}
static void surface(int kind) {
    if(primary_surface && synth.skin==3) kind=WIRE;
    if(primary_surface && kind==CHROME) kind=(int[]){CHROME,GRID,GHOST,WIRE}[synth.skin];
    if(!primary_surface && kind==GRID) {
        if(synth.theme==1) kind=background_role%2?MATTE:SOLID;
        if(synth.theme==2) kind=CHROME;
    }
    if(primary_surface && synth.theme==2 && kind!=WIRE && kind!=MATTE) kind=SOLID;
    if(shell_surface && synth.theme==1) kind=SOLID;
    if(sculpture_surface) kind=sculpture_surface==1?WIRE:GHOST;
    surface_kind=kind;
    material(kind==CHROME?&chrome_tex:kind==GRID?&grid_tex[background_role%3]:kind==GHOST?&chrome_tex:NULL);
    if(kind==MATTE) tint=(GXColor){0,0,0,255};
    if(kind==WIRE || kind==SOLID) tint=gx_color(primary_surface?palette.accent:palette_role(background_role));
    if(kind==SOLID && synth.theme==1 && !primary_surface) tint=shell_surface?(GXColor){255,255,255,255}:gx_color(palette.primary);
    if(kind==WIRE) { GX_SetLineWidth(10,GX_TO_ZERO); GX_SetPointSize(16,GX_TO_ZERO); }
    if(synth.slow) {
        material(NULL);
        int white=shell_surface?strobe_lit(&synth):!strobe_lit(&synth);
        tint=white?(GXColor){255,255,255,255}:(GXColor){0,0,0,255};
        return;
    }
    if(synth.theme==3 && kind!=MATTE) { material(NULL); tint=gx_color(primary_surface?palette.primary:palette.secondary); }
    if((kind==CHROME || kind==GRID) && synth.theme!=3) detail_material(kind);
    if(kind==GHOST || synth.theme==3) {
        GX_SetNumTevStages(2); GX_SetNumTexGens(2);
        Mtx uv; guMtxIdentity(uv); uv[0][0]=uv[1][1]=synth.theme==3?1:16;
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
    if(in_3d && synth.theme==3) { u=(p.x+p.z*0.37f)*4; v=(p.y+p.z*0.61f)*4; }
    GX_Position3f32(p.x,p.y,p.z); GX_Color4u8(shade*tint.r/255,shade*tint.g/255,shade*tint.b/255,alpha); GX_TexCoord2f32(u,v);
}
static void normal_vertex(Vec p,Vec n,u8 shade) {
    float nx=view[0][0]*n.x+view[0][1]*n.y+view[0][2]*n.z;
    float ny=view[1][0]*n.x+view[1][1]*n.y+view[1][2]*n.z;
    vertex(p,0.5f+nx*0.48f,0.5f-ny*0.48f+(surface_kind==GRID?synth.draw.uv_shift*6:0),shade,255);
}
// All 3D primitives share this guard: WIRE can never submit a filled face.
static void begin_3d(u8 primitive,u16 count) {
    assert(surface_kind!=WIRE || primitive==GX_LINES || primitive==GX_LINESTRIP || primitive==GX_POINTS);
    // Quantum particles keep their line/point contract; hybrid layers may contain faces.
    GX_Begin(primitive,GX_VTXFMT0,count);
}
static void wire_points(const Vec *p,int count,u8 shade) {
    begin_3d(GX_POINTS,count);
    for(int i=0;i<count;i++) vertex(p[i],0,0,shade,255);
    GX_End();
}
static void triangle(Vec a,Vec b,Vec c,u8 shade) {
    Vec n=unit(cross(add(b,scale_vec(a,-1)),add(c,scale_vec(a,-1))));
    Vec p[3]={a,b,c}; int count=surface_kind==WIRE?4:3;
    begin_3d(surface_kind==WIRE?GX_LINESTRIP:GX_TRIANGLES,count);
    for(int i=0;i<count;i++) normal_vertex(p[i%3],n,shade);
    GX_End();
    if(surface_kind==WIRE) wire_points(p,3,shade);
}
#include "gx_mesh.h"
static void copy_texture(void *pixels) {
#ifdef GEEZY_MASTER_CHECK
    master_copy_calls++;
#endif
    profile_copy();
    GX_CopyTex(pixels,GX_FALSE); GX_PixModeSync();
    // The GPU fence orders texture reads; only CPU readback needs GX_DrawDone here.
    if(!gpu_stage2_enabled) GX_DrawDone();
    GX_InvalidateTexAll();
}
static void box_vertices(const Vec p[8],u8 shade) {
    if(surface_kind==WIRE) {
        begin_3d(GX_LINES,24);
        for(int i=0;i<8;i++) for(int bit=1;bit<=4;bit*=2) if(!(i&bit)) {
            vertex(p[i],0,0,shade,255); vertex(p[i|bit],0,0,shade,255);
        }
        GX_End(); wire_points(p,8,shade); return;
    }
    static const int faces[24]={0,2,3,1,4,5,7,6,0,1,5,4,2,6,7,3,0,4,6,2,1,3,7,5};
    for(int f=0;f<6;f++) {
        Vec a=p[faces[f*4]],b=p[faces[f*4+1]],c=p[faces[f*4+2]];
        Vec n=unit(cross(add(b,scale_vec(a,-1)),add(c,scale_vec(a,-1))));
        begin_3d(GX_QUADS,4);
        for(int j=0;j<4;j++) {
            Vec q=p[faces[f*4+j]];
            if(surface_kind==GRID || surface_kind==GHOST)
                vertex(q,(j==1||j==2)?1:0,(j>=2?1:0)+synth.draw.uv_shift*6,shade,255);
            else normal_vertex(q,n,shade);
        }
        GX_End();
    }
}
static void box(Vec center,Vec size,float angle,u8 shade) {
    if(mesh_fast() && cube_bytes[surface_kind==WIRE] && size.x>0 && size.y>0 && size.z>0) {
        Mtx model,rotation; rigid_matrix(center,size,angle,angle*0.37f,model,rotation);
        list_draw(cube_lists[surface_kind==WIRE],cube_bytes[surface_kind==WIRE],model,rotation,1,shade); return;
    }
    Vec p[8];
    for(int i=0;i<8;i++) p[i]=add(center,rotate((Vec){i&1?size.x:-size.x,i&2?size.y:-size.y,i&4?size.z:-size.z},angle,angle*0.37f));
    box_vertices(p,shade);
}
static void beam(Vec a,Vec b,float width,u8 shade) {
    Vec axis=unit(add(b,scale_vec(a,-1)));
    Vec side=scale_vec(unit(cross(axis,fabsf(axis.y)<0.9f?(Vec){0,1,0}:(Vec){1,0,0})),width);
    Vec up=scale_vec(unit(cross(axis,side)),width),p[8];
    if(mesh_fast() && cube_bytes[surface_kind==WIRE] && width>0 && (a.x!=b.x || a.y!=b.y || a.z!=b.z)) {
        Vec half=scale_vec(add(b,scale_vec(a,-1)),0.5f),center=scale_vec(add(a,b),0.5f);
        Mtx model={{side.x,up.x,half.x,center.x},{side.y,up.y,half.y,center.y},{side.z,up.z,half.z,center.z}};
        Vec sx=unit(side),sy=unit(up);
        Mtx rotation={{sx.x,sy.x,axis.x,0},{sx.y,sy.y,axis.y,0},{sx.z,sy.z,axis.z,0}};
        list_draw(cube_lists[surface_kind==WIRE],cube_bytes[surface_kind==WIRE],model,rotation,1,shade); return;
    }
    for(int i=0;i<8;i++) p[i]=add(i&4?b:a,add(scale_vec(side,i&1?1:-1),scale_vec(up,i&2?1:-1)));
    box_vertices(p,shade);
}
static void ortho(void) {
    in_3d=0; quantize_vertices=0; GX_SetFog(GX_FOG_NONE,0,1,0,1,(GXColor){0,0,0,255});
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
static void feedback_trail(void) {
    if(!trail_valid || synth.slow || synth.theme!=0) return;
    ortho(); material(&trail_tex);
    GX_SetNumTexGens(3); Mtx base; guMtxIdentity(base); detail_uv(base,GX_TG_TEX0); indirect_stage();
    float amount=0.25f+0.55f*synth.overdrive+0.025f*synth.gear;
    float zoom=1.02f+0.04f*synth.overdrive,angle=0.008f+synth.overdrive*0.018f;
    // Subtract before gain: black remains exactly zero and old highlights eventually disappear.
    GX_SetTevColor(GX_TEVREG0,(GXColor){24,24,24,255});
    GX_SetTevColorIn(GX_TEVSTAGE0,GX_CC_C0,GX_CC_ZERO,GX_CC_ZERO,GX_CC_TEXC);
    GX_SetTevColorOp(GX_TEVSTAGE0,GX_TEV_SUB,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
    GX_SetNumTevStages(2);
    GX_SetTevOrder(GX_TEVSTAGE1,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLOR0A0);
    GX_SetTevColorIn(GX_TEVSTAGE1,GX_CC_ZERO,GX_CC_CPREV,GX_CC_RASC,GX_CC_ZERO);
    GX_SetTevColorOp(GX_TEVSTAGE1,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
    GX_SetTevAlphaIn(GX_TEVSTAGE1,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_APREV);
    GX_SetTevAlphaOp(GX_TEVSTAGE1,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
    GX_SetBlendMode(GX_BM_BLEND,GX_BL_ONE,GX_BL_ONE,GX_LO_COPY);
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
    for(int i=0;i<4;i++) {
        float x=(i==0||i==3)?-1:1,y=i<2?1:-1;
        float u=(x*cosf(angle)-y*sinf(angle))/zoom,v=(x*sinf(angle)+y*cosf(angle))/zoom;
        vertex((Vec){x,y,-1},0.5f+u*0.5f,0.5f-v*0.5f,(u8)(amount*255),255);
    }
    GX_End(); GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
}
static void projection(void) {
    in_3d=1; quantize_vertices=synth.slow;
    GX_SetFog(synth.theme==1||synth.theme==2?GX_FOG_PERSP_LIN:GX_FOG_NONE,32,110,0.2f,220,clear_color());
    Mtx44 p; guPerspective(p,synth.draw.fov,aspect,0.2f,220);
    GX_LoadProjectionMtx(p,GX_PERSPECTIVE);
    Vec eye=orbit_eye(&synth.draw),target=view_target(&synth.draw),vertical=camera_up(&synth.draw);
    scene_eye=eye;
    guVector e={eye.x,eye.y,eye.z},at={target.x,target.y,target.z},up={vertical.x,vertical.y,vertical.z};
    Mtx look,bank; guLookAt(look,&e,&up,&at);
    float a=synth.draw.roll*0.0174532925f,c=cosf(a),sn=sinf(a);
    guMtxIdentity(bank); bank[0][0]=bank[1][1]=c; bank[0][1]=-sn; bank[1][0]=sn;
    c_guMtxConcat(bank,look,view); GX_LoadPosMtxImm(view,GX_PNMTX0);
}
static Vec mesh_pos[97][13],mesh_normal[97][13];
static int compile_mesh(MeshList *slot,int rows,int cols,float cap_radius,float arc) {
    u8 *data=mesh_lists[slot-mesh_slots]; int wire=slot->key.wire;
    list_format(); DCInvalidateRange(data,MESH_LIST_BYTES); GX_BeginDispList(data,MESH_LIST_BYTES);
    if(wire) {
        for(int i=0;i<=rows;i+=2) {
            GX_Begin(GX_LINESTRIP,GX_VTXFMT1,cols+1);
            for(int j=0;j<=cols;j++) list_vertex(mesh_pos[i][j],mesh_normal[i][j],i*4.0f/rows,j/(float)cols);
            GX_End();
        }
        for(int j=0;j<cols;j++) {
            GX_Begin(GX_LINESTRIP,GX_VTXFMT1,rows+1);
            for(int i=0;i<=rows;i++) list_vertex(mesh_pos[i][j],mesh_normal[i][j],i*4.0f/rows,j/(float)cols);
            GX_End();
        }
        for(int i=0;i<rows;i+=4) {
            GX_Begin(GX_POINTS,GX_VTXFMT1,cols);
            for(int j=0;j<cols;j++) list_vertex(mesh_pos[i][j],mesh_normal[i][j],i*4.0f/rows,j/(float)cols);
            GX_End();
        }
    } else for(int i=0;i<rows;i++) {
        GX_Begin(GX_TRIANGLESTRIP,GX_VTXFMT1,2*(cols+1));
        for(int j=0;j<=cols;j++) for(int k=0;k<2;k++) list_vertex(mesh_pos[i+k][j],mesh_normal[i+k][j],(i+k)*4.0f/rows,j/(float)cols);
        GX_End();
    }
    if(cap_radius>0 && arc<6.28f) for(int end=0;end<2;end++) {
        Vec cap={cap_radius*cosf(end?arc:0),cap_radius*sinf(end?arc:0),0};
        for(int j=0;j<cols;j++) {
            Vec a=mesh_pos[end?rows:0][j],b=mesh_pos[end?rows:0][j+1];
            Vec n=unit(cross(add(a,scale_vec(cap,-1)),add(b,scale_vec(cap,-1))));
            GX_Begin(wire?GX_LINESTRIP:GX_TRIANGLES,GX_VTXFMT1,wire?4:3);
            list_vertex(cap,n,0,0); list_vertex(a,n,0,0); list_vertex(b,n,0,0);
            if(wire) list_vertex(cap,n,0,0);
            GX_End();
            if(wire) {
                GX_Begin(GX_POINTS,GX_VTXFMT1,3);
                list_vertex(cap,n,0,0); list_vertex(a,n,0,0); list_vertex(b,n,0,0); GX_End();
            }
        }
    }
    slot->bytes=GX_EndDispList(); immediate_format();
#ifdef GEEZY_GPU_CHECK
    mesh_builds++;
#endif
    return slot->bytes!=0;
}
static void mesh(int rows,int cols,Vec center,float size,float a,float b,int grid_uv) {
    for(int i=0;i<=rows;i++) for(int j=0;j<=cols;j++) {
        mesh_pos[i][j]=add(center,scale_vec(rotate(mesh_pos[i][j],a,b),size));
        mesh_normal[i][j]=rotate(mesh_normal[i][j],a,b);
    }
    if(surface_kind==WIRE) {
        for(int i=0;i<=rows;i+=2) {
            begin_3d(GX_LINESTRIP,cols+1);
            for(int j=0;j<=cols;j++) normal_vertex(mesh_pos[i][j],mesh_normal[i][j],255);
            GX_End();
        }
        for(int j=0;j<cols;j++) {
            begin_3d(GX_LINESTRIP,rows+1);
            for(int i=0;i<=rows;i++) normal_vertex(mesh_pos[i][j],mesh_normal[i][j],255);
            GX_End();
        }
        for(int i=0;i<rows;i+=4) wire_points(mesh_pos[i],cols,255);
    } else for(int i=0;i<rows;i++) {
        begin_3d(GX_TRIANGLESTRIP,2*(cols+1));
        for(int j=0;j<=cols;j++) for(int k=0;k<2;k++) {
            if(grid_uv && surface_kind==GRID) vertex(mesh_pos[i+k][j],(i+k)*4.0f/rows,j/(float)cols+synth.draw.uv_shift*6,255,255);
            else normal_vertex(mesh_pos[i+k][j],mesh_normal[i+k][j],255);
        }
        GX_End();
    }
}
static void ring(Vec center,float radius,float tube,float a,float b,int sides,float arc) {
    surface(CHROME);
    MeshList *cached=mesh_fast()?mesh_lookup((MeshKey){1,sides,0,surface_kind==WIRE,radius,tube,arc}):NULL;
    if(cached && cached->bytes) { cached_mesh_draw(cached,center,1,a,b); return; }
    for(int i=0;i<=32;i++) for(int j=0;j<=sides;j++) {
        float u=i*arc/32,v=j*6.2831853f/sides;
        mesh_normal[i][j]=(Vec){cosf(u)*cosf(v),sinf(u)*cosf(v),sinf(v)};
        mesh_pos[i][j]=(Vec){(radius+tube*cosf(v))*cosf(u),(radius+tube*cosf(v))*sinf(u),tube*sinf(v)};
    }
    if(cached && compile_mesh(cached,32,sides,radius,arc)) { cached_mesh_draw(cached,center,1,a,b); return; }
    mesh(32,sides,center,1,a,b,0);
    if(arc<6.28f) for(int end=0;end<2;end++) {
        float u=end?arc:0;
        Vec cap=add(center,rotate((Vec){radius*cosf(u),radius*sinf(u),0},a,b));
        for(int j=0;j<sides;j++) triangle(cap,mesh_pos[end?32:0][j],mesh_pos[end?32:0][j+1],255);
    }
}
static void torus(Vec center,float radius,float tube,float a,float b) { ring(center,radius,tube,a,b,12,6.2831853f); }
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
    if(surface_kind==WIRE) {
        begin_3d(GX_LINES,24);
        for(int i=0;i<6;i++) for(int j=i+1;j<6;j++) if(i/2!=j/2) {
            normal_vertex(p[i],n[i],255); normal_vertex(p[j],n[j],255);
        }
        GX_End(); wire_points(p,6,255);
    } else {
        begin_3d(GX_TRIANGLES,24);
        for(int i=0;i<24;i++) normal_vertex(p[faces[i]],n[faces[i]],255);
        GX_End();
    }
    if(!cage) return;
    surface(WIRE); GX_SetLineWidth(10,GX_TO_ZERO); GX_SetPointSize(20,GX_TO_ZERO);
    for(int shell=0;shell<2;shell++) {
        for(int i=0;i<6;i++) p[i]=add(center,scale_vec(rotate(axis[i],-angle*(1+shell*0.4f),angle*0.7f+shell*0.5f),size*(1.25f+shell*0.3f)));
        begin_3d(GX_LINES,24);
        for(int i=0;i<6;i++) for(int j=i+1;j<6;j++) if(i/2!=j/2) {
            vertex(p[i],0,0,255,255); vertex(p[j],0,0,255,255);
        }
        GX_End();
        begin_3d(GX_POINTS,6);
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
static void frame_cluster(Vec center,float angle,float scale) {
    surface(CHROME);
    for(int shell=0;shell<2;shell++) for(int side=0;side<4;side++) {
        float a=angle*(shell?-1:1),radius=2.8f-shell*0.9f;
        Vec offset={side<2?(side?radius:-radius):0,side>=2?(side==3?radius:-radius):0,0};
        Vec size=side<2?(Vec){0.4f,radius+0.4f,0.55f}:(Vec){radius-0.4f,0.4f,0.55f};
        box(add(center,scale_vec(rotate(offset,a,a*0.37f),scale)),scale_vec(size,scale),a,255);
    }
}
static void knot(Vec center,float size,float angle,Sculpture g) {
    surface(CHROME);
    MeshList *cached=mesh_fast()?mesh_lookup((MeshKey){2,g.p,g.q,surface_kind==WIRE,g.tube,0,0}):NULL;
    if(cached && cached->bytes) { cached_mesh_draw(cached,center,size,angle,angle*0.43f); return; }
    for(int i=0;i<=96;i++) {
        float t=(i%96)*6.2831853f/96;
        Vec c=knot_center(t,g.p,g.q),normal,side; knot_frame(t,g.p,g.q,&normal,&side);
        for(int j=0;j<=8;j++) {
            float v=j*6.2831853f/8;
            mesh_normal[i][j]=add(scale_vec(normal,cosf(v)),scale_vec(side,sinf(v)));
            mesh_pos[i][j]=add(c,scale_vec(mesh_normal[i][j],g.tube));
        }
    }
    if(cached && compile_mesh(cached,96,8,0,0)) { cached_mesh_draw(cached,center,size,angle,angle*0.43f); return; }
    mesh(96,8,center,size,angle,angle*0.43f,0);
}
static void polygon_slab(Vec center,float radius,float height,float angle,int sides) {
    Vec p[24];
    for(int layer=0;layer<2;layer++) for(int i=0;i<sides;i++) {
        float a=i*6.2831853f/sides;
        p[layer*sides+i]=add(center,rotate((Vec){radius*cosf(a),layer?height:-height,radius*sinf(a)},angle,angle*0.37f));
    }
    if(surface_kind==WIRE) {
        begin_3d(GX_LINES,sides*6);
        for(int i=0;i<sides;i++) {
            for(int layer=0;layer<2;layer++) {
                vertex(p[layer*sides+i],0,0,255,255); vertex(p[layer*sides+(i+1)%sides],0,0,255,255);
            }
            vertex(p[i],0,0,255,255); vertex(p[sides+i],0,0,255,255);
        }
        GX_End(); wire_points(p,sides*2,255); return;
    }
    for(int i=0;i<sides;i++) {
        int next=(i+1)%sides;
        triangle(p[i],p[sides+i],p[next],255);
        triangle(p[next],p[sides+i],p[sides+next],255);
        for(int layer=0;layer<2;layer++) {
            Vec cap=add(center,rotate((Vec){0,layer?height:-height,0},angle,angle*0.37f));
            if(layer) triangle(cap,p[sides+next],p[sides+i],255);
            else triangle(cap,p[i],p[next],255);
        }
    }
}
static void sliced(Vec center,float size,float angle,Sculpture g) {
    for(unsigned i=0;i<g.slices;i++) {
        float h=(i+0.5f)*2/g.slices-1;
        float radius=g.profile==0?sqrtf(1-h*h):g.profile==1?1:1-fabsf(h);
        float shift=(i%2?1:-1)*(0.15f+fminf(synth.draw.burst,4)*0.5f)*sinf(angle*2+i*0.4f);
        Vec offset=rotate((Vec){shift,h*2.8f,0},angle,angle*0.37f);
        surface(i%5==0?MATTE:CHROME);
        Vec c=add(center,scale_vec(offset,size));
        if(g.profile==1) box(c,(Vec){radius*2.6f*size,2.5f*size/g.slices,radius*2.6f*size},angle,255);
        else polygon_slab(c,radius*2.6f*size,2.5f*size/g.slices,angle,g.profile==0?12:4);
    }
}
static void glyph_totem(Vec center,float size,float angle,Sculpture g) {
    const char *words[]={"HYPER","VOID",".+","01","ERR","WII",NULL};
    char symbols[4]={".+:-|"[synth.seed%5],"01+[]"[(synth.seed>>4)%5],".+:-|"[(synth.seed>>8)%5],0};
    const char *word=words[g.glyph]?words[g.glyph]:symbols;
    int count=strlen(word); float dot=7.0f/(count*6);
    surface(CHROME);
    for(int i=0;i<count;i++) for(int y=0;y<5;y++) for(int x=0;x<5;x++) if(font_glyph(word[i])[y]&(1<<(4-x))) {
        Vec offset={(i*6+x-(count*6-2)*0.5f)*dot,(2-y)*dot,0};
        box(add(center,scale_vec(rotate(offset,angle,angle*0.37f),size)),
            (Vec){dot*0.46f*size,dot*0.46f*size,(0.4f+g.tube)*size},angle,255);
    }
}
static void polyform(Vec center,float size,float angle,Sculpture g) {
    surface(CHROME);
    MeshList *cached=mesh_fast() && gpu_stage2_enabled?mesh_lookup((MeshKey){3,g.symmetry,0,surface_kind==WIRE,g.sharpness,0,0}):NULL;
    if(cached && cached->bytes) cached_mesh_draw(cached,center,size,angle,angle*0.37f);
    else {
        for(int i=0;i<=32;i++) for(int j=0;j<=12;j++) {
            float u=(i%32)*6.2831853f/32-3.14159265f,v=j*3.14159265f/12-1.5707963f;
            float radius=super_radius(u,g.symmetry,g.sharpness)*super_radius(v,4,g.sharpness);
            Vec n={cosf(v)*cosf(u),sinf(v),cosf(v)*sinf(u)};
            mesh_pos[i][j]=scale_vec(n,2.8f*radius); mesh_normal[i][j]=n;
        }
        // Geometric neighbor normals keep chrome reflections attached to the changing lobes.
        for(int i=0;i<32;i++) for(int j=1;j<12;j++) {
            Vec du=add(mesh_pos[(i+1)%32][j],scale_vec(mesh_pos[(i+31)%32][j],-1));
            Vec dv=add(mesh_pos[i][j+1],scale_vec(mesh_pos[i][j-1],-1));
            mesh_normal[i][j]=unit(cross(dv,du));
        }
        for(int j=0;j<=12;j++) mesh_normal[32][j]=mesh_normal[0][j];
        if(cached && compile_mesh(cached,32,12,0,0)) cached_mesh_draw(cached,center,size,angle,angle*0.37f);
        else mesh(32,12,center,size,angle,angle*0.37f,0);
    }
    for(unsigned i=0;i<g.profile+2;i++) {
        Vec offset=rotate((Vec){(i%2?1:-1)*3.0f,1.4f-i*1.1f,0.5f},angle,angle*0.37f);
        octahedron(add(center,scale_vec(offset,size)),size*(0.5f+g.tube),-angle, i%2?MATTE:CHROME,0);
    }
}
#include "sculptures.h"
static void sculpture_draw(Vec center,float size,float phase,Sculpture g) {
    switch(g.family) {
    case 0:
        torus(center,2.4f*size,(0.6f+g.tube)*size,phase,phase*0.45f);
        torus(center,1.5f*size,g.tube*size,-phase*1.3f,1.57f+phase*0.6f); break;
    case 1: octahedron(center,3*size,phase,CHROME,1); break;
    case 2: stellar(center,(1.1f+g.tube)*size,phase); break;
    case 3: frame_cluster(center,phase,size); break;
    case 4: knot(center,size,phase,g); break;
    case 5: sliced(center,size,phase,g); break;
    case 6:
        for(unsigned r=0;r<g.rings;r++) ring(center,(1.4f+r*0.48f)*size,(g.tube*0.7f+r*0.04f)*size,
            phase*(r%2?-1:1)+r*0.6f,phase*0.6f+r*1.1f,g.sides,(r%2?1.6f:2)*3.14159265f);
        break;
    case 7: glyph_totem(center,size,phase,g); break;
    case 8: polyform(center,size,phase,g); break;
    case 9: pipe_labyrinth(center,size,phase,g); break;
    case 10: mechanical_helix(center,size,phase,g); break;
    case 11: menger_cage(center,size,phase,g); break;
    case 12: twisted_mesh(center,size,phase,g,1); break;
    case 13: mechanical_spine(center,size,phase,g); break;
    }
}
static void mega(void) {
    primary_surface=1;
    int count=subject_count(synth.layout);
    for(int i=0;i<count;i++) {
        Vec center=synth.draw.objects[i];
        float jitter=fmaxf(0,synth.tension-0.4f)*0.3f;
        center.x+=jitter*sinf(synth.tick*2.4f+i); center.y+=jitter*cosf(synth.tick*2.1f+i);
        float phase=synth.draw.phase*(i%2?-1:1)+i*0.8f;
        float size=synth.layout==1?2.0f:synth.layout==2?1.4f:(i==0?1.35f:1.05f);
        Sculpture g=sculpture(synth.seed,synth.shape,i);
#ifdef GEEZY_SCULPTURE_CHECK
        g.family=check_sculpture;
#endif
        sculpture_draw(center,size,phase,g);
        if(synth.layout==3) for(unsigned j=0;j<g.rings;j++) {
            float a=phase*1.4f+j*6.2831853f/g.rings;
            Vec offset=rotate((Vec){7*cosf(a),7*sinf(a),0},phase*0.4f,0.7f);
            Sculpture satellite=sculpture(synth.seed,synth.shape,j+1);
            sculpture_draw(add(center,scale_vec(offset,1-synth.tension*0.65f)),0.35f, -phase+j,satellite);
        }
    }
    primary_surface=0;
}
static void rail_segment(Vec fixed,Vec eye,int axis,float start,float end,float width,float world,float flow) {
    if(end<=start) return;
    int steps=(int)ceilf((end-start)/24);
    const int corner[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
    // Open side walls: no coincident end caps at tile boundaries or crossings.
    for(int i=0;i<steps;i++) {
        float a=start+(end-start)*i/steps,b=start+(end-start)*(i+1)/steps;
        Vec mid=fixed;
        if(axis==0) mid.x=(a+b)*0.5f;
        else if(axis==1) mid.y=(a+b)*0.5f;
        else mid.z=(a+b)*0.5f;
        float depth=view[2][0]*mid.x+view[2][1]*mid.y+view[2][2]*mid.z+view[2][3];
        if(depth>fabsf(view[2][axis])*(b-a)*0.5f+width*4) continue;
        Vec p[8]; u8 shade[8];
        for(int j=0;j<8;j++) {
            int c=j%4; float along=j<4?a:b;
            p[j]=fixed;
            float w=width*(synth.beam_style==2?3.0f:1),h=width*(synth.beam_style==2?0.28f:1);
            if(axis==0) { p[j].x=along; p[j].y+=corner[c][0]*w; p[j].z+=corner[c][1]*h; }
            else if(axis==1) { p[j].y=along; p[j].z+=corner[c][0]*w; p[j].x+=corner[c][1]*h; }
            else { p[j].z=along; p[j].x+=corner[c][0]*w; p[j].y+=corner[c][1]*h; }
            shade[j]=(u8)(255*lattice_fade(p[j],eye));
        }
        begin_3d(GX_QUADS,16);
        for(int side=0;side<4;side++) for(int j=0;j<4;j++) {
            int c=(side+(j==1||j==2))%4+(j>=2?4:0);
            vertex(p[c],(j==1||j==2)?1:0,((j<2?a:b)+world)/24+flow,shade[c],255);
        }
        GX_End();
    }
}
static void truss_segment(Vec center,Vec diagonal,Vec eye,float width,float flow,float start,float end) {
    if(end<=start) return;
    float length=sqrtf(diagonal.x*diagonal.x+diagonal.y*diagonal.y+diagonal.z*diagonal.z);
    Vec tangent=scale_vec(unit(cross(diagonal,fabsf(diagonal.y)<1?(Vec){0,1,0}:(Vec){0,0,1})),width);
    Vec side=scale_vec(unit(cross(diagonal,tangent)),width);
    Vec p[8]; u8 shades[8]; int visible=0;
    const int signs[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
    for(int i=0;i<8;i++) {
        p[i]=add(center,add(scale_vec(diagonal,i<4?start:end),add(scale_vec(tangent,signs[i%4][0]),scale_vec(side,signs[i%4][1]))));
        shades[i]=(u8)(255*lattice_fade(p[i],eye)); visible|=shades[i];
    }
    if(!visible) return;
    begin_3d(GX_QUADS,16);
    for(int face=0;face<4;face++) for(int j=0;j<4;j++) {
        int c=(face+(j==1||j==2))%4+(j>=2?4:0);
        vertex(p[c],(j==1||j==2)?1:0,(j<2?start:end)*length/24+flow,shades[c],255);
    }
    GX_End();
}
static void truss_beam(Vec center,Vec diagonal,Vec eye,float width,float flow) {
    float lo,hi;
    if(beam_gap(center,diagonal,&lo,&hi)) {
        truss_segment(center,diagonal,eye,width,flow,-1,fminf(1,lo));
        truss_segment(center,diagonal,eye,width,flow,fmaxf(-1,hi),1);
    } else truss_segment(center,diagonal,eye,width,flow,-1,1);
}
static void trusses(Vec world,Vec eye,float flow) {
    // A 24-unit cell repeats across the 96-unit world wrap in every axis.
    for(int x=0;x<8;x++) for(int y=0;y<8;y++) for(int z=0;z<8;z++) {
        Vec center={lattice_axis(x,world.x,eye.x),lattice_axis(y,world.y,eye.y),lattice_axis(z,world.z,eye.z)};
        Vec relative=add(center,scale_vec(eye,-1));
        if(relative.x*relative.x+relative.y*relative.y+relative.z*relative.z>104*104) continue;
        if(view[2][0]*center.x+view[2][1]*center.y+view[2][2]*center.z+view[2][3]>20) continue;
        float sign=(x+y+z)%2?1:-1;
        float width=0.28f+synth.overdrive*0.22f;
        background_role=0; surface(GRID);
        truss_beam(center,(Vec){11.5f,11.5f*sign,0},eye,width,flow);
        background_role=1; surface(GRID);
        truss_beam(center,(Vec){0,11.5f,11.5f*sign},eye,width,flow);
        background_role=2; surface(GRID);
        truss_beam(center,(Vec){11.5f*sign,0,11.5f},eye,width,flow);
    }
}
static void swarm(void) {
    Vec eye=scene_eye;
    Vec world=synth.slow?synth.draw.world_cam:synth.live.world_cam;
    float flow=(synth.slow?synth.draw.uv_shift:synth.live.uv_shift)*2;
    quantize_vertices=0; background_role=0; surface(GRID);
    if(synth.beam_style==3) { trusses(world,eye,flow); quantize_vertices=synth.slow; return; }
    for(int axis=0;axis<3;axis++) for(int i=0;i<8;i++) for(int j=0;j<8;j++) {
        if(!beam_axis(synth.beam_style,axis)) continue;
        background_role=axis; surface(GRID);
        Vec fixed={0,0,0}; float origin,scroll,perp,view_perp;
        if(axis==0) {
            fixed.y=lattice_axis(i,world.y,eye.y); fixed.z=lattice_axis(j,world.z,eye.z);
            origin=eye.x; scroll=world.x;
            perp=fixed.y*fixed.y+fixed.z*fixed.z;
            view_perp=(fixed.y-eye.y)*(fixed.y-eye.y)+(fixed.z-eye.z)*(fixed.z-eye.z);
        } else if(axis==1) {
            fixed.z=lattice_axis(i,world.z,eye.z); fixed.x=lattice_axis(j,world.x,eye.x);
            origin=eye.y; scroll=world.y;
            perp=fixed.z*fixed.z+fixed.x*fixed.x;
            view_perp=(fixed.z-eye.z)*(fixed.z-eye.z)+(fixed.x-eye.x)*(fixed.x-eye.x);
        } else {
            fixed.x=lattice_axis(i,world.x,eye.x); fixed.y=lattice_axis(j,world.y,eye.y);
            origin=eye.z; scroll=world.z;
            perp=fixed.x*fixed.x+fixed.y*fixed.y;
            view_perp=(fixed.x-eye.x)*(fixed.x-eye.x)+(fixed.y-eye.y)*(fixed.y-eye.y);
        }
        if(view_perp>86*86) continue; // Already fully black before a rail is recycled.
        float reach=sqrtf(86*86-view_perp);
        float start=origin-reach,end=origin+reach;
        float width=(0.65f+synth.overdrive*0.5f)*(1+axis*0.12f);
        if(perp<20*20) {
            float hole=sqrtf(20*20-perp);
            rail_segment(fixed,eye,axis,start,fminf(end,-hole),width,scroll,flow);
            rail_segment(fixed,eye,axis,fmaxf(start,hole),end,width,scroll,flow);
        } else rail_segment(fixed,eye,axis,start,end,width,scroll,flow);
    }
    quantize_vertices=synth.slow;
}
static void vector_tunnel(void) {
    const Pose *p=synth.slow?&synth.draw:&synth.live;
    Vec eye=scene_eye,direction=unit(p->travel);
    float distance=sqrtf(p->travel.x*p->travel.x+p->travel.y*p->travel.y+p->travel.z*p->travel.z);
    if(distance<0.0001f) direction=synth.flow;
    Vec tangent=unit(cross(direction,fabsf(direction.y)<0.9f?(Vec){0,1,0}:(Vec){1,0,0}));
    Vec side=cross(direction,tangent);
    float length=0.7f+fminf(distance,2.8f);
    // Fixed world-space rings; fade completely before recycling any axis.
    quantize_vertices=0; background_role=2; surface(WIRE);
    for(int i=0;i<192;i++) {
        Vec center=synth.backdrop==3?matrix_particle(i,p->world_cam,eye):tunnel_particle(i,p->world_cam,eye);
        float fade=particle_fade(center,eye);
        if(fade<0.004f) continue;
        u8 shade=(u8)(fade*255);
        Vec tip=add(center,scale_vec(direction,-length));
        Vec tail=add(center,scale_vec(direction,length));
        Vec a=add(tail,scale_vec(tangent,0.32f)),b=add(tail,scale_vec(tangent,-0.32f));
        Vec c=add(tail,scale_vec(side,0.32f));
        triangle(tip,a,c,shade); triangle(tip,c,b,shade); triangle(tip,b,a,shade);
    }
    quantize_vertices=synth.slow;
}
static Vec dome_directions[13][24];
static float dome_sine[13][24],dome_cosine[13][24];
static void dome_init(void) {
    for(int row=0;row<13;row++) for(int col=0;col<24;col++) {
        Vec n=dome_directions[row][col]=dome_formula(row,col);
        float phase=n.x*5+n.z*3;
        dome_sine[row][col]=sinf(phase); dome_cosine[row][col]=cosf(phase);
    }
}
static void dome(void) {
    Vec eye=scene_eye;
    float phase_sine=sinf(synth.draw.phase),phase_cosine=cosf(synth.draw.phase);
    quantize_vertices=0; shell_surface=1;
    GX_SetFog(GX_FOG_NONE,0,1,0,1,clear_color());
    GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
    for(int row=0;row<12;row++) for(int col=0;col<24;col++) {
        background_role=(row+col)%3; surface((row+col+synth.shape)%3?CHROME:GRID);
        u8 shade=(row+col)%7==0?0:255;
        begin_3d(GX_QUADS,4);
        for(int j=0;j<4;j++) {
            int r=row+(j>=2),c=(col+(j==1||j==2))%24;
            Vec n=gpu_stage3_enabled?dome_directions[r][c]:dome_formula(r,c);
            float wave=(gpu_stage3_enabled?dome_sine[r][c]*phase_cosine+dome_cosine[r][c]*phase_sine:
                sinf(n.x*5+n.z*3+synth.draw.phase))*8;
            Vec pos=add(eye,scale_vec(n,150+wave));
            if(surface_kind==GRID) vertex(pos,(j==1||j==2)?1:0,(j>=2?1:0)+synth.draw.uv_shift*3,shade,255);
            else normal_vertex(pos,scale_vec(n,-1),shade);
        }
        GX_End();
    }
    shell_surface=0; quantize_vertices=synth.slow;
    GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
}
static void warp_tunnel(void) {
    const Pose *p=synth.slow?&synth.draw:&synth.live;
    Vec eye=scene_eye,axis=p->tunnel_axis;
    Vec right=unit(cross(axis,fabsf(axis.y)<0.9f?(Vec){0,1,0}:(Vec){1,0,0})),up=cross(right,axis);
    int sides=(int[]){6,8,12,4}[(synth.beam_style+(synth.seed>>4))%4];
    quantize_vertices=0;
    for(int ring=0;ring<24;ring++) {
        float depth=tunnel_depth(ring,p->tunnel_distance),fade=tunnel_fade(depth);
        if(fade<=0) continue;
        Vec center=add(eye,scale_vec(axis,depth));
        float twist=ring*6.2831853f/12+synth.draw.phase*0.2f;
        float radius=14+(ring%3)*0.8f;
        background_role=ring%3; surface(synth.theme==1?GRID:ring%3==0?CHROME:GRID);
        for(int side=0;side<sides;side++) {
            float a=side*6.2831853f/sides+twist,b=(side+1)*6.2831853f/sides+twist;
            Vec start=add(center,add(scale_vec(right,cosf(a)*radius),scale_vec(up,sinf(a)*radius)));
            Vec end=add(center,add(scale_vec(right,cosf(b)*radius),scale_vec(up,sinf(b)*radius)));
            beam(start,end,0.45f+synth.overdrive*0.45f,(u8)(255*fade));
        }
    }
    quantize_vertices=synth.slow;
}
static void mask_rect(float x,float y,float w,float h) {
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
    vertex((Vec){x,y,-1},0,0,255,255); vertex((Vec){x+w,y,-1},1,0,255,255);
    vertex((Vec){x+w,y+h,-1},1,1,255,255); vertex((Vec){x,y+h,-1},0,1,255,255);
    GX_End();
}
#include "overlay.h"
#include "worlds.h"
#include "master_fx.h"
static void small_text(const char *text,float x,float y) {
    float dot=0.008f;
    for(int i=0;text[i];i++) for(int row=0;row<5;row++) for(int col=0;col<5;col++)
        if(font_glyph(text[i])[row]&(1<<(4-col)))
            mask_rect(x+(i*6+col)*dot,y-(row+1)*dot*aspect,dot,dot*aspect);
}
static void telemetry(void) {
    ortho(); material(NULL); GX_SetNumTevStages(1); GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    tint=synth.theme==1||synth.theme==3?(GXColor){0,0,0,255}:(GXColor){255,255,255,255};
    GXColor ink=tint; tint=synth.theme==1||synth.theme==3?(GXColor){255,255,255,255}:(GXColor){0,0,0,255};
    mask_rect(-0.97f,0.66f,0.76f,0.31f); mask_rect(0.15f,0.66f,0.82f,0.31f);
    mask_rect(-0.97f,-0.94f,0.9f,0.14f); mask_rect(0.63f,-0.94f,0.34f,0.14f); tint=ink;
    char text[40];
    float cruise=(float[]){3,12,36,72}[synth.gear]*synth.boost;
    float speed=synth.slow?0:sqrtf(synth.velocity.x*synth.velocity.x+synth.velocity.y*synth.velocity.y+synth.velocity.z*synth.velocity.z)+cruise;
    snprintf(text,sizeof(text),"VEL:%04.0f",fminf(speed,9999)); small_text(text,-0.94f,0.94f);
    snprintf(text,sizeof(text),"GEAR:%d",synth.gear); small_text(text,-0.94f,0.84f);
    snprintf(text,sizeof(text),"FOV:%03.0f",synth.draw.fov); small_text(text,0.94f-strlen(text)*0.048f,0.94f);
    snprintf(text,sizeof(text),"SEED:%04X",(unsigned)(synth.seed&65535)); small_text(text,0.94f-strlen(text)*0.048f,0.84f);
    snprintf(text,sizeof(text),"S%d L%d M%d V%d",synth.shape,synth.layout,synth.skin,synth.motion_mode); small_text(text,-0.94f,0.74f);
    snprintf(text,sizeof(text),"WM%d W%d P%02d X%d",synth.world_mode,synth.theme,synth.palette_bank,synth.mask_pattern); small_text(text,0.94f-strlen(text)*0.048f,0.74f);
    if(synth.tension<=0.7f || (synth.tick/6)%2==0) {
        char bar[11]; for(int i=0;i<10;i++) bar[i]=i<(int)(synth.tension*10)?'|':' '; bar[10]=0;
        snprintf(text,sizeof(text),"TNS:[%s]",bar); small_text(text,-0.94f,-0.82f);
        snprintf(text,sizeof(text),"%03.0f%%",synth.tension*100); small_text(text,0.94f-strlen(text)*0.048f,-0.82f);
    }
#ifdef GEEZY_PROFILE
    tint=synth.theme==1||synth.theme==3?(GXColor){255,255,255,255}:(GXColor){0,0,0,255};
    mask_rect(-0.97f,-0.75f,1.94f,0.42f); tint=ink;
    float *ms=profile_stats.ms;
    snprintf(text,sizeof(text),"HZ:%.1f CPU:%.1f WAIT:%.1f",profile_stats.interval_ms>0?1000/profile_stats.interval_ms:0,
        ms[TIME_INPUT]+ms[TIME_SCENE]+ms[TIME_EFFECTS]+ms[TIME_UI],ms[TIME_WAIT]); small_text(text,-0.94f,-0.36f);
    snprintf(text,sizeof(text),"IN:%.1f 3D:%.1f FX:%.1f UI:%.1f",ms[TIME_INPUT],ms[TIME_SCENE],ms[TIME_EFFECTS],ms[TIME_UI]);
    small_text(text,-0.94f,-0.49f);
    snprintf(text,sizeof(text),"WORK MAX:%.1f MS EFB:%.1f",profile_stats.peak_ms,profile_stats.copies); small_text(text,-0.94f,-0.62f);
#endif
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
}
static void render_scene(void) {
#ifdef GEEZY_STENCIL_CHECK
    ortho(); material(NULL); tint=(GXColor){0,0,255,255}; mask_rect(-1,-1,2,2);
    tint=(GXColor){0,255,0,255}; mask_rect(wrap(synth.tick*0.01f,1.6f),-0.5f,0.1f,0.1f);
    palette.primary=(ChromeColor){1,0,0}; palette.secondary=(ChromeColor){0,1,0}; palette.accent=(ChromeColor){1,1,0};
    synth.mask_pattern=2+(synth.tick/120)%2; synth.mask_env=1; overlay(); return;
#endif
#ifdef GEEZY_TRAIL_CHECK
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    feedback_trail();
    if(synth.tick%240<120) {
        ortho(); material(NULL); mask_rect(-0.3f,-0.3f,0.6f,0.6f);
    }
    copy_texture(trail_pixels);
    trail_valid=1; return;
#endif
#ifdef GEEZY_CHECKER_CHECK
    ortho(); material(NULL);
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    tint=(GXColor){0,0,0,255}; mask_rect(-1,-1,2,2);
    synth.mask_pattern=6; synth.mask_env=1; overlay(); return;
#endif
    if(!(synth.master_fx&FX_FREEZE)) freeze_valid=0;
    RenderPlan plan=render_plan(&synth,freeze_valid);
    if(plan.hold) {
        profile_mark(TIME_SCENE); master_hold(plan.hold); profile_mark(TIME_EFFECTS); return;
    }
#ifdef GEEZY_MASTER_CHECK
    master_scene_calls++;
#endif
    static unsigned last_tick;
    static int last_world;
    if(synth.tick<last_tick || synth.world_mode!=last_world) trail_valid=0;
    last_world=synth.world_mode;
    last_tick=synth.tick;
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    ortho(); material(NULL); tint=clear_color(); mask_rect(-1,-1,2,2);
    if(synth.slow) {
        ortho(); material(NULL);
        tint=strobe_lit(&synth)?(GXColor){255,255,255,255}:(GXColor){0,0,0,255};
        mask_rect(-1,-1,2,2);
        trail_valid=0; // A white strobe must not seed a persistent white trail after release.
    }
    if(synth.world_mode==0) {
        projection(); GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
        if(synth.gear==3 || synth.backdrop==2) { dome(); projection(); }
        if(synth.backdrop==0 || synth.backdrop==2) swarm();
        if(synth.backdrop==1) warp_tunnel();
        if(synth.backdrop!=2) vector_tunnel();
#ifndef GEEZY_LATTICE_CHECK
        feedback_trail();
        projection(); GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
        mega();
#else
        (void)mega; (void)feedback_trail;
#endif
    } else {

        independent_world();
    }
    profile_mark(TIME_SCENE);
    if(plan.history || !gpu_stage2_enabled) copy_texture(history_pixels);
    if(plan.echo) feedback_echo();
#ifndef GEEZY_LATTICE_CHECK
    if(plan.trail || (!gpu_stage2_enabled && !synth.slow)) {
        copy_texture(trail_pixels);
        trail_valid=1;
    } else trail_valid=0;
    profile_mark(TIME_EFFECTS);
    overlay(); master_finish(plan);
    if(synth.hud_visible && !(synth.master_fx&FX_FREEZE)) telemetry();
#ifdef GEEZY_WORLDS_CHECK
    ortho(); material(NULL);
    tint=(GXColor){32+synth.world_mode*32,32+synth.theme*48,17,255}; mask_rect(-1,-1,0.04f,0.04f);
#endif
#else
    (void)overlay; (void)telemetry;
#endif
}
#include "gpu_check.h"
#include "master_check.h"
static void render(void) {
    mesh_frame++;
#if defined(GEEZY_MASTER_CHECK)
    master_check();
#elif defined(GEEZY_GPU_CHECK)
    mesh_calls=mesh_builds=0; gpu_check();
#else
    render_scene();
#endif
}

static unsigned remote_keys(u32 buttons,int vertical) {
    unsigned keys=0;
    const u32 raw[]={WPAD_BUTTON_A,WPAD_BUTTON_B,WPAD_BUTTON_1,WPAD_BUTTON_2,WPAD_BUTTON_PLUS,WPAD_BUTTON_MINUS};
    const unsigned key[]={KEY_A,KEY_B,KEY_ONE,KEY_TWO,KEY_PLUS,KEY_MINUS};
    for(int i=0;i<6;i++) if(buttons&raw[i]) keys|=key[i];
    const u32 arrows[]={WPAD_BUTTON_UP,WPAD_BUTTON_RIGHT,WPAD_BUTTON_DOWN,WPAD_BUTTON_LEFT};
    const unsigned upright[]={KEY_UP,KEY_RIGHT,KEY_DOWN,KEY_LEFT},sideways[]={KEY_LEFT,KEY_UP,KEY_RIGHT,KEY_DOWN};
    for(int i=0;i<4;i++) if(buttons&arrows[i]) keys|=vertical?upright[i]:sideways[i];
    return keys;
}
int main(void) {
    VIDEO_Init(); WPAD_Init();
    SYS_SetPowerCallback(power); SYS_SetResetCallback(reset_button);
    mode=VIDEO_GetPreferredMode(NULL);
    aspect=CONF_GetAspectRatio()==CONF_ASPECT_16_9?16.0f/9:4.0f/3;
    void *fb[2]={SYS_AllocateFramebuffer(mode),SYS_AllocateFramebuffer(mode)};
    void *fifo=memalign(32,256*1024);
    u32 history_size=GX_GetTexBufferSize(mode->fbWidth,mode->efbHeight,GX_TF_RGB565,GX_FALSE,0);
    history_pixels=memalign(32,history_size); trail_pixels=memalign(32,history_size); freeze_pixels=memalign(32,history_size);
    if(!fb[0]||!fb[1]||!fifo||!history_pixels||!trail_pixels||!freeze_pixels) return 1;
    fb[0]=MEM_K0_TO_K1(fb[0]); fb[1]=MEM_K0_TO_K1(fb[1]);
    memset(fifo,0,256*1024); memset(history_pixels,0,history_size); DCFlushRange(history_pixels,history_size);
    memset(trail_pixels,0,history_size); DCFlushRange(trail_pixels,history_size);
    memset(freeze_pixels,0,history_size); DCFlushRange(freeze_pixels,history_size);
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
    mesh_init();
    dome_init(); indirect_init();
    master_init();
    GX_InitTexObj(&chrome_tex,chrome_pixels,128,128,GX_TF_RGB565,GX_CLAMP,GX_CLAMP,GX_FALSE);
    GX_InitTexObjLOD(&chrome_tex,GX_LINEAR,GX_LINEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    for(int role=0;role<3;role++) {
        GX_InitTexObj(&grid_tex[role],grid_pixels[role],64,64,GX_TF_RGB565,GX_REPEAT,GX_REPEAT,GX_TRUE);
        GX_InitTexObjLOD(&grid_tex[role],GX_LIN_MIP_LIN,GX_LINEAR,0,6,0,GX_TRUE,GX_TRUE,GX_ANISO_4);
    }
    GX_InitTexObj(&history,history_pixels,mode->fbWidth,mode->efbHeight,GX_TF_RGB565,GX_CLAMP,GX_CLAMP,GX_FALSE);
    GX_InitTexObjLOD(&history,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    GX_InitTexObj(&freeze_tex,freeze_pixels,mode->fbWidth,mode->efbHeight,GX_TF_RGB565,GX_CLAMP,GX_CLAMP,GX_FALSE);
    GX_InitTexObjLOD(&freeze_tex,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    GX_InitTexObj(&trail_tex,trail_pixels,mode->fbWidth,mode->efbHeight,GX_TF_RGB565,GX_CLAMP,GX_CLAMP,GX_FALSE);
    GX_InitTexObjLOD(&trail_tex,GX_LINEAR,GX_LINEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    GX_SetTexCopySrc(0,0,mode->fbWidth,mode->efbHeight);
    GX_SetTexCopyDst(mode->fbWidth,mode->efbHeight,GX_TF_RGB565,GX_FALSE);
    const u8 bayer[8][8]={{0,32,8,40,2,34,10,42},{48,16,56,24,50,18,58,26},{12,44,4,36,14,46,6,38},{60,28,52,20,62,30,54,22},
        {3,35,11,43,1,33,9,41},{51,19,59,27,49,17,57,25},{15,47,7,39,13,45,5,37},{63,31,55,23,61,29,53,21}};
    for(int y=0;y<8;y++) for(int x=0;x<8;x++) dither_pixels[y*8+x]=bayer[y][x]<36?255:0;
    for(int y=0;y<8;y++) for(int x=0;x<64;x++)
        halftone_pixels[((y/4)*8+x/8)*32+(y%4)*8+x%8]=bayer[y][x%8]<x?255:0;
    DCFlushRange(halftone_pixels,sizeof(halftone_pixels));
    GX_InitTexObj(&halftone_tex,halftone_pixels,64,8,GX_TF_I8,GX_REPEAT,GX_REPEAT,GX_FALSE);
    GX_InitTexObjLOD(&halftone_tex,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    DCFlushRange(dither_pixels,sizeof(dither_pixels));
    GX_InitTexObj(&dither_tex,dither_pixels,8,8,GX_TF_I8,GX_REPEAT,GX_REPEAT,GX_FALSE);
    GX_InitTexObjLOD(&dither_tex,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC);
    reset(&synth); make_metal();
    midi_start();
    GX_CopyDisp(fb[0],GX_TRUE); GX_DrawDone();
    VIDEO_SetBlack(FALSE); VIDEO_Flush();
    int frame=1; u64 last=gettime();
#ifdef GEEZY_VISUAL_CHECK
    unsigned check_frame=0;
#endif
    while(!quitting&&SYS_MainLoop()) {
        u64 now=gettime(); float dt=ticks_to_microsecs(now-last)/1000000.0f;
        profile_begin(now,ticks_to_microsecs(now-last)); last=now;
        WPAD_ScanPads(); u32 down=WPAD_ButtonsDown(0),held=WPAD_ButtonsHeld(0);
#ifdef GEEZY_VISUAL_CHECK
#if defined(GEEZY_WORLDS_CHECK)
        unsigned stage=(check_frame/180)%20,beat=check_frame%180;
        down=held=0;
        if(beat==0) {
            reset(&synth); memset(&midi_input,0,sizeof(midi_input));
            midi_action(&synth,&midi_input,56+stage%5,1); midi_action(&synth,&midi_input,56+stage%5,0);
            theme_preset(&synth,stage/5); synth.gear=2;
            synth.layout=stage/5; synth.shape=stage/5; synth.backdrop=stage/5;
            synth.motion_mode=stage/5; make_metal();
        }
        if(beat>=30 && beat<55) held=WPAD_BUTTON_DOWN;
        if(beat>=60 && beat<125) held=WPAD_BUTTON_A;
        if(beat>=145 && beat<170) { synth.mask_pattern=2+stage%2; held=WPAD_BUTTON_B; }
#elif defined(GEEZY_GRAPHICS_CHECK)
        unsigned stage=(check_frame/120)%32,beat=check_frame%120;
        down=held=0;
        if(beat==0) {
            reset(&synth); memset(&midi_input,0,sizeof(midi_input));
            theme_preset(&synth,stage/8); harmony_bank(&synth,stage%16);
            object_preset(&synth,stage%8); make_metal();
        }
        midi_action(&synth,&midi_input,8+stage%8,beat>=20 && beat<100);
#elif defined(GEEZY_SCULPTURE_CHECK)
        unsigned stage=(check_frame/120)%56,beat=check_frame%120;
        down=held=0;
        if(beat==0) {
            reset(&synth); check_sculpture=stage%14;
            synth.skin=stage/14; synth.layout=stage%4; synth.backdrop=stage%4;
            synth.gear=stage%4; synth.seed=7+stage*7919; make_metal();
            if(stage%3==0) hit(&synth,1.5f);
        }
        if(stage%4==0 && beat>=30 && beat<110) held=WPAD_BUTTON_A;
        if(stage%4==1 && beat>=60 && beat<90) held=WPAD_BUTTON_B;
        if(beat==90) held|=WPAD_BUTTON_1|WPAD_BUTTON_PLUS;
#elif defined(GEEZY_LATTICE_CHECK) || defined(GEEZY_CHECKER_CHECK) || defined(GEEZY_TRAIL_CHECK) || defined(GEEZY_STENCIL_CHECK)
        down=held=0;
        if(check_frame==0) {
            reset(&synth); synth.gear=2; steer(&synth,1,1);
#ifdef GEEZY_BEAM_STYLE
            synth.beam_style=GEEZY_BEAM_STYLE;
#endif
        }
#else
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
        u32 direction=(u32[]){WPAD_BUTTON_RIGHT,WPAD_BUTTON_DOWN,WPAD_BUTTON_LEFT,WPAD_BUTTON_UP}[stage%4];
        if(beat==1) held=WPAD_BUTTON_1|direction;
        if(beat==3) held=WPAD_BUTTON_2|direction;
        if(beat==5) held=WPAD_BUTTON_1|WPAD_BUTTON_2|direction;
        if(beat==6) held=WPAD_BUTTON_1|WPAD_BUTTON_2|(u32[]){
            WPAD_BUTTON_RIGHT|WPAD_BUTTON_UP,WPAD_BUTTON_RIGHT|WPAD_BUTTON_DOWN,
            WPAD_BUTTON_LEFT|WPAD_BUTTON_DOWN,WPAD_BUTTON_LEFT|WPAD_BUTTON_UP}[stage%4];
        for(unsigned i=0;i<stage%4;i++) if(beat==7+i*2) held=WPAD_BUTTON_A|WPAD_BUTTON_1;
        if(beat==15) held=WPAD_BUTTON_B|WPAD_BUTTON_2;
        for(unsigned i=0;i<stage;i++) if(beat==18+i*2) held=WPAD_BUTTON_B|WPAD_BUTTON_1;
        if(beat==30) down=WPAD_BUTTON_DOWN;
        if(beat>=30 && beat<90) held|=WPAD_BUTTON_DOWN;
        if(beat>=60 && beat<90) held|=WPAD_BUTTON_B;
        if(beat>=90 && beat<210) held|=WPAD_BUTTON_A;
        if(beat>=240 && beat<280) held|=WPAD_BUTTON_B;
#endif
        check_frame++;
#endif
        if(down&(WPAD_BUTTON_HOME|WPAD_CLASSIC_BUTTON_HOME)) break;
        unsigned classic=0; float stick_x=0,stick_y=0;
#ifndef GEEZY_VISUAL_CHECK
        struct expansion_t expansion; memset(&expansion,0,sizeof(expansion)); WPAD_Expansion(0,&expansion);
        if(expansion.type==WPAD_EXP_CLASSIC && WPAD_Data(0)->err==WPAD_ERR_NONE) {
            const u32 buttons[]={WPAD_CLASSIC_BUTTON_A,WPAD_CLASSIC_BUTTON_B,WPAD_CLASSIC_BUTTON_X,WPAD_CLASSIC_BUTTON_Y,
                WPAD_CLASSIC_BUTTON_FULL_L,WPAD_CLASSIC_BUTTON_FULL_R,WPAD_CLASSIC_BUTTON_ZL,WPAD_CLASSIC_BUTTON_ZR,
                WPAD_CLASSIC_BUTTON_UP,WPAD_CLASSIC_BUTTON_RIGHT,WPAD_CLASSIC_BUTTON_DOWN,WPAD_CLASSIC_BUTTON_LEFT,
                WPAD_CLASSIC_BUTTON_PLUS,WPAD_CLASSIC_BUTTON_MINUS};
            for(int i=0;i<14;i++) if(held&buttons[i]) classic|=1u<<i;
            float angle=expansion.classic.ljs.ang*0.0174532925f,mag=clamp01(expansion.classic.ljs.mag);
            stick_x=mag*sinf(angle); stick_y=mag*cosf(angle);
        }
#endif
        unsigned held_right=0,down_right=0; int dual_connected=0;
#ifndef GEEZY_VISUAL_CHECK
        WPADData *right=WPAD_Data(1);
        dual_connected=right->err==WPAD_ERR_NONE;
        if(dual_connected) { held_right=WPAD_ButtonsHeld(1); down_right=WPAD_ButtonsDown(1); }
#endif
        if(down_right&WPAD_BUTTON_HOME) break;
        unsigned keys=remote_keys(held,dual_connected),right_keys=remote_keys(held_right,1);
        int dirty=dual_controls(&synth,&dual_input,right_keys,dual_connected);
        dirty|=controls(&synth,keys);
        dirty|=classic_controls(&synth,classic,stick_x,stick_y);
        dirty|=midi_poll(&synth,&midi_input);
        set_master_fx(&synth,remote_master_fx(keys)|classic_master_fx(classic)|midi_master_fx(&midi_input)|right_master_fx(right_keys));
        int midi_mask=midi_overlay(&midi_input);
        if(midi_mask>=0) synth.mask_pattern=midi_mask;
        int midi_boost=midi_steering(&synth,&midi_input);
        int classic_boost=stick_x*stick_x+stick_y*stick_y>0.04f;
        if(dirty) make_metal();
        int recovery=(keys&(KEY_PLUS|KEY_MINUS))==(KEY_PLUS|KEY_MINUS) || (classic&(CLASS_PLUS|CLASS_MINUS))==(CLASS_PLUS|CLASS_MINUS);
        if(!recovery) {
#ifndef GEEZY_VISUAL_CHECK
            Vec forces[2]={{0,0,0},{0,0,0}}; int valid[2]={0,0};
            int held_b[2]={!!(held&WPAD_BUTTON_B),!!(held_right&WPAD_BUTTON_B)};
            for(int channel=0;channel<(dual_connected?2:1);channel++) {
                WPADData *remote=WPAD_Data(channel);
                if(remote->err==WPAD_ERR_NONE && (remote->data_present&WPAD_DATA_ACCEL)) {
                    struct gforce_t gf; WPAD_GForce(channel,&gf);
                    forces[channel]=(Vec){gf.x,gf.y,gf.z}; valid[channel]=1;
                }
            }
            if(remote_gestures(&synth,&dual_input,forces,valid,held_b,dt)) make_metal();
#endif
            unsigned performance_keys=keys&~synth.remote_fx_latch,performance_classic=classic&~synth.classic_fx_latch;
            advance(&synth,dt,!!(performance_keys&KEY_A)||!!(performance_classic&CLASS_A)||midi_input.held[4],
                steering_buttons(performance_keys)||classic_boost||midi_boost,!!(performance_keys&KEY_B)||!!(performance_classic&CLASS_B)||midi_mask>=0);
        }
        profile_mark(TIME_INPUT);
        GX_SetColorUpdate(GX_TRUE); GX_InvalidateTexAll(); render();
        profile_mark(TIME_UI);
        GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE); GX_SetColorUpdate(GX_TRUE);
        GX_CopyDisp(fb[frame],GX_TRUE); GX_DrawDone(); profile_end();
        VIDEO_SetNextFramebuffer(fb[frame]); VIDEO_Flush(); VIDEO_WaitVSync(); frame^=1;
    }
    midi_usb.stop=1;
    return 0;
}
