#ifndef GEEZY_WORLDS_H
#define GEEZY_WORLDS_H
#include "world_math.h"

static void world_projection(void) {
    quantize_vertices=synth.slow;
    Mtx44 p; Mtx look,bank;
    Vec offset=hybrid_eye(&synth);
    guVector eye={offset.x,offset.y,offset.z},up={0,1,0},at;
    float roll=flight_bank(&synth);
    if(synth.world_mode==4) {
        // Orthographic throughout; dolly distance controls poster scale without perspective.
        float extent=11*(0.75f+0.25f*synth.draw.radius/16);
        guOrtho(p,extent,-extent,-extent*aspect,extent*aspect,0.2f,360);
        GX_LoadProjectionMtx(p,GX_ORTHOGRAPHIC);
        eye.z+=32; at=(guVector){synth.draw.aim_x*3,synth.draw.aim_y*3,0};
        roll=synth.draw.roll;
    } else {
        guPerspective(p,synth.draw.fov,aspect,0.2f,synth.world_mode==2?280:160);
        GX_LoadProjectionMtx(p,GX_PERSPECTIVE);
        Vec target=flight_target(&synth); at=(guVector){eye.x+target.x+0.25f*sinf(synth.draw.azimuth),eye.y+target.y+0.2f*sinf(synth.draw.elevation),eye.z+target.z};
        if(synth.world_mode==2) {
            AtriumCamera camera=atrium_camera(&synth);
            eye=(guVector){camera.eye.x+offset.x,camera.eye.y+offset.y,camera.eye.z+offset.z};
            at=(guVector){camera.target.x+offset.x+0.25f*sinf(synth.draw.azimuth),camera.target.y+offset.y+0.2f*sinf(synth.draw.elevation),camera.target.z+offset.z}; roll=camera.roll;
        }
    }
    scene_eye=(Vec){eye.x,eye.y,eye.z};
    guLookAt(look,&eye,&up,&at);
    float a=roll*0.0174532925f,c=cosf(a),sn=sinf(a);
    guMtxIdentity(bank); bank[0][0]=bank[1][1]=c; bank[0][1]=-sn; bank[1][0]=sn;
    c_guMtxConcat(bank,look,view); GX_LoadPosMtxImm(view,GX_PNMTX0);
    GX_SetFog(synth.world_mode==4?GX_FOG_ORTHO_LIN:GX_FOG_PERSP_LIN,
        synth.world_mode==4?65:32,synth.world_mode==4?140:47,0.2f,synth.world_mode==4?360:160,clear_color());
    if(synth.world_mode==2) {
        GXColor fog=synth.theme==1?clear_color():gx_color(palette.secondary);
        if(synth.theme==0 || synth.theme==2) { fog.r/=3; fog.g/=3; fog.b/=3; }
        if(synth.slow) fog=strobe_lit(&synth)?(GXColor){255,255,255,255}:(GXColor){0,0,0,255};
        GX_SetFog(GX_FOG_PERSP_LIN,28,145,0.2f,280,fog);
    }
    GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
}
static void world_surface(int kind,int role) {
    background_role=role%3; surface(kind);
}
static void canyon_prism(Vec c,float height,int ceiling,u8 shade) {
    float sign=ceiling?-1:1;
    Vec p[6]={{c.x-3,c.y,c.z-3},{c.x+3,c.y,c.z-3},{c.x,c.y+height*sign,c.z-3},
              {c.x-3,c.y,c.z+3},{c.x+3,c.y,c.z+3},{c.x,c.y+height*sign,c.z+3}};
    const int faces[24]={0,1,2,3,5,4,0,3,4,0,4,1,1,4,5,1,5,2,2,5,3,2,3,0};
    for(int i=0;i<24;i+=3) triangle(p[faces[i]],p[faces[i+1]],p[faces[i+2]],shade);
}
static void horizon_world(void) {
    Vec cam=synth.draw.world_cam;
    int layers=(synth.backdrop&1)||(synth.layout&1)?2:1;
    for(int layer=0;layer<layers;layer++) for(int z=0;z<12;z++) for(int x=0;x<12;x++) {
        uint32_t h=terrain_hash(x,z,synth.seed+layer*71);
        float px=world_cell(x,cam.x,8),pz=world_cell(z,cam.z,8);
        float height=2+(h%7)*2.5f,sign=layer?-1:1;
        float base=sign*(-23),top=base+sign*height;
        // Includes the foundation, pipe width and the maximum vertex-snap displacement.
        Vec bounds={px,base+sign*(height-1.7f)*0.5f,pz};
        if(gpu_stage3_enabled && box_behind_view(bounds,(Vec){4.34f,(height+4.3f)*0.5f+0.34f,4.34f},view[2])) continue;
        u8 shade=255;
        world_surface(GRID,(h>>8)%3);
        box((Vec){px,base-sign*1.5f,pz},(Vec){4,1.5f,4},0,shade);
        switch((h+synth.backdrop)%3) {
        case 0:
            world_surface(h%5==0?MATTE:CHROME,(h>>4)%3);
            box((Vec){px,(base+top)*0.5f,pz},(Vec){2.6f,height*0.5f,2.6f},0,shade); break;
        case 1:
            world_surface(CHROME,(h>>4)%3); canyon_prism((Vec){px,base,pz},height,layer,shade); break;
        default:
            world_surface(GRID,x%3);
            beam((Vec){px,top,pz-4},(Vec){px,top,pz+4},1.3f,shade);
            box((Vec){px,base+sign*height*0.5f,pz},(Vec){0.8f,height*0.5f,0.8f},0,shade); break;
        }
    }
    // A full horizontal rim remains a horizon even when the pilot looks sideways.
    world_surface(WIRE,2); GX_SetFog(GX_FOG_NONE,0,1,0,1,clear_color());
    GX_SetLineWidth(12,GX_TO_ZERO); begin_3d(GX_LINESTRIP,97);
    for(int i=0;i<=96;i++) {
        float a=i*6.2831853f/96; vertex((Vec){46*cosf(a),0,46*sinf(a)},0,0,255,255);
    }
    GX_End();
}
static void atrium_light(Vec center,uint32_t h) {
    Vec a=add(center,(Vec){-7,39,-5}),b=add(center,(Vec){8,-26,5});
    world_surface(SOLID,h%2?2:0);
    if(!synth.slow && h%3==0) tint=(GXColor){255,255,255,255};
    beam(a,b,0.10f,255);
    // Parallel rays imply a light volume without translucent gray accumulation.
    GX_SetLineWidth(6,GX_TO_ZERO); begin_3d(GX_LINES,32);
    for(int i=0;i<16;i++) {
        Vec offset={i*0.38f,0,i*0.12f};
        vertex(add(a,offset),0,0,255,255); vertex(add(b,offset),1,1,255,255);
    }
    GX_End();
}
static void atrium_monolith(Vec c,uint32_t h,int detail) {
    Vec half={7.5f+(h%6)*1.5f,30,5+(h>>4)%5};
    world_surface(h&1?MATTE:CHROME,h%3);
    box(c,half,0,255);
    world_surface(GRID,(h>>6)%3);
    box(add(c,(Vec){half.x+1,5,2}),(Vec){1,36,2.5f},0,255);
    if(detail) {
        world_surface(h&1?CHROME:MATTE,1);
        for(int rib=0;rib<19;rib++)
            box(add(c,(Vec){0,-26+rib*2.8f,-half.z-0.45f}),(Vec){half.x+0.7f,0.14f,0.45f},0,255);
    }
    atrium_light(add(c,(Vec){-half.x-3,0,0}),h);
}
static void atrium_bridges(Vec c,uint32_t h,int detail) {
    world_surface(MATTE,0); box(add(c,(Vec){12,4,8}),(Vec){4,35,5},0,255);
    for(int level=0;level<3;level++) {
        float y=-16+level*17;
        Vec a=add(c,(Vec){-30,y,level*5-9}),b=add(c,(Vec){25,y+3,12-level*9});
        world_surface(level%2?CHROME:GRID,(h+level)%3); beam(a,b,1.4f,255);
        if(detail) {
            world_surface(WIRE,2); begin_3d(GX_LINES,24);
            for(int k=0;k<6;k++) {
                Vec p=add(a,scale_vec(add(b,scale_vec(a,-1)),k/6.0f));
                Vec q=add(a,scale_vec(add(b,scale_vec(a,-1)),(k+1)/6.0f));
                vertex(p,0,0,255,255); vertex(add(q,(Vec){0,-4,0}),0,0,255,255);
                vertex(add(p,(Vec){0,-4,0}),0,0,255,255); vertex(q,0,0,255,255);
            }
            GX_End();
        }
    }
    world_surface(CHROME,1);
    for(int stair=0;stair<6;stair++)
        box(add(c,(Vec){-22+stair*5,-22+stair*2.5f,-12}),(Vec){3,0.8f,5},0,255);
    float travel=synth.draw.phase*0.15915494f*3+(h%8)*0.125f;
    travel=(travel-floorf(travel))*52-26;
    world_surface(SOLID,2); box(add(c,(Vec){6,travel,8}),(Vec){1.2f,2.5f,2},0,255);
}
static void atrium_crusher(Vec c,uint32_t h,int detail) {
    float drive=atrium_piston(synth.draw.phase+(synth.cut%4)*1.57079633f,h,synth.draw.burst);
    int horizontal=(h>>2)&1;
    Vec axis=horizontal?(Vec){1,0,0}:(Vec){0,1,0};
    world_surface(GRID,1);
    beam(add(c,scale_vec(axis,-32)),add(c,scale_vec(axis,32)),2.5f,255);
    world_surface(CHROME,0);
    Vec head=add(c,scale_vec(axis,18-drive));
    box(head,horizontal?(Vec){8,11,7}:(Vec){11,8,7},0,255);
    world_surface(MATTE,0);
    box(add(c,scale_vec(axis,-23)),horizontal?(Vec){5,16,9}:(Vec){16,5,9},0,255);
    if(detail) {
        world_surface(SOLID,2);
        for(int i=0;i<10;i++) {
            Vec rib=add(c,scale_vec(axis,drive+i*1.5f));
            box(rib,horizontal?(Vec){0.13f,3.1f,3.1f}:(Vec){3.1f,0.13f,3.1f},0,255);
        }
    }
}
static void brutalist_world(void) {
    Vec cam=synth.draw.world_cam;
    // box_vertices has CCW outer faces; GX calls CW front-facing, so cull FRONT to hide interiors.
    GX_SetCullMode(GX_CULL_FRONT);
    int cx=(int)floorf(cam.x/48),cy=(int)floorf(cam.y/48),cz=(int)floorf(cam.z/48);
    for(int z=cz-3;z<=cz+3;z++) for(int y=cy-3;y<=cy+3;y++) for(int x=cx-3;x<=cx+3;x++) {
        uint32_t h=atrium_hash(x,y,z,synth.seed);
        Vec c=atrium_cell(x,y,z,cam);
        c=add(c,(Vec){17+(h%7),12+(h>>4)%13,8+(h>>8)%13});
        // Cull only beyond the fog or entirely behind the eye, allowing large blocks to straddle cells.
        Vec v={view[0][0]*c.x+view[0][1]*c.y+view[0][2]*c.z+view[0][3],
            view[1][0]*c.x+view[1][1]*c.y+view[1][2]*c.z+view[1][3],
            view[2][0]*c.x+view[2][1]*c.y+view[2][2]*c.z+view[2][3]};
        if(v.z>65 || v.z<-205 || fabsf(v.x)>190 || fabsf(v.y)>190) continue;
        int detail=c.x*c.x+c.y*c.y+c.z*c.z<90*90;
        switch(atrium_kind(h,synth.layout,synth.backdrop)) {
        case 0: atrium_monolith(c,h,detail); break;
        case 1: atrium_bridges(c,h,detail); break;
        case 2: atrium_crusher(c,h,detail); break;
        case 3:
            world_surface(MATTE,0); box(add(c,(Vec){-9,18,4}),(Vec){9,5,19},0,255);
            world_surface(CHROME,1); box(add(c,(Vec){16,10,-12}),(Vec){3,33,6},0,255);
            atrium_light(c,h); atrium_light(add(c,(Vec){-12,9,16}),h+1);
            if(detail) {
                world_surface(WIRE,2);
                beam(add(c,(Vec){-16,-25,12}),add(c,(Vec){21,31,8}),0.35f,255);
            }
            break;
        }
    }
    GX_SetCullMode(GX_CULL_NONE);
}
static Vec cloud_vertices[CLOUD_POINTS];
static u16 cloud_edges[CLOUD_POINTS*6],cloud_row_end[16];
static u8 cloud_point_shade[CLOUD_POINTS],cloud_line_shade[CLOUD_POINTS];
static void cloud_update(void) {
    static Pose previous;
    static uint32_t seed;
    static int shape=-1,layout=-1;
    static float amount=-1;
    float next_amount=crystal_amount(&synth);
    if(gpu_mesh_enabled && seed==synth.seed && shape==synth.shape && layout==synth.layout && amount==next_amount &&
       memcmp(&previous,&synth.draw,sizeof(previous))==0) return;
    previous=synth.draw; seed=synth.seed; shape=synth.shape; layout=synth.layout; amount=next_amount;
    Vec cells[125]; int count=cloud_glyph_cells(cells,synth.shape);
    for(int i=0;i<CLOUD_POINTS;i++) {
        Vec p=cloud_vertices[i]=cloud_position(i,&synth,cells,count);
        float fade=cloud_fade(p); cloud_point_shade[i]=(u8)(fade*255); cloud_line_shade[i]=(u8)(fade*150);
    }
    int n=0;
    for(int row=0;row<16;row++) {
        for(int i=row*144;i<(row+1)*144;i++) for(int k=0;k<3;k++) {
            int j=i+(int[]){1,12,144}[k]; if(j>=CLOUD_POINTS) continue;
            Vec d=add(cloud_vertices[j],scale_vec(cloud_vertices[i],-1));
            float length=d.x*d.x+d.y*d.y+d.z*d.z;
            if(length>0.05f && length<125) { cloud_edges[n++]=i; cloud_edges[n++]=j; }
        }
        cloud_row_end[row]=n;
    }
}
static void cloud_ink(int i) {
    if(synth.slow) return;
    tint=(i%4==3)?(GXColor){255,255,255,255}:gx_color(palette_role(i%4));
}
static void quantum_world(void) {
    cloud_update();
    u8 visible[CLOUD_POINTS];
    // One-unit margin includes any snap rounding; retain lines that cross the view plane.
    for(int i=0;i<CLOUD_POINTS;i++) visible[i]=!gpu_stage3_enabled || view_depth(cloud_vertices[i],view[2])<=1;
    world_surface(WIRE,2);
    // Bounded local-neighbor search: no all-pairs work on the Wii CPU.
    GX_SetLineWidth(6,GX_TO_ZERO);
    for(int row=0;row<16;row++) {
        int start=row?cloud_row_end[row-1]:0,end=cloud_row_end[row];
        int count=0;
        for(int i=start;i<end;i+=2) if(visible[cloud_edges[i]] || visible[cloud_edges[i+1]]) count+=2;
        if(!count) continue;
        cloud_ink(row); begin_3d(GX_LINES,count);
        for(int i=start;i<end;i+=2) if(visible[cloud_edges[i]] || visible[cloud_edges[i+1]])
            for(int j=0;j<2;j++) { int v=cloud_edges[i+j]; vertex(cloud_vertices[v],0,0,cloud_line_shade[v],255); }
        GX_End();
    }
    for(int group=0;group<4;group++) {
        cloud_ink(group); GX_SetPointSize(12+group*6+(u8)(synth.tension*12),GX_TO_ZERO);
        int count=0; for(int i=group;i<CLOUD_POINTS;i+=4) count+=visible[i];
        if(!count) continue;
        begin_3d(GX_POINTS,count);
        for(int i=group;i<CLOUD_POINTS;i+=4) if(visible[i]) vertex(cloud_vertices[i],0,0,cloud_point_shade[i],255);
        GX_End();
    }
}
static void ortho_world(void) {
    float phase=synth.draw.phase;
    Vec cam=synth.draw.world_cam;
    for(int lane=0;lane<5;lane++) {
        float offset=wrap((cam.x+cam.z)/3+lane*7,32);
        world_surface(GRID,lane%3);
        beam((Vec){offset-16,-18,-12-lane*3},(Vec){offset+10,18,-12-lane*3},0.65f+lane*0.15f,255);
    }
    swarm();
    primary_surface=1;
    for(int zone=0;zone<4;zone++) {
        Sculpture g=sculpture(synth.seed,synth.shape,zone);
        g.family=(g.family+zone+synth.layout)%14;
        float shift=wrap(cam.z/4+zone*6,24);
        Vec center={(zone%2?1:-1)*(8+2*sinf(phase*0.4f)),(zone<2?6:-6)+shift*0.22f,-zone*5};
        sculpture_draw(center,zone==0?1.8f:1.3f,phase*(zone%2?-1:1)+zone*0.7f,g);
    }
    primary_surface=0;
    world_surface(SOLID,2);
    for(int i=0;i<6;i++) {
        float x=wrap(i*5-cam.x/3,32);
        box((Vec){x,wrap(i*3-cam.y/4,24),5},(Vec){0.12f,3,0.3f},phase*0.2f,255);
    }
}
static void world_monuments(void) {
    primary_surface=1;
    if(synth.world_mode==3) {
        sculpture_surface=synth.layout%2?2:1;
        Sculpture g=sculpture(synth.seed,synth.shape,0);
        sculpture_draw(cloud_focus(&synth),2.2f,synth.draw.phase,g);
        sculpture_surface=0;
    } else {
        // Four repeating cells: cache reuse and conservative depth rejection bound the added load.
        for(int i=0;i<4;i++) {
            Vec c={world_cell(i*3+1,synth.draw.world_cam.x,8),
                synth.world_mode==1?(i%2?23:-17):8+(i%2)*18,
                world_cell(i*3+2,synth.draw.world_cam.z,8)};
            if(synth.world_mode==2) c.y=world_cell(i*3+1,synth.draw.world_cam.y,8);
            if(view_depth(c,view[2])>22) continue;
            Sculpture g=sculpture(synth.seed,synth.shape,i+3);
            if(synth.world_mode==2 && synth.layout==0) g.family=(unsigned[]){11,13,5,10}[(g.family+synth.layout)%4];
            sculpture_draw(c,synth.world_mode==1?3.5f:3,synth.draw.phase*(i%2?-0.5f:0.5f),g);
        }
    }
    primary_surface=0;
}
static void independent_world(void) {
    world_projection(); primary_surface=0; shell_surface=0;
    if(synth.gear==3 || synth.backdrop==2) { dome(); world_projection(); }
    if(synth.backdrop==1) warp_tunnel();
    if(synth.backdrop!=2) vector_tunnel();
    feedback_trail(); world_projection();
    switch(synth.world_mode) {
    case 1: horizon_world(); break;
    case 2: brutalist_world(); break;
    case 3: quantum_world(); break;
    case 4: ortho_world(); break;
    }
    if(synth.world_mode!=4) world_monuments();
    primary_surface=0; shell_surface=0;
}
#endif
