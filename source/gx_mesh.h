#ifndef GEEZY_GX_MESH_H
#define GEEZY_GX_MESH_H
// Lists contain primitives only: material state stays under libogc's state tracking.
enum { MESH_LISTS=32, MESH_LIST_BYTES=65536 };
typedef struct { int type,a,b,wire; float x,y,z; } MeshKey;
typedef struct { MeshKey key; u32 bytes,frame; } MeshList;
static u8 mesh_lists[MESH_LISTS][MESH_LIST_BYTES] ATTRIBUTE_ALIGN(32);
static MeshList mesh_slots[MESH_LISTS];
static u8 cube_lists[2][2048] ATTRIBUTE_ALIGN(32);
static u32 cube_bytes[2],mesh_frame;
static int gpu_mesh_enabled=1;
static int gpu_stage2_enabled=1;
static int gpu_stage3_enabled=1;
#ifdef GEEZY_GPU_CHECK
static unsigned mesh_calls,mesh_builds;
#endif
static void list_format(void) {
    GX_ClearVtxDesc(); GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_NRM,GX_DIRECT); GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);
}
static void immediate_format(void) {
    GX_ClearVtxDesc(); GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT); GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);
}
static void list_vertex(Vec p,Vec n,float u,float v) {
    GX_Position3f32(p.x,p.y,p.z); GX_Normal3f32(n.x,n.y,n.z); GX_TexCoord2f32(u,v);
}
static void mesh_init(void) {
    GX_SetVtxAttrFmt(GX_VTXFMT1,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT1,GX_VA_NRM,GX_NRM_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT1,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
    Vec p[8]; for(int i=0;i<8;i++) p[i]=(Vec){i&1?1:-1,i&2?1:-1,i&4?1:-1};
    const int faces[24]={0,2,3,1,4,5,7,6,0,1,5,4,2,6,7,3,0,4,6,2,1,3,7,5};
    list_format();
    for(int wire=0;wire<2;wire++) {
        DCInvalidateRange(cube_lists[wire],sizeof(cube_lists[wire]));
        GX_BeginDispList(cube_lists[wire],sizeof(cube_lists[wire]));
        GX_Begin(wire?GX_LINES:GX_QUADS,GX_VTXFMT1,24);
        if(wire) {
            for(int i=0;i<8;i++) for(int bit=1;bit<=4;bit*=2) if(!(i&bit)) {
                list_vertex(p[i],(Vec){0,0,1},0,0); list_vertex(p[i|bit],(Vec){0,0,1},0,0);
            }
        } else for(int f=0;f<6;f++) {
            Vec a=p[faces[f*4]],b=p[faces[f*4+1]],c=p[faces[f*4+2]];
            Vec n=unit(cross(add(b,scale_vec(a,-1)),add(c,scale_vec(a,-1))));
            for(int j=0;j<4;j++) list_vertex(p[faces[f*4+j]],n,j==1||j==2,j>=2);
        }
        GX_End();
        if(wire) {
            GX_Begin(GX_POINTS,GX_VTXFMT1,8);
            for(int i=0;i<8;i++) list_vertex(p[i],(Vec){0,0,1},0,0);
            GX_End();
        }
        cube_bytes[wire]=GX_EndDispList();
    }
    immediate_format();
}
static int mesh_fast(void) { return gpu_mesh_enabled && !quantize_vertices; }
static void list_draw(void *data,u32 bytes,Mtx model,Mtx rotation,int box_uv,u8 shade) {
    // Scalar concat avoids a NaN lane observed in the paired-single path with this build.
    Mtx mv,norm,uv,detail; c_guMtxConcat(view,model,mv); c_guMtxConcat(view,rotation,norm);
    GX_LoadPosMtxImm(mv,GX_PNMTX0); GX_LoadNrmMtxImm(norm,GX_PNMTX0);
    guMtxIdentity(uv);
    int explicit_uv=(box_uv && surface_kind==GRID) || (box_uv==1 && surface_kind==GHOST);
    if(explicit_uv) uv[1][3]=synth.draw.uv_shift*6;
    else {
        for(int j=0;j<3;j++) { uv[0][j]=norm[0][j]*0.48f; uv[1][j]=-norm[1][j]*0.48f; }
        uv[0][3]=0.5f; uv[1][3]=0.5f+(surface_kind==GRID?synth.draw.uv_shift*6:0);
    }
    GX_LoadTexMtxImm(uv,GX_TEXMTX3,GX_MTX2x4);
    GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,explicit_uv?GX_TG_TEX0:GX_TG_NRM,GX_TEXMTX3);
    if(detail_kind) detail_uv(uv,explicit_uv?GX_TG_TEX0:GX_TG_NRM);
    if(surface_kind==GHOST) {
        guMtxCopy(uv,detail);
        for(int i=0;i<2;i++) for(int j=0;j<4;j++) detail[i][j]*=16;
        GX_LoadTexMtxImm(detail,GX_TEXMTX2,GX_MTX2x4);
        GX_SetTexCoordGen(GX_TEXCOORD1,GX_TG_MTX2x4,explicit_uv?GX_TG_TEX0:GX_TG_NRM,GX_TEXMTX2);
    }
    GX_SetChanMatColor(GX_COLOR0A0,(GXColor){shade*tint.r/255,shade*tint.g/255,shade*tint.b/255,255});
    GX_SetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_REG,0,GX_DF_NONE,GX_AF_NONE);
    list_format(); GX_CallDispList(data,bytes);
