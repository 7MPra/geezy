#ifndef GEEZY_WORLD_MATH_H
#define GEEZY_WORLD_MATH_H
#include "geometry.h"
#include "font.h"

enum { CLOUD_POINTS=2304 };
// The 96-unit period matches world_cam, including negative coordinates and wraps.
static inline float world_cell(int index,float camera,float spacing) { return wrap(index*spacing-camera,96); }
static inline uint32_t terrain_hash(int x,int z,uint32_t seed) {
    return geometry_hash(seed^(uint32_t)(x+1)*0x9e3779b9u^(uint32_t)(z+1)*0x85ebca6bu);
}
static inline float flight_bank(const Synth *s) { return fmaxf(-70,fminf(70,s->draw.roll+s->bank*2)); }
static inline Vec flight_target(const Synth *s) {
    return unit((Vec){s->flow.x+s->draw.aim_x*0.65f,
        fmaxf(-0.45f,fminf(0.45f,s->flow.y*0.25f+s->draw.aim_y*0.35f)),
        -fmaxf(0.35f,fabsf(s->flow.z))});
}
static inline Vec cloud_lattice(int i,Vec camera) {
    return (Vec){world_cell(i%12,camera.x,8),world_cell((i/12)%12,camera.y,8),world_cell(i/144,camera.z,6)};
}
static inline Vec cloud_free(int i,const Pose *p,uint32_t seed) {
    Vec q=cloud_lattice(i,p->world_cam);
    uint32_t h=geometry_hash(seed+(unsigned)i);
    float a=p->phase*0.8f+(h&255)*0.02464f;
    // Periodic displacement before wrapping prevents seams when world_cam recycles.
    q.x=wrap(q.x+4*sinf(q.y*0.06544985f+a)+2*cosf(q.z*0.1308997f-a),96);
    q.y=wrap(q.y+4*cosf(q.z*0.06544985f+a)+2*sinf(q.x*0.1308997f+a),96);
    q.z=wrap(q.z+3*sinf(q.x*0.06544985f-a),96);
    return q;
}
static inline Vec cloud_shell(int i,float phase) {
    int n=i/2; float y=1-2*(n+0.5f)/(CLOUD_POINTS/2),r=sqrtf(1-y*y),a=n*2.39996323f;
    Vec q=rotate((Vec){r*cosf(a),y,r*sinf(a)},phase*0.1f,0.2f);
    return add(scale_vec(q,i%2?22:16),(Vec){0,0,-28});
}
static inline int cloud_glyph_cells(Vec cells[125],int shape) {
    const char *word=(const char*[]){"HYPER","VOID",".+","01","ERR"}[shape%5];
    int count=0,length=(int)strlen(word);
    for(int c=0;c<length;c++) for(int row=0;row<5;row++) for(int col=0;col<5;col++)
        if(font_glyph(word[c])[row]&(1<<(4-col)))
            cells[count++]=(Vec){(c*6+col-(length*6-2)*0.5f)*1.6f,(2-row)*2.4f,0};
    return count;
}
static inline Vec cloud_glyph(int i,const Vec *cells,int count) {
    Vec p=cells[i%count]; int layer=i/count;
    // Subdots fill each lit pixel; repeated depth planes make the word volumetric.
    p.x+=(layer%2?0.36f:-0.36f); p.y+=((layer/2)%2?0.48f:-0.48f);
    int planes=(CLOUD_POINTS+count*4-1)/(count*4);
    p.z=-18-(layer/4)*24.0f/(planes-1); return p;
}
static inline float crystal_amount(const Synth *s) {
    if(s->motion_mode==2) return 1;
    if(s->slow) return clamp01(s->tension*2.5f);
    return clamp01(s->draw.burst/0.8f);
}
static inline Vec cloud_position(int i,const Synth *s,const Vec *cells,int count) {
    Vec free=cloud_free(i,&s->draw,s->seed),target;
    int form=(s->shape+s->layout)%3;
    if(form==0) target=cloud_lattice(i,s->draw.world_cam);
    else if(form==1) target=cloud_shell(i,s->draw.phase);
    else target=cloud_glyph(i,cells,count);
    float amount=crystal_amount(s); amount=amount*amount*(3-2*amount);
    return add(scale_vec(free,1-amount),scale_vec(target,amount));
}
static inline float cloud_fade(Vec p) {
    float edge=fmaxf(fabsf(p.x),fmaxf(fabsf(p.y),fabsf(p.z)));
    return clamp01((48-edge)/6);
}
#endif
