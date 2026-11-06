// Standalone renderer with C arrays/allocation/I/O and OpenMP. No C++ STL.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <new> // placement construction only; no allocation or STL container
#include <omp.h>
#ifdef __linux__
#include <sched.h>
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

static const int max_depth=24;
static float maximum(float a,float b) { return a<b ? b:a; }
static int minimum(int a,int b) { return a<b ? a:b; }
struct Float2 {
    float x, y;

    Float2() = default;
    Float2(float x, float y) : x(x), y(y) {}
    Float2 operator+(Float2 b) const { return {x + b.x, y + b.y}; }
    Float2 operator-(Float2 b) const { return {x - b.x, y - b.y}; }
    Float2 operator*(Float2 b) const { return {x * b.x, y * b.y}; }
    Float2 operator*(float scale) const { return {x * scale, y * scale}; }
};

struct Float3 {
    float x, y, z;

    Float3() = default;
    Float3(float x, float y, float z) : x(x), y(y), z(z) {}
    Float3 operator+(Float3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    Float3 operator-(Float3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    Float3 operator*(Float3 b) const { return {x * b.x, y * b.y, z * b.z}; }
    Float3 operator*(float scale) const { return {x * scale, y * scale, z * scale}; }
    Float3 operator+=(Float3 b) { *this = *this + b; return *this; }
    Float3 operator*=(Float3 b) { *this = *this * b; return *this; }
};

float dot(Float3 a, Float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Float3 cross(Float3 a, Float3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Float3 normalize(Float3 v) {
    return v * (1 / sqrtf(dot(v, v)));
}

struct Ray { Float3 origin, direction; };

struct Hit {
    float t = INFINITY, u = 0, v = 0;
    int triangle = -1;
};

struct Triangle {
    Float3 p, e1, e2;
    Float3 normal;
    Float3 albedo, emission;

    Triangle() = default;
    Triangle(Float3 p, Float3 a, Float3 b, Float3 color, Float3 light = {})
        : p(p), e1(a - p), e2(b - p),
          normal(normalize(cross(e1, e2))),
          albedo(color), emission(light) {}

    bool intersect(Ray ray, Hit& hit) const {
        Float3 h = cross(ray.direction, e2);
        float determinant = dot(e1, h);
        if (determinant < 1e-10f) return false;

        float inverse = 1 / determinant;
        Float3 offset = ray.origin - p;
        float u = dot(offset, h) * inverse;
        if (u < 0 || u > 1) return false;

        Float3 q = cross(offset, e1);
        float v = dot(ray.direction, q) * inverse;
        if (v < 0 || u + v > 1) return false;

        float t = dot(e2, q) * inverse;
        if (t <= 1e-4f || t >= hit.t) return false;
        hit.t = t;
        hit.u = u;
        hit.v = v;
        return true;
    }
};

struct Camera {
    Float3 position, target, up;
    float fovy, aspect;
    Float3 u, v, w;

    void compute_basis() {
        w = normalize(target - position);
        u = normalize(cross(up, w));
        v = cross(w, u);
    }
};

struct Basis { Float3 tangent,bitangent; };
struct Light { int triangle;float cumulative_area; };
struct SceneData {
    Triangle triangles[64];int count=0;
    Camera camera;
    Basis bases[64];Light lights[64];int light_count=0;float light_area=0;
};
void add_quad(SceneData& scene, Float3 p, Float3 u, Float3 v,
              Float3 color, Float3 emission = {}) {
    if (scene.count == 64) abort();
    scene.triangles[scene.count++] = Triangle(p, p + u, p + u + v, color, emission);
    if (scene.count == 64) abort();
    scene.triangles[scene.count++] = Triangle(p, p + u + v, p + v, color, emission);
}

void add_block(SceneData& scene, Float3 center, Float3 size, float degrees,
               Float3 color, Float3 emission = {}) {
    float angle = degrees * M_PI / 180;
    float c = cosf(angle), s = sinf(angle);
    Float3 u{size.x * c, 0, -size.x * s};
    Float3 v{size.z * s, 0, size.z * c};
    Float3 up{0, size.y, 0};
    Float3 p = center - (u + v + up) * 0.5f;

    add_quad(scene, p, up, u, color, emission);
    add_quad(scene, p + v, u, up, color, emission);
    add_quad(scene, p, v, up, color, emission);
    add_quad(scene, p + u, up, v, color, emission);
    add_quad(scene, p + up, v, u, color, emission);
    add_quad(scene, p, u, v, color, emission);
}

SceneData cornell_box() {
    SceneData scene;
    scene.camera = {
        {278, 278, -800}, {278, 278, 278}, {0, 1, 0}, 40, 1.0f, {}, {}, {}
    };
    scene.camera.compute_basis();

    Float3 white{0.73f, 0.73f, 0.73f};
    Float3 red{0.65f, 0.05f, 0.05f};
    Float3 green{0.12f, 0.45f, 0.15f};

    add_quad(scene, {0, 0, 0}, {0, 555, 0}, {0, 0, 555}, red);
    add_quad(scene, {555, 0, 0}, {0, 0, 555}, {0, 555, 0}, green);
    add_quad(scene, {0, 0, 0}, {0, 0, 555}, {555, 0, 0}, white);
    add_quad(scene, {0, 555, 0}, {555, 0, 0}, {0, 0, 555}, white);
    add_quad(scene, {0, 0, 555}, {0, 555, 0}, {555, 0, 0}, white);

    add_quad(scene, {213, 554, 227}, {130, 0, 0}, {0, 0, 105}, {}, {15, 15, 15});

    add_block(scene, {370, 82.5f, 169}, {165, 165, 165}, 18, white);
    add_block(scene, {187, 165, 351}, {165, 330, 165}, -15, white);

    return scene;
}

// PCG XSH RR, adapted from M. E. O'Neill's Apache-2.0 minimal C implementation.
// See licenses/PCG-NOTICE and licenses/Apache-2.0.txt.
struct Random {
    uint64_t state = 0, increment = 1;
    static constexpr const char* name() {
#if defined(RNG_XOROSHIRO128PP)
        return "xoroshiro128++";
#elif defined(RNG_XOROSHIRO128SS)
        return "xoroshiro128**";
#else
        return "pcg32";
#endif
    }
#if defined(RNG_XOROSHIRO128PP) || defined(RNG_XOROSHIRO128SS)
    // Blackman/Vigna xoroshiro128++ 1.0 and SplitMix64; public domain.
    // increment is the second state word in this optional build, retaining
    // the existing 16-byte AoS/SoA storage interface. See licenses/XOROSHIRO-NOTICE.
    static uint64_t rotate(uint64_t x, int k) { return (x<<k)|(x>>(64-k)); }
    static uint64_t mix(uint64_t x) {
        x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;
        x=(x^(x>>27))*0x94d049bb133111ebULL;
        return x^(x>>31);
    }
    static uint64_t seed_key(uint64_t seed) { return mix(seed^0xd1b54a32d192ed03ULL); }
#else
    static uint64_t seed_key(uint64_t seed) { return seed; }
#endif
    uint32_t next() {
#if defined(RNG_XOROSHIRO128PP) || defined(RNG_XOROSHIRO128SS)
        auto first=state, second=increment;
#ifdef RNG_XOROSHIRO128PP
        auto result=rotate(first+second,17)+first;
        second^=first;
        state=rotate(first,49)^second^(second<<21);
        increment=rotate(second,28);
#else
        auto result=rotate(first*5,7)*9;
        second^=first;
        state=rotate(first,24)^second^(second<<16);
        increment=rotate(second,37);
#endif
        return uint32_t(result>>32);
#else
        auto previous = state;
        state = previous * 6364136223846793005ULL + increment;
        auto bits = uint32_t(((previous >> 18) ^ previous) >> 27);
        auto rotation = uint32_t(previous >> 59);
        return (bits >> rotation) | (bits << ((-rotation) & 31));
#endif
    }
    void seed(uint64_t value, uint64_t stream) {
#if defined(RNG_XOROSHIRO128PP) || defined(RNG_XOROSHIRO128SS)
        auto key=value^stream;
        state=mix(key+0x9e3779b97f4a7c15ULL);
        increment=mix(key+2*0x9e3779b97f4a7c15ULL);
        // Consecutive SplitMix inputs are distinct; their pair cannot be zero.
#else
        state = 0; increment = (stream << 1) | 1;
        next(); state += value; next();
#endif
    }
    float uniform() { return float(next() >> 8) * 0x1p-24f; }
};

Float3 sample_hemisphere(Float3 normal, Random& random) {
    float z = 2 * random.uniform() - 1;
    float phi = 2 * M_PI * random.uniform();
    float r = sqrtf(maximum(0.0f, 1 - z * z));
#if defined(__GLIBC__)
    float sine, cosine;
    __builtin_sincosf(phi, &sine, &cosine);
    Float3 direction{r * cosine, r * sine, z};
#else
    Float3 direction{r * cosf(phi), r * sinf(phi), z};
#endif
    return dot(direction, normal) < 0 ? direction * -1 : direction;
}

struct Path {
    Ray ray;
    Float3 weight{1, 1, 1}, result{};
    Random random;
    bool active = false;
};


static void prepare(SceneData& scene) {
    for(int i=0;i<scene.count;++i) {
        const Triangle& t=scene.triangles[i];
        Float3 axis=fabsf(t.normal.z)<.9f ? Float3{0,0,1}:Float3{0,1,0};
        Float3 tangent=normalize(cross(axis,t.normal));
        scene.bases[i]={tangent,cross(t.normal,tangent)};
        float area=.5f*sqrtf(dot(cross(t.e1,t.e2),cross(t.e1,t.e2)));
        if(area>0 && dot(t.emission,t.emission)>0) {
            scene.light_area+=area;scene.lights[scene.light_count++]={i,scene.light_area};
        }
    }
}
static void intersect_batch(const SceneData& scene,const Ray* rays,Hit* hits) {
    float ox[8]{},oy[8]{},oz[8]{},dx[8]{},dy[8]{},dz[8]{},ts[8],us[8]{},vs[8]{};
    int ids[8];
    for(int r=0;r<8;++r) {
        ox[r]=rays[r].origin.x;oy[r]=rays[r].origin.y;oz[r]=rays[r].origin.z;
        dx[r]=rays[r].direction.x;dy[r]=rays[r].direction.y;dz[r]=rays[r].direction.z;
        ts[r]=INFINITY;ids[r]=-1;
    }
    for(int i=0;i<scene.count;++i) {
        const Triangle& triangle=scene.triangles[i];
        for(int r=0;r<8;++r) {
            Float3 direction{dx[r],dy[r],dz[r]};
            Float3 h=cross(direction,triangle.e2);
            float determinant=dot(triangle.e1,h);
            float inverse=1/maximum(determinant,1e-10f);
            Float3 offset=Float3{ox[r],oy[r],oz[r]}-triangle.p;
            float u=dot(offset,h)*inverse;
            Float3 q=cross(offset,triangle.e1);
            float v=dot(direction,q)*inverse,t=dot(triangle.e2,q)*inverse;
            bool valid=(determinant>=1e-10f)&(u>=0)&(u<=1)&(v>=0)&(u+v<=1)&(t>1e-4f)&(t<ts[r]);
            ts[r]=valid?t:ts[r];us[r]=valid?u:us[r];vs[r]=valid?v:vs[r];ids[r]=valid?i:ids[r];
        }
    }
    for(int r=0;r<8;++r)hits[r]={ts[r],us[r],vs[r],ids[r]};
}
struct Scratch { float previous_pdf[8]{};uint64_t shadows=0; };
static bool visible(const SceneData& scene,Ray ray,float distance,int light_id,Scratch& scratch) {
    ++scratch.shadows;Hit hit;hit.t=distance;const Triangle& light=scene.triangles[light_id];
    for(int i=0;i<scene.count;++i) {
        const Triangle& t=scene.triangles[i];
        if(i==light_id || (dot(t.emission,t.emission)>0 && t.normal.x==light.normal.x
             && t.normal.y==light.normal.y && t.normal.z==light.normal.z
             && dot(light.normal,t.p-light.p)==0))continue;
        if(t.intersect(ray,hit))return false;
    }
    return true;
}
static float power_weight(float a,float b) {
    if(!(a>0))return 0;
    float ratio=b/a;return 1/(1+ratio*ratio);
}
static Float3 cosine_direction(const SceneData& scene,int id,Random& random) {
    float u=random.uniform(),phi=2*M_PI*random.uniform(),radius=sqrtf(u),sine,cosine;
#ifdef __GLIBC__
    __builtin_sincosf(phi,&sine,&cosine);
#else
    sine=sinf(phi);cosine=cosf(phi);
#endif
    return scene.bases[id].tangent*(radius*cosine)+scene.bases[id].bitangent*(radius*sine)
           +scene.triangles[id].normal*sqrtf(1-u);
}
// 0 uniform, 1 cosine, 2 NEE, 3 MIS. Compile-time mode removes unused work.
template<int Mode> static void scatter(const SceneData& scene,Path& path,Hit hit,int depth,int lane,Scratch& scratch) {
    if(hit.triangle<0){path.active=false;return;}
    const Triangle& triangle=scene.triangles[hit.triangle];
    if(dot(triangle.emission,triangle.emission)>0) {
        float weight=1;
        if constexpr(Mode==2)weight=depth==0?1:0;
        if constexpr(Mode==3)if(depth>0) {
            float cosine=maximum(0,dot(triangle.normal,path.ray.direction*-1));
            float distance2=hit.t*hit.t*dot(path.ray.direction,path.ray.direction);
            float pdf=cosine>0 && scene.light_area>0?distance2/(scene.light_area*cosine):INFINITY;
            weight=power_weight(scratch.previous_pdf[lane],pdf);
        }
        if constexpr(Mode==0)path.result=path.weight*triangle.emission;
        else path.result+=path.weight*triangle.emission*weight;
        path.active=false;return;
    }
    Float3 position=triangle.p+triangle.e1*hit.u+triangle.e2*hit.v;
    Float3 origin=position+triangle.normal*1e-4f;
    if constexpr(Mode>=2)if(depth+1<max_depth && scene.light_area>0) {
        float selector=path.random.uniform()*scene.light_area;
        int id=scene.lights[scene.light_count-1].triangle;
        for(int i=0;i<scene.light_count;++i)if(selector<scene.lights[i].cumulative_area){id=scene.lights[i].triangle;break;}
        const Triangle& light=scene.triangles[id];
        float root=sqrtf(path.random.uniform()),v=path.random.uniform();
        Float3 point=light.p+light.e1*(1-root)+light.e2*(root*v),delta=point-origin;
        float distance2=dot(delta,delta),distance=sqrtf(distance2);
        if(distance>0) {
            Float3 direction=delta*(1/distance);
            float cx=maximum(0,dot(triangle.normal,direction)),cl=maximum(0,dot(light.normal,direction*-1));
            if(cx>0 && cl>0 && visible(scene,{origin,direction},distance,id,scratch)) {
                float pdf=distance2/(scene.light_area*cl),weight=1;
                if constexpr(Mode==3)weight=power_weight(pdf,cx/float(M_PI));
                path.result+=path.weight*triangle.albedo*light.emission*(cx*weight/(float(M_PI)*pdf));
            }
        }
    }
    Float3 direction;
    if constexpr(Mode==0) {
        direction=sample_hemisphere(triangle.normal,path.random);
        path.weight*=triangle.albedo*(2*maximum(0,dot(triangle.normal,direction)));
    } else {
        direction=cosine_direction(scene,hit.triangle,path.random);
        if constexpr(Mode==3)scratch.previous_pdf[lane]=maximum(0,dot(triangle.normal,direction))/float(M_PI);
        path.weight*=triangle.albedo;
    }
    path.ray={origin,direction};
}
struct Options {
    int samples=128,width=320,threads=1,mode=2,cpus[1024],cpu_count=0;
    uint64_t seed=42,random_seed=Random::seed_key(42);
    const char* filename="build/plain.hdr";bool raw=false;
};
struct alignas(64) Statistics { uint64_t rays=0,slots=0,batches=0,shadows=0; };
template<int Mode> static void render_tile(const SceneData& scene,const Options& options,int x0,int y0,int x1,int y1,
                                         float* pixels,Float3* results,Scratch& scratch,Statistics& stats) {
    int tile_width=x1-x0,jobs=tile_width*(y1-y0)*options.samples,next_job=0,sample=0,x=x0,y=y0;
    Path paths[8];int job[8]{},depth[8]{};
    const Camera& c=scene.camera;float focal=1/tan(c.fovy*.5f*M_PI/180);
    auto refill=[&](int r) {
        Path p{};p.active=next_job<jobs;
        if(p.active) {
            job[r]=next_job++;depth[r]=0;
            p.random.seed(options.random_seed,((uint64_t(y)*options.width+x)<<32)|uint32_t(sample));
            Float2 uv=(Float2{float(x),float(y)}+Float2{p.random.uniform(),p.random.uniform()}*.5f)
                        *Float2{1.f/options.width,1.f/options.width};
            p.ray={c.position,normalize(c.u*(2*uv.x-1)*c.aspect+c.v*(-2*uv.y+1)+c.w*focal)};
            if(++sample==options.samples){sample=0;if(++x==x1){x=x0;++y;}}
        }
        paths[r]=p;
    };
    for(int r=0;r<8;++r)refill(r);
    for(;;) {
        Ray rays[8]{};Hit hits[8];int live=0;
        for(int r=0;r<8;++r)if(paths[r].active){rays[r]=paths[r].ray;++live;}
        if(!live)break;
        stats.rays+=live;stats.slots+=8;++stats.batches;
        intersect_batch(scene,rays,hits);
        for(int r=0;r<8;++r) {
            Path p=paths[r];if(!p.active)continue;
            scatter<Mode>(scene,p,hits[r],depth[r],r,scratch);
            if(++depth[r]==max_depth)p.active=false;
            if(!p.active){results[job[r]]=p.result;refill(r);}else paths[r]=p;
        }
    }
    for(int pixel=0;pixel<jobs/options.samples;++pixel) {
        Float3 color{};
        for(int s=0;s<options.samples;++s)color+=results[pixel*options.samples+s];
        color=color*(1.f/options.samples);
        int px=x0+pixel%tile_width,py=y0+pixel/tile_width;
        memcpy(pixels+3*(size_t(py)*options.width+px),&color,sizeof(color));
    }
}
static double now() { timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9; }
template<int Mode> static int render(const SceneData& scene,const Options& options,float* pixels,Statistics& total) {
    int columns=(options.width+15)/16,rows=(options.width+7)/8,next=0,failed=0;
    Statistics* stats=nullptr;
    if(posix_memalign((void**)&stats,64,sizeof(Statistics)*options.threads))return 1;
    for(int i=0;i<options.threads;++i)::new (stats+i) Statistics;
    omp_set_dynamic(0);
#pragma omp parallel num_threads(options.threads)
    {
        int id=omp_get_thread_num();
        if(omp_get_num_threads()!=options.threads)__atomic_store_n(&failed,1,__ATOMIC_RELAXED);
#ifdef __linux__
        if(options.cpu_count) {
            cpu_set_t set;CPU_ZERO(&set);CPU_SET(options.cpus[id],&set);
            if(sched_setaffinity(0,sizeof(set),&set))__atomic_store_n(&failed,1,__ATOMIC_RELAXED);
        }
#endif
        // Every completed job writes this slot before the ordered sum reads it.
        Float3* results=(Float3*)malloc(size_t(minimum(16,options.width))*minimum(8,options.width)*options.samples*sizeof(Float3));
        if(!results)__atomic_store_n(&failed,1,__ATOMIC_RELAXED);
        else {
            for(size_t i=0;i<size_t(minimum(16,options.width))*minimum(8,options.width)*options.samples;++i)
                ::new (results+i) Float3; // trivial default construction: no zero fill
            Scratch scratch;
            for(;;) {
                int tile=__atomic_fetch_add(&next,1,__ATOMIC_RELAXED);if(tile>=columns*rows)break;
                int x0=tile%columns*16,y0=tile/columns*8;
                render_tile<Mode>(scene,options,x0,y0,minimum(x0+16,options.width),minimum(y0+8,options.width),pixels,results,scratch,stats[id]);
            }
            stats[id].shadows=scratch.shadows;
        }
        free(results);
    }
    for(int i=0;i<options.threads;++i){total.rays+=stats[i].rays;total.slots+=stats[i].slots;total.batches+=stats[i].batches;total.shadows+=stats[i].shadows;}
    free(stats);return failed;
}
#ifndef PATHTRACER_PLAIN_NO_MAIN
int main(int argc,char** argv) {
    Options options;int positional=0;const char* error=nullptr;
    for(int i=1;i<argc && !error;++i) {
        const char* arg=argv[i];
        if(!strcmp(arg,"--raw")){options.raw=true;continue;}
        if(!strcmp(arg,"--refill"))continue;
        if(strncmp(arg,"--",2)) {
            if(positional==0)options.samples=atoi(arg);else if(positional==1)options.width=atoi(arg);
            else if(positional==2)options.filename=arg;else error="Too many arguments";
            ++positional;continue;
        }
        if(++i==argc){error="Missing option value";break;}const char* value=argv[i];
        if(!strcmp(arg,"--threads"))options.threads=atoi(value);
        else if(!strcmp(arg,"--seed"))options.seed=strtoull(value,nullptr,10);
        else if(!strcmp(arg,"--sampling")) {
            if(!strcmp(value,"uniform"))options.mode=0;else if(!strcmp(value,"cosine"))options.mode=1;
            else if(!strcmp(value,"nee"))options.mode=2;else if(!strcmp(value,"mis"))options.mode=3;
            else error="Sampling must be uniform, cosine, nee or mis";
        } else if(!strcmp(arg,"--cpus")) {
            char* end=nullptr;const char* cursor=value;
            do {
                long cpu=strtol(cursor,&end,10);
                if(end==cursor || cpu<0 || cpu>=1024 || options.cpu_count==1024){error="Invalid CPU list";break;}
                options.cpus[options.cpu_count++]=int(cpu);
                if(!*end)break;
                if(*end!=','){error="Invalid CPU list";break;}cursor=end+1;
            }while(true);
        } else if(!strcmp(arg,"--batch")){if(strcmp(value,"8"))error="Plain renderer uses batch8";}
        else if(!strcmp(arg,"--layout")){if(strcmp(value,"aos"))error="Plain renderer uses AoS";}
        else if(!strcmp(arg,"--kernel")){if(strcmp(value,"rays"))error="Plain renderer uses ray intersection";}
        else if(!strcmp(arg,"--runtime")){if(strcmp(value,"omp"))error="Plain renderer uses OpenMP";}
        else error="Unknown option";
    }
    if(options.samples<1 || options.samples>INT_MAX/128 || options.width<1 || options.width>32768
        || options.threads<1 || options.threads>1024 || (options.cpu_count && options.cpu_count!=options.threads))error="Invalid dimensions or worker/CPU count";
    if(error){fprintf(stderr,"%s\n",error);return 1;}
    options.random_seed=Random::seed_key(options.seed);
    SceneData scene=cornell_box();prepare(scene);
    size_t count=size_t(options.width)*options.width*3;
    float* pixels=(float*)calloc(count,sizeof(float));if(!pixels){perror("pixels");return 1;}
    Statistics stats;double start=now();int failed;
    switch(options.mode) {
    case 0:failed=render<0>(scene,options,pixels,stats);break;
    case 1:failed=render<1>(scene,options,pixels,stats);break;
    case 2:failed=render<2>(scene,options,pixels,stats);break;
    default:failed=render<3>(scene,options,pixels,stats);break;
    }
    double seconds=now()-start;
    if(failed){fprintf(stderr,"Allocation, affinity or OpenMP team failed\n");free(pixels);return 1;}
    int written;
    if(options.raw) {
        FILE* file=fopen(options.filename,"wb");written=file && fwrite(pixels,sizeof(float),count,file)==count;
        if(file && fclose(file))written=0;
    } else written=stbi_write_hdr(options.filename,options.width,options.width,3,pixels);
    free(pixels);if(!written){fprintf(stderr,"Cannot write image\n");return 1;}
    const char* modes[]={"uniform","cosine","nee","mis"};uint64_t paths=uint64_t(options.width)*options.width*options.samples;
    fprintf(stderr,"{\"rng\":\"%s\",\"seed\":%llu,\"sampling\":\"%s\",\"runtime\":\"omp\",\"threads\":%d,\"render_seconds\":%.9g,\"completed_paths\":%llu,\"rays\":%llu,\"ray_slots\":%llu,\"batch_calls\":%llu,\"shadow_rays\":%llu}\n",Random::name(),(unsigned long long)options.seed,modes[options.mode],options.threads,seconds,(unsigned long long)paths,(unsigned long long)stats.rays,(unsigned long long)stats.slots,(unsigned long long)stats.batches,(unsigned long long)stats.shadows);
    return 0;
}
#endif