#ifdef GEEZY_GPU_CHECK
    mesh_calls++;
#endif
    immediate_format();
    GX_SetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,0,GX_DF_NONE,GX_AF_NONE);
    GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    if(surface_kind==GHOST) GX_SetTexCoordGen(GX_TEXCOORD1,GX_TG_MTX2x4,GX_TG_TEX0,GX_TEXMTX0);
    if(detail_kind) { Mtx base; guMtxIdentity(base); detail_uv(base,GX_TG_TEX0); }
    GX_LoadPosMtxImm(view,GX_PNMTX0);
}
static void rigid_matrix(Vec center,Vec size,float a,float b,Mtx model,Mtx rotation) {
    float ca=cosf(a),sa=sinf(a),cb=cosf(b),sb=sinf(b);
    rotation[0][0]=ca; rotation[0][1]=0; rotation[0][2]=sa; rotation[0][3]=0;
    rotation[1][0]=sa*sb; rotation[1][1]=cb; rotation[1][2]=-ca*sb; rotation[1][3]=0;
    rotation[2][0]=-sa*cb; rotation[2][1]=sb; rotation[2][2]=ca*cb; rotation[2][3]=0;
    guMtxCopy(rotation,model);
    for(int row=0;row<3;row++) { model[row][0]*=size.x; model[row][1]*=size.y; model[row][2]*=size.z; }
    model[0][3]=center.x; model[1][3]=center.y; model[2][3]=center.z;
}
static MeshList *mesh_lookup(MeshKey key) {
    MeshList *victim=NULL;
    for(int i=0;i<MESH_LISTS;i++) {
        MeshList *slot=&mesh_slots[i];
        if(slot->bytes && memcmp(&slot->key,&key,sizeof(key))==0) { slot->frame=mesh_frame; return slot; }
        // main's end-of-frame GX_DrawDone retires these buffers before they can be reused.
        if(slot->frame!=mesh_frame && (!victim || slot->frame<victim->frame)) victim=slot;
    }
    if(victim) { victim->key=key; victim->bytes=0; victim->frame=mesh_frame; }
    return victim;
}
static void cached_mesh_draw(MeshList *slot,Vec center,float size,float a,float b) {
    Mtx model,rotation; rigid_matrix(center,(Vec){size,size,size},a,b,model,rotation);
    list_draw(mesh_lists[slot-mesh_slots],slot->bytes,model,rotation,slot->key.type>=4?2:0,255);
}
#endif
