#ifndef GEEZY_CHROME_H
#define GEEZY_CHROME_H
#include <math.h>
#include <stdint.h>
typedef struct { float r,g,b; } ChromeColor;
typedef struct { ChromeColor primary,secondary,accent,bg_tint; } Palette;
static inline ChromeColor color_mix(ChromeColor a,ChromeColor b,float t) {
    return (ChromeColor){a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};
}
static inline ChromeColor hsv(float hue,float saturation,float value) {
    hue=fmodf(hue+720,360)/60;
    float c=value*saturation,x=c*(1-fabsf(fmodf(hue,2)-1)),m=value-c;
    ChromeColor rgb=hue<1?(ChromeColor){c,x,0}:hue<2?(ChromeColor){x,c,0}:hue<3?(ChromeColor){0,c,x}:
        hue<4?(ChromeColor){0,x,c}:hue<5?(ChromeColor){x,0,c}:(ChromeColor){c,0,x};
    return (ChromeColor){rgb.r+m,rgb.g+m,rgb.b+m};
}
static inline Palette palette_generate(uint32_t seed,int harmony) {
    float h=(seed%36000)/100.0f;
    Palette p;
    p.primary=hsv(h,1,1);
    if(harmony==0) { p.secondary=hsv(h+150,0.9f,1); p.accent=hsv(h+210,1,1); }
    else if(harmony==1) { p.secondary=hsv(h+120,1,1); p.accent=hsv(h+240,1,1); }
    else if(harmony==2) { p.secondary=hsv(h+38,0.85f,1); p.accent=hsv(h+85,1,1); }
    else { p.secondary=hsv(h+180,1,1); p.accent=(ChromeColor){1,1,1}; }
    p.bg_tint=harmony==3?(ChromeColor){0,0,0}:color_mix((ChromeColor){0,0,0},p.secondary,0.12f);
    return p;
}
static inline float chrome_smooth(float x) {
    x=fmaxf(0,fminf(1,x)); return x*x*(3-2*x);
}
static inline float chrome_normal_y(int x,int y) {
    float nx=(x-63.5f)/63.5f,ny=(63.5f-y)/63.5f;
    return ny*(1-0.18f*nx*nx*(1-ny*ny));
}
static inline ChromeColor chrome_profile(float ny,ChromeColor sky,ChromeColor ground) {
    if(ny>=-0.28f && ny<0) return (ChromeColor){0,0,0};
    if(ny>=0 && ny<=0.14f) return (ChromeColor){1,1,1};
    if(ny>0.14f) {
        float color=chrome_smooth((ny-0.14f)/0.28f);
        float fade=1-chrome_smooth((ny-0.42f)/0.58f);
        return (ChromeColor){(1+(sky.r-1)*color)*fade,(1+(sky.g-1)*color)*fade,(1+(sky.b-1)*color)*fade};
    }
    float ring=fmaxf(0,1-fabsf(ny+0.65f)/0.30f);
    return (ChromeColor){ground.r*ring,ground.g*ring,ground.b*ring};
}
static inline ChromeColor chrome_palette(float nx,float ny,Palette palette,int harmony) {
    ChromeColor sky=palette.primary;
    if(harmony==2) sky=color_mix(sky,palette.secondary,(nx+1)*0.5f);
    ChromeColor c=chrome_profile(ny,sky,palette.secondary);
    if(ny>=-0.28f && ny<0) return c;
    if(ny>=0 && ny<=0.14f) return c;
    float rim=chrome_smooth((fabsf(nx)-0.55f)/0.4f);
    float side=chrome_smooth((fabsf(nx)-0.2f)/0.45f)*(1-rim);
    c=color_mix(c,palette.secondary,side*0.65f);
    c=color_mix(c,palette.accent,rim*0.85f);
    if(harmony==3) {
        float light=fmaxf(c.r,fmaxf(c.g,c.b));
        return light<0.12f?(ChromeColor){0,0,0}:light>0.85f?(ChromeColor){1,1,1}:ny>0?palette.primary:palette.secondary;
    }
    return c;
}
#endif
