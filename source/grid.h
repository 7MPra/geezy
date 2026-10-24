#ifndef GEEZY_GRID_H
#define GEEZY_GRID_H
#include <stdint.h>
#define GRID_TEXELS 5488
static inline unsigned grid_index(unsigned size,unsigned x,unsigned y) {
    return ((y/4)*((size+3)/4)+x/4)*16+(y%4)*4+x%4;
}
static inline void grid_mipmaps(uint16_t *pixels) {
    unsigned previous=0,next=4096;
    for(unsigned size=32;size;size/=2) {
        for(unsigned y=0;y<size;y++) for(unsigned x=0;x<size;x++) {
            unsigned r=0,g=0,b=0;
            for(unsigned j=0;j<2;j++) for(unsigned i=0;i<2;i++) {
                uint16_t c=pixels[previous+grid_index(size*2,x*2+i,y*2+j)];
                r+=c>>11; g+=(c>>5)&63; b+=c&31;
            }
            pixels[next+grid_index(size,x,y)]=((r/4)<<11)|((g/4)<<5)|(b/4);
        }
        previous=next; next+=size>=4?size*size:16;
    }
}
#endif
