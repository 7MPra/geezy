#ifndef GEEZY_TEV_H
#define GEEZY_TEV_H
static GXTexObj ind_tex;
static u16 ind_pixels[64*64] ATTRIBUTE_ALIGN(32);
static int detail_kind;
static void indirect_init(void) {
    for(int y=0;y<64;y++) for(int x=0;x<64;x++) {
        float u=x*6.2831853f/64,v=y*6.2831853f/64;
        float a=0.5f+0.24f*sinf(u+2*sinf(v))+0.24f*cosf(v*2-u);
        float b=0.5f+0.24f*cosf(v+2*cosf(u))+0.24f*sinf(u*2+v);
        ind_pixels[((y/4)*16+x/4)*16+(y%4)*4+x%4]=rgb565(a,b,a);
    }
    DCFlushRange(ind_pixels,sizeof(ind_pixels));
    GX_InitTexObj(&ind_tex,ind_pixels,64,64,GX_TF_RGB565,GX_REPEAT,GX_REPEAT,GX_FALSE);
    GX_InitTexObjLOD(&ind_tex,GX_LINEAR,GX_LINEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
}
// Compose every detail coordinate with the same base UV, including normal-generated lists.
static void detail_uv(Mtx base,u32 source) {
    Mtx m; float clock=synth.draw.uv_shift;
    for(int i=0;i<2;i++) {
        guMtxCopy(base,m); float scale=i==0?-2.3f:1.7f;
        for(int r=0;r<2;r++) for(int c=0;c<4;c++) m[r][c]*=scale;
        m[0][3]+=clock*(i?0.7f:-3); m[1][3]+=clock*(i?-0.9f:5);
        GX_LoadTexMtxImm(m,i?GX_TEXMTX2:GX_TEXMTX1,GX_MTX2x4);
        GX_SetTexCoordGen(i?GX_TEXCOORD2:GX_TEXCOORD1,GX_TG_MTX2x4,source,i?GX_TEXMTX2:GX_TEXMTX1);
    }
}
static void indirect_stage(void) {
    GX_LoadTexObj(&ind_tex,GX_TEXMAP2); GX_SetNumIndStages(1);
    GX_SetIndTexOrder(GX_INDTEXSTAGE0,GX_TEXCOORD2,GX_TEXMAP2);
    GX_SetIndTexCoordScale(GX_INDTEXSTAGE0,GX_ITS_1,GX_ITS_1);
    float strength=0.035f+0.10f*synth.overdrive+0.13f*synth.tension+fminf(0.18f,synth.draw.burst*0.04f);
    f32 m[2][3]={{0,strength,0},{0,0,strength}};
    GX_SetIndTexMatrix(GX_ITM_0,m,0);
    GX_SetTevIndirect(GX_TEVSTAGE0,GX_INDTEXSTAGE0,GX_ITF_8,GX_ITB_STU,GX_ITM_0,
        GX_ITW_OFF,GX_ITW_OFF,GX_FALSE,GX_FALSE,GX_ITBA_OFF);
}
static void detail_material(int kind) {
    detail_kind=kind==CHROME?1:2;
    GX_SetNumTexGens(3); Mtx base; guMtxIdentity(base); detail_uv(base,GX_TG_TEX0);
    GX_SetTevKColor(GX_KCOLOR0,gx_color(palette.primary));
    GX_SetTevKColor(GX_KCOLOR1,gx_color(palette.secondary));
    GX_SetTevKColor(GX_KCOLOR2,gx_color(palette.accent));
    GX_SetNumTevStages(kind==CHROME?4:3);
    for(int i=1;i<4;i++) {
        GX_SetTevOrder(i,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLOR0A0);
        GX_SetTevAlphaIn(i,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_APREV);
        GX_SetTevAlphaOp(i,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
    }
    if(kind==CHROME) {
        indirect_stage();
        GX_SetTevColorIn(GX_TEVSTAGE1,GX_CC_ZERO,GX_CC_ZERO,GX_CC_ZERO,GX_CC_CPREV);
        GX_SetTevColorOp(GX_TEVSTAGE1,GX_TEV_ADD,GX_TB_SUBHALF,GX_CS_SCALE_4,GX_TRUE,GX_TEVREG1);
        GX_SetTevOrder(GX_TEVSTAGE2,GX_TEXCOORD1,GX_TEXMAP2,GX_COLORNULL);
        GX_SetTevKColorSel(GX_TEVSTAGE2,GX_TEV_KCSEL_K0);
        GXColor secondary=gx_color(palette.secondary),accent=gx_color(palette.accent);
        GX_SetTevColor(GX_TEVREG2,(GXColor){(secondary.r+accent.r)/2,(secondary.g+accent.g)/2,(secondary.b+accent.b)/2,255});
        GX_SetTevColorIn(GX_TEVSTAGE2,GX_CC_KONST,GX_CC_C2,GX_CC_TEXC,GX_CC_ZERO);
        GX_SetTevColorOp(GX_TEVSTAGE2,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_2,GX_TRUE,GX_TEVREG2);
        // Multiplication by the original reflection preserves exact black; highlights clip white.
        GX_SetTevColorIn(GX_TEVSTAGE3,GX_CC_ZERO,GX_CC_C2,GX_CC_CPREV,GX_CC_C1);
        GX_SetTevColorOp(GX_TEVSTAGE3,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
    } else {
        GX_SetTevOrder(GX_TEVSTAGE1,GX_TEXCOORD1,GX_TEXMAP0,GX_COLORNULL);
        GX_SetTevColorIn(GX_TEVSTAGE1,GX_CC_ZERO,GX_CC_TEXC,GX_CC_CPREV,GX_CC_ZERO);
        GX_SetTevColorOp(GX_TEVSTAGE1,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_4,GX_TRUE,GX_TEVREG1);
        GX_SetTevKColorSel(GX_TEVSTAGE2,GX_TEV_KCSEL_K2);
        GX_SetTevColorIn(GX_TEVSTAGE2,GX_CC_ZERO,GX_CC_KONST,GX_CC_C1,GX_CC_CPREV);
        GX_SetTevColorOp(GX_TEVSTAGE2,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
    }
}
#endif
