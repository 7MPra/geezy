#ifndef GEEZY_OVERLAY_H
#define GEEZY_OVERLAY_H
// Included after the GX material/vertex helpers; all coordinates are screen space.
static int graphic_pattern;
static void graphic_material(ChromeColor color,int pattern,int window) {
    graphic_pattern=pattern;
    material(window?&history:pattern==2?&halftone_tex:pattern?&dither_tex:NULL); tint=gx_color(color);
    if(window) {
        GX_SetTevSwapModeTable(GX_TEV_SWAP1,GX_CH_GREEN,GX_CH_BLUE,GX_CH_RED,GX_CH_ALPHA);
        GX_SetTevSwapMode(GX_TEVSTAGE0,GX_TEV_SWAP0,GX_TEV_SWAP1);
    }
}
static void graphic_vertex(Vec p,int window) {
    if(window) {
        float a=synth.draw.phase*0.05f,x=p.x*cosf(a)-p.y*sinf(a),y=p.x*sinf(a)+p.y*cosf(a);
        vertex(p,0.5f+x*0.38f,0.5f-y*0.38f,255,255);
    } else vertex(p,graphic_pattern==2?(p.x+1)*0.5f:p.x*36,p.y*24,255,255);
}
static void graphic_polygon(const Vec *points,int count,ChromeColor color,int pattern,int window) {
    Vec center={0,0,-1};
    for(int i=0;i<count;i++) { center.x+=points[i].x/count; center.y+=points[i].y/count; }
    for(int layer=0;layer<3;layer++) {
        graphic_material(layer==0?(ChromeColor){0,0,0}:layer==1?(ChromeColor){1,1,1}:color,layer==2?pattern:0,layer==2?window:0);
        float scale=layer==2?1:1.025f;
        GX_Begin(GX_TRIANGLEFAN,GX_VTXFMT0,count);
        for(int i=0;i<count;i++) {
            Vec p={center.x+(points[i].x-center.x)*scale+(layer==0?0.018f:0),
                center.y+(points[i].y-center.y)*scale-(layer==0?0.025f:0),-1};
            graphic_vertex(p,layer==2?window:0);
        }
        GX_End();
    }
}
static Vec screen_rotate(Vec p,float angle,float x,float y) {
    return (Vec){(p.x*cosf(angle)-p.y*sinf(angle))/aspect+x,p.x*sinf(angle)+p.y*cosf(angle)+y,-1};
}
static void graphic_box(float x,float y,float w,float h,float angle,ChromeColor color,int pattern) {
    Vec points[4];
    for(int i=0;i<4;i++) points[i]=screen_rotate((Vec){(i==1||i==2)?w:-w,i>=2?h:-h,0},angle,x,y);
    graphic_polygon(points,4,color,pattern,0);
}
static void graphic_arc(float x,float y,float radius,float width,float start,float length,ChromeColor color,int pattern) {
    width*=synth.mask_env;
    for(int layer=0;layer<3;layer++) {
        graphic_material(layer==0?(ChromeColor){0,0,0}:layer==1?(ChromeColor){1,1,1}:color,layer==2?pattern:0,0);
        float border=layer==2?0:0.006f*synth.mask_env;
        GX_Begin(GX_TRIANGLESTRIP,GX_VTXFMT0,98);
        for(int i=0;i<=48;i++) for(int edge=0;edge<2;edge++) {
            float a=start+length*i/48,r=radius+(edge?1:-1)*(width+border);
            graphic_vertex((Vec){x+r*cosf(a)/aspect+(layer==0?0.018f:0),y+r*sinf(a)-(layer==0?0.025f:0),-1},0);
        }
        GX_End();
    }
}
static void depth_polygon(const Vec *p,int count) {
    GX_Begin(GX_TRIANGLEFAN,GX_VTXFMT0,count);
    for(int i=0;i<count;i++) vertex((Vec){p[i].x,p[i].y,-1},0,0,255,255);
    GX_End();
}
static void stencil_begin(void) {
    material(NULL); GX_SetColorUpdate(GX_FALSE); GX_SetZMode(GX_TRUE,GX_ALWAYS,GX_TRUE);
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
    for(int i=0;i<4;i++) vertex((Vec){i==1||i==2?1:-1,i>=2?1:-1,-9},0,0,255,255);
    GX_End();
}
static void stencil_cover(float env) {
    GX_SetColorUpdate(GX_TRUE); GX_SetZMode(GX_TRUE,GX_LESS,GX_FALSE);
    graphic_material(palette.primary,1,0);
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
    for(int i=0;i<4;i++) graphic_vertex((Vec){(i==1||i==2?1:-1)*2*env,(i>=2?1:-1)*2*env,-5},0);
    GX_End();
    // The same depth mask clips tiny crosses to the opaque poster, never to its windows.
    material(NULL); tint=gx_color(palette.accent);
    for(int y=-6;y<=6;y++) for(int x=-10;x<=10;x++) {
        float px=x*0.14f,py=y*0.18f;
        if(fabsf(px)>2*env || fabsf(py)>2*env) continue;
        GX_Begin(GX_QUADS,GX_VTXFMT0,8);
        const float rects[2][4]={{-0.012f,-0.002f,0.024f,0.004f},{-0.002f,-0.012f,0.004f,0.024f}};
        for(int r=0;r<2;r++) for(int i=0;i<4;i++) vertex((Vec){px+rects[r][0]+((i==1||i==2)?rects[r][2]:0),py+rects[r][1]+(i>=2?rects[r][3]:0),-5},0,0,255,255);
        GX_End();
    }
    GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
}
static void overlay(void) {
    float env=synth.mask_env;
    if(env<=0.001f) return;
    ortho(); GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    float t=synth.mask_clock,turn=synth.draw.phase*0.3f;
    switch(synth.mask_pattern) {
    case 0:
        for(int ring=0;ring<5;ring++) {
            float r=(0.24f+ring*0.14f)*env,a=(ring%2?-1:1)*(t*0.8f+(1-env)*3)+ring;
            graphic_arc(0,0,r,0.018f+ring*0.003f,a,4.7f,palette_role(ring),ring%2);
            for(int tick=0;tick<12;tick++) graphic_arc(0,0,r+0.065f,0.012f,a+tick*0.52f,0.06f,palette.accent,0);
        }
        break;
    case 1: {
        float travel=synth.mask_active?-2*(1-env):synth.mask_release*9;
        for(int i=0;i<5;i++) {
            float x=-1+i*0.5f+travel+0.12f*sinf(t*2+i);
            graphic_box(x,0,0.12f*env,1.8f,0.785398f+(i%2)*0.2618f,palette_role(i),i%2);
            graphic_box(x+0.14f,0,0.006f*env,1.8f,0.785398f,palette.accent,0);
        }
        break;
    }
    case 2: {
        Vec windows[3][4]; stencil_begin();
        for(int panel=0;panel<3;panel++) {
            for(int i=0;i<4;i++) windows[panel][i]=screen_rotate((Vec){(i==1||i==2?0.24f:-0.24f)*env,(i>=2?0.78f:-0.78f)*env,0},-0.15f,(panel-1)*0.57f*env,0);
            depth_polygon(windows[panel],4);
        }
        stencil_cover(env);
        for(int layer=0;layer<3;layer++) {
            graphic_material(layer==0?(ChromeColor){0,0,0}:layer==1?(ChromeColor){1,1,1}:palette.accent,0,0);
            GX_SetLineWidth(layer==0?24:layer==1?18:6,GX_TO_ZERO);
            for(int panel=0;panel<3;panel++) {
                GX_Begin(GX_LINESTRIP,GX_VTXFMT0,5);
                for(int i=0;i<5;i++) {
                    Vec p=windows[panel][i%4]; if(layer==0) { p.x+=0.018f; p.y-=0.025f; }
                    graphic_vertex(p,0);
                }
                GX_End();
            }
        }
        break;
    }
    case 3: {
        float radius=0.78f*env; Vec circle[64];
        for(int i=0;i<64;i++) circle[i]=(Vec){radius*cosf(i*6.2831853f/64)/aspect,radius*sinf(i*6.2831853f/64),-1};
        stencil_begin(); depth_polygon(circle,64); stencil_cover(env);
        graphic_arc(0,0,radius+0.025f,0.023f,0,6.2831853f,palette.accent,0);
        for(int i=0;i<32;i++) graphic_arc(0,0,radius+0.08f,0.018f,turn+i*0.19635f,0.045f,palette.secondary,0);
        break;
    }
    case 4: {
        Vec blade[4];
        for(int i=0;i<4;i++) blade[i]=screen_rotate((Vec){(i==1||i==2?0.55f:-0.55f)*env,i>=2?2:-2,0},0.5f+0.3f*sinf(t),wrap(t*0.45f,2),0);
        graphic_polygon(blade,4,(ChromeColor){1,1,1},0,1);
        break;
    }
    case 5: {
        const char *word=(const char*[]){"HYPER","VOID",".+"}[(synth.seed>>3)%3];
        float dot=0.52f*env,width=strlen(word)*6*dot/aspect;
        float left=1.1f-fmodf(t*0.8f,width+2.2f);
        for(int ch=0;word[ch];ch++) for(int row=0;row<5;row++) for(int col=0;col<5;col++)
            if(font_glyph(word[ch])[row]&(1<<(4-col)))
                graphic_box(left+(ch*6+col)*dot/aspect,1.3f*env-row*dot,dot*0.46f,dot*0.46f,0,palette_role(ch),1);
        break;
    }
    case 6:
        // Six columns repeat both the two-shape checker and the three-color sequence.
        for(int y=0;y<8;y++) for(int x=-6;x<13;x++) {
            float reveal=clamp01(env*1.6f-((x%6+6)%6+y)*0.035f);
            float px=-1.1f+x*0.2f+checker_scroll(synth.tick,17)*1.2f,py=-0.9f+y*0.26f;
            if((x+y)%2==0) graphic_box(px,py,0.11f*reveal,0.09f*reveal,0,palette_role(x+y),2);
            else {
                graphic_box(px,py,0.045f*reveal,0.008f*reveal,0,palette.accent,0);
                graphic_box(px,py,0.008f*reveal,0.045f*reveal,0,palette.accent,0);
            }
        }
        graphic_box(0,0.8f,0.8f*env,0.008f,0,palette.primary,0);
        graphic_box(0,-0.8f,0.8f*env,0.008f,0,palette.secondary,0);
        break;
    case 7:
        for(int i=0;i<8;i++) {
            float a=i*0.785398f+turn+(1-env)*0.8f,r=3.0f-env*2.7f;
            Vec p[4]={screen_rotate((Vec){r,0,0},a,0,0),screen_rotate((Vec){2.2f,-0.65f,0},a,0,0),
                screen_rotate((Vec){2.5f,0.7f,0},a,0,0),screen_rotate((Vec){r+0.4f,0.28f,0},a,0,0)};
            graphic_polygon(p,4,palette_role(i),i%2,0);
        }
        break;
    }
    GX_SetColorUpdate(GX_TRUE); GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
    GX_SetTevSwapMode(GX_TEVSTAGE0,GX_TEV_SWAP0,GX_TEV_SWAP0);
}
#endif
