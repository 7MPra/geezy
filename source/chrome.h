#ifndef GEEZY_CHROME_H
#define GEEZY_CHROME_H
#include <math.h>
typedef struct { float r,g,b; } ChromeColor;
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
    float ring=fmaxf(0,1-fabsf(ny+0.65f)/0.045f);
    return (ChromeColor){ground.r*ring,ground.g*ring,ground.b*ring};
}
#endif
