#ifndef GEEZY_GEOMETRY_H
#define GEEZY_GEOMETRY_H
#include "state.h"
static inline Vec add(Vec a,Vec b) { return (Vec){a.x+b.x,a.y+b.y,a.z+b.z}; }
static inline Vec rotate(Vec p,float a,float b) {
    float x=p.x*cosf(a)+p.z*sinf(a),z=-p.x*sinf(a)+p.z*cosf(a);
    return (Vec){x,p.y*cosf(b)-z*sinf(b),p.y*sinf(b)+z*cosf(b)};
}
static inline Vec scale_vec(Vec p,float scale) { return (Vec){p.x*scale,p.y*scale,p.z*scale}; }
static inline Vec unit(Vec p) {
    float len=sqrtf(p.x*p.x+p.y*p.y+p.z*p.z);
    return len>0.00001f?scale_vec(p,1/len):(Vec){0,1,0};
}
static inline Vec cross(Vec a,Vec b) { return (Vec){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
static inline float view_depth(Vec p,const float row[4]) { return row[0]*p.x+row[1]*p.y+row[2]*p.z+row[3]; }
static inline int box_behind_view(Vec center,Vec half,const float row[4]) {
    float reach=fabsf(row[0])*half.x+fabsf(row[1])*half.y+fabsf(row[2])*half.z;
    return view_depth(center,row)-reach>0;
}
static inline Vec dome_formula(int row,int col) {
    float longitude=(col%24)*6.2831853f/24,latitude=row*3.14159265f/12;
    return (Vec){sinf(latitude)*cosf(longitude),cosf(latitude),sinf(latitude)*sinf(longitude)};
}

static inline uint32_t geometry_hash(uint32_t x) {
    x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; return x^(x>>16);
}
typedef struct {
    unsigned family,p,q,slices,rings,sides,profile,glyph,symmetry;
    float tube,sharpness;
} Sculpture;
static inline Sculpture sculpture(uint32_t seed,int shape,int slot) {
    uint32_t h=geometry_hash(seed^(uint32_t)(shape+1)*0x9e3779b9u^(uint32_t)(slot+1)*0x85ebca6bu);
    unsigned pair=(h>>4)%4;
    return (Sculpture){h%14,(unsigned[]){2,3,3,2}[pair],(unsigned[]){3,4,5,5}[pair],
        8+(h>>7)%5,3+(h>>10)%3,(unsigned[]){4,8,12}[(h>>12)%3],
        (h>>14)%3,(h>>16)%7,3+(h>>20)%6,0.22f+(h>>24)%7*0.045f,0.5f+(h>>27)%5*0.6f};
}
static inline Vec helix_point(float u,float v,unsigned turns,float tube) {
    float a=u*6.2831853f*turns,r=1.8f+(0.24f+tube)*cosf(v);
    return (Vec){r*cosf(a),u*8-4+(0.24f+tube)*sinf(v),r*sinf(a)};
}
static inline Vec mobius_point(float u,float v,unsigned half_twists) {
    float a=u*6.2831853f,t=a*half_twists*0.5f;
    float width=0.95f*cosf(v),edge=0.13f*sinf(v);
    float radial=width*cosf(t)-edge*sinf(t),z=width*sinf(t)+edge*cosf(t);
    return (Vec){(2.8f+radial)*cosf(a),(2.8f+radial)*sinf(a),z};
}
static inline Vec knot_center(float t,unsigned p,unsigned q) {
    float r=2.2f+0.7f*cosf(q*t);
    return (Vec){r*cosf(p*t),r*sinf(p*t),0.7f*sinf(q*t)};
}
static inline void knot_frame(float t,unsigned p,unsigned q,Vec *normal,Vec *side) {
    float r=2.2f+0.7f*cosf(q*t),dr=-0.7f*q*sinf(q*t);
    Vec tangent=unit((Vec){dr*cosf(p*t)-p*r*sinf(p*t),dr*sinf(p*t)+p*r*cosf(p*t),0.7f*q*cosf(q*t)});
    Vec radial={cosf(p*t),sinf(p*t),0};
    float dot=radial.x*tangent.x+radial.y*tangent.y;
    *normal=unit(add(radial,scale_vec(tangent,-dot))); *side=cross(tangent,*normal);
}
static inline float super_radius(float angle,unsigned symmetry,float sharpness) {
    float a=powf(fabsf(cosf(symmetry*angle*0.25f)),sharpness);
    float b=powf(fabsf(sinf(symmetry*angle*0.25f)),sharpness);
    return fmaxf(0.45f,fminf(1.5f,powf(fmaxf(a+b,0.01f),-1/sharpness)));
}
static inline float tunnel_depth(unsigned ring,float distance) { return wrap(ring*8.0f-distance,192); }
static inline float tunnel_fade(float depth) { return clamp01((92-fabsf(depth))/20); }
#endif
