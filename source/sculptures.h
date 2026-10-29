// Extra families reuse the bounded mesh cache and unit-box display lists.
static void twisted_mesh(Vec center,float size,float angle,Sculpture g,int blade) {
    surface(CHROME);
    MeshList *cached=mesh_fast()?mesh_lookup((MeshKey){blade?5:4,g.p,g.q,surface_kind==WIRE,g.tube,0,0}):NULL;
    if(cached && cached->bytes) { cached_mesh_draw(cached,center,size,angle,angle*0.37f); return; }
    int rows=64,cols=blade?4:8;
    for(int i=0;i<=rows;i++) for(int j=0;j<=cols;j++) {
        float u=i/(float)rows,v=j*6.2831853f/cols;
        Vec p=blade?mobius_point(u,v,g.q%3*2+1):helix_point(u,v,g.p,g.tube);
        float eps=0.0005f;
        Vec a=blade?mobius_point(u+eps,v,g.q%3*2+1):helix_point(u+eps,v,g.p,g.tube);
        Vec b=blade?mobius_point(u,v+eps,g.q%3*2+1):helix_point(u,v+eps,g.p,g.tube);
        Vec n=unit(cross(add(a,scale_vec(p,-1)),add(b,scale_vec(p,-1))));
        if(!blade) { p=rotate(p,g.q*1.04719755f,0); n=rotate(n,g.q*1.04719755f,0); }
        mesh_pos[i][j]=p; mesh_normal[i][j]=n;
    }
    if(cached && compile_mesh(cached,rows,cols,0,0)) cached_mesh_draw(cached,center,size,angle,angle*0.37f);
    else mesh(rows,cols,center,size,angle,angle*0.37f,1);
}
static Vec sculpture_local(Vec center,Vec p,float size,float angle) {
    return add(center,scale_vec(rotate(p,angle,angle*0.37f),size));
}
static void pipe_labyrinth(Vec center,float size,float angle,Sculpture g) {
    surface(CHROME);
    for(unsigned strand=0;strand<3;strand++) {
        Vec p={(float)strand*1.2f-1.2f,-3,(float)strand-1};
        for(unsigned i=0;i<9;i++) {
            uint32_t h=geometry_hash(g.p*731+g.q*47+strand*31+i);
            Vec q=p; float step=(h&1?1:-1)*(1.2f+(h%3)*0.5f);
            if(i%3==0) q.x=fmaxf(-3,fminf(3,p.x+step));
            if(i%3==1) q.y=-3+i*0.7f;
            if(i%3==2) q.z=fmaxf(-3,fminf(3,p.z+step));
            Vec a=sculpture_local(center,p,size,angle),b=sculpture_local(center,q,size,angle);
            beam(a,b,(0.3f+g.tube*0.4f)*size,255);
            box(b,scale_vec((Vec){0.48f,0.48f,0.48f},size),angle,255); p=q;
        }
    }
}
static void mechanical_helix(Vec center,float size,float angle,Sculpture g) {
    int strands=g.profile==2?3:2;
    for(int i=0;i<strands;i++) { Sculpture strand=g; strand.q=i*(6/strands); twisted_mesh(center,size,angle,strand,0); }
    surface(CHROME);
    for(int i=0;i<12;i++) {
        float t=i/11.0f,a=t*6.2831853f*g.p;
        Vec p={1.8f*cosf(a),t*8-4,1.8f*sinf(a)};
        p=rotate(p,(i%strands)*6.2831853f/strands,0);
        Vec q=rotate(p,6.2831853f/strands,0);
        beam(sculpture_local(center,p,size,angle),sculpture_local(center,q,size,angle),0.16f*size,255);
    }
}
static void menger_cage(Vec center,float size,float angle,Sculpture g) {
    surface(CHROME);
    for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) for(int z=-1;z<=1;z++) {
        if((x==0)+(y==0)+(z==0)>=2) continue;
        box(sculpture_local(center,(Vec){x*2.1f,y*2.1f,z*2.1f},size,angle),scale_vec((Vec){0.95f,0.95f,0.95f},size),angle,255);
    }
    frame_cluster(center,-angle,size*0.48f);
    octahedron(center,size*(0.7f+g.tube),-angle*1.7f,CHROME,1);
    surface(CHROME);
    beam(sculpture_local(center,(Vec){-4,-3,-2},size,angle),sculpture_local(center,(Vec){4,3,2},size,angle),0.22f*size,255);
}
static void mechanical_spine(Vec center,float size,float phase,Sculpture g) {
    surface(CHROME); int count=10+g.slices%5; Vec previous=center;
    for(int i=0;i<count;i++) {
        float wave=roundf(sinf(phase*1.7f-i*0.55f)*4)/4;
        Vec p={wave*1.8f,(i-(count-1)*0.5f)*0.7f,cosf(phase-i*0.4f)*0.65f};
        Vec c=sculpture_local(center,p,size,phase*0.25f);
        if(i) beam(previous,c,0.25f*size,255);
        for(int side=-1;side<=1;side+=2) {
            Vec tip=add(p,(Vec){side*(1.3f+g.tube),0.65f,-0.5f});
            beam(c,sculpture_local(center,tip,size,phase*0.25f),0.22f*size,255);
        }
        box(c,scale_vec((Vec){0.45f,0.22f,0.5f},size),phase*0.25f,255); previous=c;
    }
}
