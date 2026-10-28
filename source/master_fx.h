#ifndef GEEZY_MASTER_FX_H
#define GEEZY_MASTER_FX_H
// Static screen geometry: even the 40x30 mosaic is one display-list call per frame.
static u8 master_plain[3][480] ATTRIBUTE_ALIGN(32);
static u8 master_mosaic[3][115296] ATTRIBUTE_ALIGN(32);
static u32 master_bytes[2][3];
static void master_geometry(int kind,int mosaic) {
    int columns=mosaic?40:kind?2:1,rows=mosaic?30:kind==2?2:1;
    unsigned fx=kind==2?FX_KALEIDO_4:kind==1?FX_MIRROR_X:0;
    GX_Begin(GX_QUADS,GX_VTXFMT0,columns*rows*4);
    for(int y=0;y<rows;y++) for(int x=0;x<columns;x++) for(int j=0;j<4;j++) {
        float sx=(x+(j==1||j==2))/(float)columns,sy=(y+(j>=2))/(float)rows;
        float u=mosaic?(x+0.5f)/columns:sx,v=mosaic?(y+0.5f)/rows:sy;
        master_uv(fx,&u,&v);
        if(mosaic) {
            // Sample a texel center, never the boundary between two nearest texels.
            u=(floorf(u*mode->fbWidth+0.001f)+0.5f)/mode->fbWidth;
            v=(floorf(v*mode->efbHeight+0.001f)+0.5f)/mode->efbHeight;
        }
        vertex((Vec){sx*2-1,1-sy*2,-1},u,v,255,255);
    }
    GX_End();
}
static void master_init(void) {
    material(NULL);
    for(int mosaic=0;mosaic<2;mosaic++) for(int kind=0;kind<3;kind++) {
        void *data=mosaic?(void*)master_mosaic[kind]:(void*)master_plain[kind];
        unsigned size=mosaic?sizeof(master_mosaic[0]):sizeof(master_plain[0]);
        DCInvalidateRange(data,size); GX_BeginDispList(data,size);
        master_geometry(kind,mosaic); master_bytes[mosaic][kind]=GX_EndDispList();
    }
}
static void master_texture(GXTexObj *texture,unsigned fx) {
    ortho(); material(texture); GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    int kind=(fx&FX_KALEIDO_4)?2:(fx&FX_MIRROR_X)?1:0,mosaic=!!(fx&FX_MOSAIC);
    if(master_bytes[mosaic][kind]) GX_CallDispList(mosaic?(void*)master_mosaic[kind]:(void*)master_plain[kind],master_bytes[mosaic][kind]);
    else master_geometry(kind,mosaic);
}
static void master_hold(int hold) {
    trail_valid=0;
    if(hold==HOLD_FREEZE) { master_texture(&freeze_tex,0); return; }
    ortho(); material(NULL); GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    int white=hold==HOLD_WHITE || (hold==HOLD_STROBE && (mesh_frame&1));
    tint=white?(GXColor){255,255,255,255}:(GXColor){0,0,0,255}; mask_rect(-1,-1,2,2);
}
static void master_finish(RenderPlan plan) {
    if(plan.post_history) { copy_texture(history_pixels); master_texture(&history,synth.master_fx); }
    if(synth.master_fx&FX_INVERT) {
        ortho(); material(NULL); GX_SetBlendMode(GX_BM_LOGIC,GX_BL_ONE,GX_BL_ZERO,GX_LO_INV);
        mask_rect(-1,-1,2,2); GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    }
    if(plan.capture_freeze) {
        copy_texture(freeze_pixels); freeze_valid=1; trail_valid=0;
        master_texture(&freeze_tex,0);
    }
}
#endif
