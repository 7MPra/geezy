#ifndef GEEZY_WORLDS_H
#define GEEZY_WORLDS_H
#include "world_math.h"

static void world_projection(void) {
    in_3d=1; quantize_vertices=synth.slow;
    Mtx44 p; Mtx look,bank;
    guVector eye={0,0,0},up={0,1,0},at;
    float roll=flight_bank(&synth);
    if(synth.world_mode==4) {
        // Fixed extents: neither depth nor charge FOV can change the projected scale.
        guOrtho(p,11,-11,-11*aspect,11*aspect,0.2f,160);
        GX_LoadProjectionMtx(p,GX_ORTHOGRAPHIC);
        eye.z=32; at=(guVector){synth.draw.aim_x*3,synth.draw.aim_y*3,0};
        roll=synth.draw.roll;
    } else {
        guPerspective(p,synth.draw.fov,aspect,0.2f,160);
        GX_LoadProjectionMtx(p,GX_PERSPECTIVE);
        Vec target=flight_target(&synth); at=(guVector){target.x,target.y,target.z};
    }
    guLookAt(look,&eye,&up,&at);
    guVector axis={0,0,1}; guMtxRotAxisDeg(bank,&axis,roll);
    guMtxConcat(bank,look,view); GX_LoadPosMtxImm(view,GX_PNMTX0);
    GX_SetFog(synth.world_mode==4?GX_FOG_ORTHO_LIN:GX_FOG_PERSP_LIN,
        synth.world_mode==4?65:32,synth.world_mode==4?140:47,0.2f,160,clear_color());
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
static void brutalist_world(void) {
    Vec cam=synth.draw.world_cam;
    for(int z=0;z<8;z++) {
        float depth=world_cell(z,cam.z,12);
        int step=(int)floorf(synth.draw.phase*2)+synth.cut+z;
        float drive=((step%4+4)%4)*0.65f+fminf(synth.draw.burst,2)*0.7f;
        for(int side=-1;side<=1;side+=2) {
            world_surface((z+side)%3==0?MATTE:CHROME,z%3);
            box((Vec){side*(8-drive),0,depth},(Vec){3,20,1.3f},0,255);
            world_surface(GRID,(z+1)%3);
            box((Vec){0,side*(9-drive),depth+4},(Vec){18,3,1.2f},0,255);
            // Outside the narrow passage, diagonals reveal a second, distant scale.
            world_surface(GRID,(z+2)%3);
            beam((Vec){side*14,-18,depth-6},(Vec){side*23,18,depth+6},0.7f,255);
            beam((Vec){side*23,-18,depth-6},(Vec){side*14,18,depth+6},0.7f,255);
        }
    }
    // Camera-relative shutters provide near occlusion without ever closing the entire aperture.
    int step=synth.cut+(int)floorf(synth.draw.phase*3);
    float slide=1.8f+((step%4+4)%4)*0.45f;
    float drift=wrap(cam.x,8)*0.12f;
    world_surface(MATTE,0);
    box((Vec){-slide+drift-1.8f,0,-4},(Vec){1.8f,16,0.65f},0,255);
    world_surface(CHROME,1);
    box((Vec){slide+drift+1.8f,0,-5.5f},(Vec){1.8f,16,0.8f},0,255);
    world_surface(SOLID,2);
    beam((Vec){-slide+drift,-16,-3.3f},(Vec){-slide+drift,16,-3.3f},0.035f,255);
}
static Vec cloud_vertices[CLOUD_POINTS];
static void cloud_ink(int i) {
    if(synth.slow) return;
    tint=(i%4==3)?(GXColor){255,255,255,255}:gx_color(palette_role(i%4));
}
static void quantum_world(void) {
    Vec cells[125]; int count=cloud_glyph_cells(cells,synth.shape);
    for(int i=0;i<CLOUD_POINTS;i++) cloud_vertices[i]=cloud_position(i,&synth,cells,count);
    world_surface(WIRE,2);
    // Bounded local-neighbor search: no all-pairs work on the Wii CPU.
    GX_SetLineWidth(6,GX_TO_ZERO);
    for(int row=0;row<16;row++) {
        Vec lines[144*6]; int n=0;
        for(int i=row*144;i<(row+1)*144;i++) for(int k=0;k<3;k++) {
            int j=i+(int[]){1,12,144}[k]; if(j>=CLOUD_POINTS) continue;
            Vec d=add(cloud_vertices[j],scale_vec(cloud_vertices[i],-1));
            float length=d.x*d.x+d.y*d.y+d.z*d.z;
            if(length>0.05f && length<125) { lines[n++]=cloud_vertices[i]; lines[n++]=cloud_vertices[j]; }
        }
        cloud_ink(row); begin_3d(GX_LINES,n);
        for(int i=0;i<n;i++) vertex(lines[i],0,0,(u8)(cloud_fade(lines[i])*150),255);
        GX_End();
    }
    for(int group=0;group<4;group++) {
        cloud_ink(group); GX_SetPointSize(12+group*6+(u8)(synth.tension*12),GX_TO_ZERO);
        begin_3d(GX_POINTS,CLOUD_POINTS/4);
        for(int i=group;i<CLOUD_POINTS;i+=4) vertex(cloud_vertices[i],0,0,(u8)(cloud_fade(cloud_vertices[i])*255),255);
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
    primary_surface=1;
    for(int zone=0;zone<4;zone++) {
        Sculpture g=sculpture(synth.seed,synth.shape,zone);
        g.family=(unsigned[]){4,5,6,7}[(zone+synth.layout)%4];
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
static void independent_world(void) {
    world_projection(); primary_surface=0; shell_surface=0;
    switch(synth.world_mode) {
    case 1: horizon_world(); break;
    case 2: brutalist_world(); break;
    case 3: quantum_world(); break;
    case 4: ortho_world(); break;
    }
    primary_surface=0; shell_surface=0;
}
#endif
