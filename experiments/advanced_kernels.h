#pragma once
#include "renderer_common.h"
#include <immintrin.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <vector>

// Base owns triangles and camera. Prepared arrays are read-only during tracing.
namespace aggressive {
using V = __m256;
inline V broadcast(float x) { return _mm256_set1_ps(x); }
inline V load(const float* p) { return _mm256_load_ps(p); }
inline V add(V a, V b) { return _mm256_add_ps(a, b); }
inline V sub(V a, V b) { return _mm256_sub_ps(a, b); }
inline V mul(V a, V b) { return _mm256_mul_ps(a, b); }
inline V mad(V a, V b, V c) { return _mm256_fmadd_ps(a, b, c); }
inline V msub(V a, V b, V c) { return _mm256_fmsub_ps(a, b, c); }
inline V both(V a, V b) { return _mm256_and_ps(a, b); }
inline V ge(V a, V b) { return _mm256_cmp_ps(a, b, _CMP_GE_OQ); }
inline V gt(V a, V b) { return _mm256_cmp_ps(a, b, _CMP_GT_OQ); }
inline V le(V a, V b) { return _mm256_cmp_ps(a, b, _CMP_LE_OQ); }
inline V lt(V a, V b) { return _mm256_cmp_ps(a, b, _CMP_LT_OQ); }
inline V absolute(V a) { return _mm256_andnot_ps(broadcast(-0.0f),a); }
inline V dot3(V ax, V ay, V az, V bx, V by, V bz) {
    return mad(az, bz, mad(ay, by, mul(ax, bx)));
}
struct Scratch {
    explicit Scratch(int = 0) {}
#ifdef COUNT_WORK
    uint64_t triangle_tests=0,box_tests=0,fallback_queries=0,vector_divides=0;
#endif
};
inline void work_triangles(Scratch& s,int count) {
#ifdef COUNT_WORK
    s.triangle_tests+=count;
#else
    (void)s;(void)count;
#endif
}
inline void work_packet(Scratch& s,const int* ids) {
#ifdef COUNT_WORK
    for(int j=0;j<8;++j)s.triangle_tests+=ids[j]>=0;
#else
    (void)s;(void)ids;
#endif
}
inline void work_boxes(Scratch& s,int count) {
#ifdef COUNT_WORK
    s.box_tests+=count;
#else
    (void)s;(void)count;
#endif
}
inline void work_divide(Scratch& s) {
#ifdef COUNT_WORK
    ++s.vector_divides;
#else
    (void)s;
#endif
}

struct Precise3 {
    double x,y,z;
    explicit Precise3(Float3 a):x(a.x),y(a.y),z(a.z){}
    Precise3(double x,double y,double z):x(x),y(y),z(z){}
    Precise3 operator-(Precise3 b)const{return{x-b.x,y-b.y,z-b.z};}
};
inline double precise_dot(Precise3 a,Precise3 b){return(a.x*b.x+a.y*b.y)+a.z*b.z;}
inline Precise3 precise_cross(Precise3 a,Precise3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float parallel_limit(const Triangle& t){auto n=precise_cross(Precise3(t.e1),Precise3(t.e2));return float(1e-6*(std::fabs(n.x)+std::fabs(n.y)+std::fabs(n.z)));}

// Rare ill-conditioned rays use double throughout, avoiding cancellation in
// determinant and edge tests. This is not a claim of fully watertight triangles.
template<class Base> bool precise_intersect(const Base& scene,Ray ray,Hit& hit,Scratch& scratch){
#ifdef COUNT_WORK
    ++scratch.fallback_queries;
#endif
    work_triangles(scratch,int(scene.triangles.size()));
    hit={};double nearest=INFINITY;Precise3 o(ray.origin),d(ray.direction);
    for(int i=0;i<int(scene.triangles.size());++i){const auto& p=scene.triangles[i];Precise3 a(p.e1),b(p.e2);
        auto h=precise_cross(d,b);double det=precise_dot(a,h);if(det<double(1e-10f))continue;
        auto s=o-Precise3(p.p);double u=precise_dot(s,h)/det;if(u<0||u>1)continue;
        auto q=precise_cross(s,a);double v=precise_dot(d,q)/det;if(v<0||u+v>1)continue;
        double t=precise_dot(b,q)/det;if(t<=double(1e-4f)||t>=nearest)continue;
        nearest=t;hit={float(t),float(u),float(v),i};
    }
    return hit.triangle>=0;
}

struct alignas(32) AffinePacket {
    float px[8]{}, py[8]{}, pz[8]{}, nx[8]{}, ny[8]{}, nz[8]{};
    float ux[8]{}, uy[8]{}, uz[8]{}, vx[8]{}, vy[8]{}, vz[8]{};
    float parallel[8]{};
    int id[8]{-1,-1,-1,-1,-1,-1,-1,-1};
};

template<class Base> struct AffineScene : Base {
    using Scratch = aggressive::Scratch;
    std::vector<AffinePacket> prepared;
    explicit AffineScene(Base data) : Base(std::move(data)) { prepare(); }

    void prepare() {
        prepared.assign((this->triangles.size() + 7) / 8, {});
        for (int i = 0; i < int(this->triangles.size()); ++i) {
            const Triangle& t = this->triangles[i];
            // Compute coefficients in double; reject truly degenerate triangles.
            double ax=t.e1.x, ay=t.e1.y, az=t.e1.z;
            double bx=t.e2.x, by=t.e2.y, bz=t.e2.z;
            double nx=ay*bz-az*by, ny=az*bx-ax*bz, nz=ax*by-ay*bx;
            double norm=nx*nx+ny*ny+nz*nz;
            if (norm == 0) continue;
            double ux=(by*nz-bz*ny)/norm, uy=(bz*nx-bx*nz)/norm, uz=(bx*ny-by*nx)/norm;
            double vx=(ny*az-nz*ay)/norm, vy=(nz*ax-nx*az)/norm, vz=(nx*ay-ny*ax)/norm;
            auto& p=prepared[i/8]; int j=i%8;
            p.px[j]=t.p.x; p.py[j]=t.p.y; p.pz[j]=t.p.z;
            p.nx[j]=float(nx); p.ny[j]=float(ny); p.nz[j]=float(nz);
            p.ux[j]=float(ux); p.uy[j]=float(uy); p.uz[j]=float(uz);
            p.vx[j]=float(vx); p.vy[j]=float(vy); p.vz[j]=float(vz);
            p.parallel[j]=parallel_limit(t);
            p.id[j]=i;
        }
    }

    bool intersect(Ray ray, Hit& hit, Scratch& scratch) const {
        hit={};
        V ox=broadcast(ray.origin.x), oy=broadcast(ray.origin.y), oz=broadcast(ray.origin.z);
        V dx=broadcast(ray.direction.x), dy=broadcast(ray.direction.y), dz=broadcast(ray.direction.z);
        V zero=broadcast(0), one=broadcast(1), eps=broadcast(1e-10f);
        V direction_norm=broadcast(std::fabs(ray.direction.x)+std::fabs(ray.direction.y)+std::fabs(ray.direction.z));
        for (const auto& p : prepared) {
            work_packet(scratch,p.id);
            V nx=load(p.nx), ny=load(p.ny), nz=load(p.nz);
            V det=sub(zero,dot3(nx,ny,nz,dx,dy,dz));
            if(_mm256_movemask_ps(lt(absolute(det),mul(load(p.parallel),direction_norm))))return precise_intersect(*this,ray,hit,scratch);
            V mask=ge(det,eps);
            if (!_mm256_movemask_ps(mask)) continue;
            V sx=sub(ox,load(p.px)),sy=sub(oy,load(p.py)),sz=sub(oz,load(p.pz));
            V numerator=dot3(nx,ny,nz,sx,sy,sz);
            work_divide(scratch);
            V t=_mm256_div_ps(numerator,_mm256_blendv_ps(one,det,mask));
            mask=both(mask,both(gt(t,broadcast(1e-4f)),lt(t,broadcast(hit.t))));
            if (!_mm256_movemask_ps(mask)) continue;
            V x=mad(dx,t,sx), y=mad(dy,t,sy), z=mad(dz,t,sz);
            V u=dot3(load(p.ux),load(p.uy),load(p.uz),x,y,z);
            V v=dot3(load(p.vx),load(p.vy),load(p.vz),x,y,z);
            mask=both(mask,both(ge(u,zero),ge(v,zero)));
            mask=both(mask,le(add(u,v),one));
            int valid=_mm256_movemask_ps(mask);
            if (!valid) continue;
            alignas(32) float ts[8], us[8], vs[8];
            _mm256_store_ps(ts,t); _mm256_store_ps(us,u); _mm256_store_ps(vs,v);
            while (valid) {
                int j=__builtin_ctz(unsigned(valid)); valid&=valid-1;
                if (p.id[j]>=0 && ts[j]<hit.t) hit={ts[j],us[j],vs[j],p.id[j]};
            }
        }
        return hit.triangle>=0;
    }
};

struct alignas(32) MollerPacket {
    float px[8]{},py[8]{},pz[8]{},ax[8]{},ay[8]{},az[8]{},bx[8]{},by[8]{},bz[8]{};
    float parallel[8]{};
    int id[8]{-1,-1,-1,-1,-1,-1,-1,-1};
};
inline void set_triangle(MollerPacket& p,int j,const Triangle& t,int id) {
    p.px[j]=t.p.x; p.py[j]=t.p.y; p.pz[j]=t.p.z;
    p.ax[j]=t.e1.x; p.ay[j]=t.e1.y; p.az[j]=t.e1.z;
    p.bx[j]=t.e2.x; p.by[j]=t.e2.y; p.bz[j]=t.e2.z; p.id[j]=id;
    p.parallel[j]=parallel_limit(t);
}

// Reject in determinant-scaled coordinates. Divide only packets with a hit.
inline bool intersect_packet(const MollerPacket& p, Ray ray, Hit& hit,Scratch& scratch) {
    work_packet(scratch,p.id);
    V dx=broadcast(ray.direction.x),dy=broadcast(ray.direction.y),dz=broadcast(ray.direction.z);
    V ax=load(p.ax),ay=load(p.ay),az=load(p.az),bx=load(p.bx),by=load(p.by),bz=load(p.bz);
    V hx=msub(dy,bz,mul(dz,by)),hy=msub(dz,bx,mul(dx,bz)),hz=msub(dx,by,mul(dy,bx));
    V det=dot3(ax,ay,az,hx,hy,hz),zero=broadcast(0);
    V direction_norm=broadcast(std::fabs(ray.direction.x)+std::fabs(ray.direction.y)+std::fabs(ray.direction.z));
    if(_mm256_movemask_ps(lt(absolute(det),mul(load(p.parallel),direction_norm))))return false;
    V mask=ge(det,broadcast(1e-10f));
    if (!_mm256_movemask_ps(mask)) return true;
    V sx=sub(broadcast(ray.origin.x),load(p.px));
    V sy=sub(broadcast(ray.origin.y),load(p.py));
    V sz=sub(broadcast(ray.origin.z),load(p.pz));
    V u=dot3(sx,sy,sz,hx,hy,hz);
    mask=both(mask,both(ge(u,zero),le(u,det)));
    if (!_mm256_movemask_ps(mask)) return true;
    V qx=msub(sy,az,mul(sz,ay)),qy=msub(sz,ax,mul(sx,az)),qz=msub(sx,ay,mul(sy,ax));
    V v=dot3(dx,dy,dz,qx,qy,qz);
    mask=both(mask,both(ge(v,zero),le(add(u,v),det)));
    if (!_mm256_movemask_ps(mask)) return true;
    V t=dot3(bx,by,bz,qx,qy,qz);
    mask=both(mask,both(gt(t,mul(broadcast(1e-4f),det)),le(t,mul(broadcast(hit.t),det))));
    int valid=_mm256_movemask_ps(mask);
    if (!valid) return true;
    work_divide(scratch);
    V inverse=_mm256_div_ps(broadcast(1),_mm256_blendv_ps(broadcast(1),det,mask));
    alignas(32) float ts[8],us[8],vs[8];
    _mm256_store_ps(ts,mul(t,inverse)); _mm256_store_ps(us,mul(u,inverse)); _mm256_store_ps(vs,mul(v,inverse));
    while(valid) {
        int j=__builtin_ctz(unsigned(valid));valid&=valid-1;int id=p.id[j];
        if(id>=0 && (ts[j]<hit.t || (ts[j]==hit.t && id<hit.triangle))) hit={ts[j],us[j],vs[j],id};
    }
    return true;
}

struct Bounds {
    Float3 lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
    void add(Float3 p) {
        lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
        hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
    }
    float area() const { Float3 d=hi-lo; return d.x*d.y+d.x*d.z+d.y*d.z; }
};
inline Bounds bounds(const Triangle& t) { Bounds b;b.add(t.p);b.add(t.p+t.e1);b.add(t.p+t.e2);return b; }
inline float component(Float3 v,int k) { return k==0?v.x:k==1?v.y:v.z; }

template<class Base> struct CulledScene : Base {
    using Scratch=aggressive::Scratch;
    explicit CulledScene(Base data) : Base(std::move(data)) { prepare(); }
    struct alignas(32) Group {
        float lx[8]{},ly[8]{},lz[8]{},hx[8]{},hy[8]{},hz[8]{};
        std::array<MollerPacket,8> packets;
        int count=0;
    };
    std::vector<Group> groups;
    std::vector<std::vector<int>> leaves;

    void split(std::vector<int> ids) {
        if(ids.size()<=8) {leaves.push_back(std::move(ids));return;}
        float best=INFINITY;int best_axis=0,best_pos=int(ids.size()/2);
        std::vector<int> order;
        for(int axis=0;axis<3;++axis) {
            auto sorted=ids;
            std::sort(sorted.begin(),sorted.end(),[&](int a,int b){
                auto centroid=[&](int i){const auto& t=this->triangles[i];return component(t.p+(t.e1+t.e2)*(1.0f/3),axis);};
                float ca=centroid(a),cb=centroid(b);return ca<cb || (ca==cb&&a<b);
            });
            std::vector<Bounds> prefix(sorted.size()),suffix(sorted.size());Bounds p,s;
            for(size_t i=0;i<sorted.size();++i){auto b=bounds(this->triangles[sorted[i]]);p.add(b.lo);p.add(b.hi);prefix[i]=p;}
            for(int i=int(sorted.size())-1;i>=0;--i){auto b=bounds(this->triangles[sorted[i]]);s.add(b.lo);s.add(b.hi);suffix[i]=s;}
            for(int i=1;i<int(sorted.size());++i){float cost=prefix[i-1].area()*i+suffix[i].area()*(sorted.size()-i);
                if(cost<best){best=cost;best_axis=axis;best_pos=i;order=sorted;}}
        }
        (void)best_axis;
        split(std::vector<int>(order.begin(),order.begin()+best_pos));
        split(std::vector<int>(order.begin()+best_pos,order.end()));
    }
    void prepare() {
        leaves.clear();groups.clear();if(this->triangles.empty())return;
        std::vector<int> ids(this->triangles.size());std::iota(ids.begin(),ids.end(),0);split(std::move(ids));
        groups.resize((leaves.size()+7)/8);
        for(int i=0;i<int(leaves.size());++i){auto& g=groups[i/8];int j=i%8;g.count=j+1;Bounds b;
            for(int k=0;k<int(leaves[i].size());++k){int id=leaves[i][k];const auto& t=this->triangles[id];
                set_triangle(g.packets[j],k,t,id);auto tb=bounds(t);b.add(tb.lo);b.add(tb.hi);}
            // Conservative enlargement for the renderer's finite scene coordinates.
            float scale=std::max({1.0f,std::fabs(b.lo.x),std::fabs(b.lo.y),std::fabs(b.lo.z),std::fabs(b.hi.x),std::fabs(b.hi.y),std::fabs(b.hi.z)});
            float pad=scale*2e-6f;
            g.lx[j]=b.lo.x-pad;g.ly[j]=b.lo.y-pad;g.lz[j]=b.lo.z-pad;
            g.hx[j]=b.hi.x+pad;g.hy[j]=b.hi.y+pad;g.hz[j]=b.hi.z+pad;
        }
    }
    bool intersect(Ray ray,Hit& hit,Scratch& scratch) const {
        hit={};
        for(const auto& g:groups){V near=broadcast(1e-4f),far=broadcast(INFINITY),mask=ge(near,broadcast(0));
            work_boxes(scratch,g.count);
            auto slab=[&](float origin,float direction,const float* lo,const float* hi){
                V o=broadcast(origin),l=load(lo),h=load(hi);
                if(direction==0){mask=both(mask,both(ge(o,l),le(o,h)));return;}
                V inverse=broadcast(1/direction),a=mul(sub(l,o),inverse),b=mul(sub(h,o),inverse);
                near=_mm256_max_ps(near,_mm256_min_ps(a,b));far=_mm256_min_ps(far,_mm256_max_ps(a,b));
            };
            slab(ray.origin.x,ray.direction.x,g.lx,g.hx);slab(ray.origin.y,ray.direction.y,g.ly,g.hy);slab(ray.origin.z,ray.direction.z,g.lz,g.hz);
            int valid=_mm256_movemask_ps(both(mask,le(near,far)))&((1<<g.count)-1);
            while(valid){int j=__builtin_ctz(unsigned(valid));valid&=valid-1;if(!intersect_packet(g.packets[j],ray,hit,scratch))return precise_intersect(*this,ray,hit,scratch);}
        }
        return hit.triangle>=0;
    }
};

template<class Base> struct Ray8Scene : Base {
    using Scratch=aggressive::Scratch;
    std::vector<float> parallel;
    explicit Ray8Scene(Base data) : Base(std::move(data)) { prepare(); }
    void prepare() {parallel.resize(this->triangles.size());for(size_t i=0;i<parallel.size();++i)parallel[i]=parallel_limit(this->triangles[i]);}
    void intersect_batch(const Ray* rays,Hit* hits,int count,Scratch& scratch) const {
        for(int first=0;first<count;first+=8){int size=std::min(8,count-first);
            alignas(32) float oxs[8]{},oys[8]{},ozs[8]{},dxs[8]{},dys[8]{},dzs[8]{};
            for(int j=0;j<size;++j){const auto& r=rays[first+j];oxs[j]=r.origin.x;oys[j]=r.origin.y;ozs[j]=r.origin.z;dxs[j]=r.direction.x;dys[j]=r.direction.y;dzs[j]=r.direction.z;}
            int active_count=0;
#ifdef COUNT_WORK
            for(int j=0;j<size;++j)active_count+=(dxs[j]!=0||dys[j]!=0||dzs[j]!=0);
#endif
            V ox=load(oxs),oy=load(oys),oz=load(ozs),dx=load(dxs),dy=load(dys),dz=load(dzs);
            V nearest=broadcast(INFINITY),bu=broadcast(0),bv=broadcast(0),zero=broadcast(0),one=broadcast(1);
            V direction_norm=add(add(absolute(dx),absolute(dy)),absolute(dz)),unstable=zero;
            __m256i ids=_mm256_set1_epi32(-1);
            for(int i=0;i<int(this->triangles.size());++i){const auto& p=this->triangles[i];
                work_triangles(scratch,active_count);
                V ax=broadcast(p.e1.x),ay=broadcast(p.e1.y),az=broadcast(p.e1.z),bx=broadcast(p.e2.x),by=broadcast(p.e2.y),bz=broadcast(p.e2.z);
                V hx=msub(dy,bz,mul(dz,by)),hy=msub(dz,bx,mul(dx,bz)),hz=msub(dx,by,mul(dy,bx));
                V det=dot3(ax,ay,az,hx,hy,hz),mask=ge(det,broadcast(1e-10f));
                unstable=_mm256_or_ps(unstable,lt(absolute(det),mul(broadcast(parallel[i]),direction_norm)));
                // Start the divide before independent numerator arithmetic.
                work_divide(scratch);
                V inverse=_mm256_div_ps(one,_mm256_blendv_ps(one,det,mask));
                V sx=sub(ox,broadcast(p.p.x)),sy=sub(oy,broadcast(p.p.y)),sz=sub(oz,broadcast(p.p.z));
                V u=dot3(sx,sy,sz,hx,hy,hz);mask=both(mask,both(ge(u,zero),le(u,det)));
                V qx=msub(sy,az,mul(sz,ay)),qy=msub(sz,ax,mul(sx,az)),qz=msub(sx,ay,mul(sy,ax));
                V v=dot3(dx,dy,dz,qx,qy,qz);mask=both(mask,both(ge(v,zero),le(add(u,v),det)));
                V t=dot3(bx,by,bz,qx,qy,qz);mask=both(mask,both(gt(t,mul(broadcast(1e-4f),det)),lt(t,mul(nearest,det))));
                t=mul(t,inverse);mask=both(mask,lt(t,nearest));
                nearest=_mm256_blendv_ps(nearest,t,mask);bu=_mm256_blendv_ps(bu,mul(u,inverse),mask);bv=_mm256_blendv_ps(bv,mul(v,inverse),mask);
                ids=_mm256_blendv_epi8(ids,_mm256_set1_epi32(i),_mm256_castps_si256(mask));
            }
            alignas(32) float ts[8],us[8],vs[8];alignas(32) int is[8];
            _mm256_store_ps(ts,nearest);_mm256_store_ps(us,bu);_mm256_store_ps(vs,bv);_mm256_store_si256(reinterpret_cast<__m256i*>(is),ids);
            for(int j=0;j<size;++j)hits[first+j]={ts[j],us[j],vs[j],is[j]};
            int fallback=_mm256_movemask_ps(unstable)&((1<<size)-1);
            while(fallback){int j=__builtin_ctz(unsigned(fallback));fallback&=fallback-1;precise_intersect(*this,rays[first+j],hits[first+j],scratch);}
        }
    }
    bool intersect(Ray ray,Hit& hit,Scratch& scratch) const {intersect_batch(&ray,&hit,1,scratch);return hit.triangle>=0;}
};

// Ray-parallel variant of the precomputed transform: one triangle serves eight rays.
template<class Base> struct Ray8AffineScene : AffineScene<Base> {
    using Scratch=aggressive::Scratch;
    explicit Ray8AffineScene(Base data) : AffineScene<Base>(std::move(data)) {}
    void intersect_batch(const Ray* rays,Hit* hits,int count,Scratch& scratch) const {
        for(int first=0;first<count;first+=8){int size=std::min(8,count-first);
            alignas(32) float oxs[8]{},oys[8]{},ozs[8]{},dxs[8]{},dys[8]{},dzs[8]{};
            for(int j=0;j<size;++j){const auto& r=rays[first+j];oxs[j]=r.origin.x;oys[j]=r.origin.y;ozs[j]=r.origin.z;dxs[j]=r.direction.x;dys[j]=r.direction.y;dzs[j]=r.direction.z;}
            int active_count=0;
#ifdef COUNT_WORK
            for(int j=0;j<size;++j)active_count+=(dxs[j]!=0||dys[j]!=0||dzs[j]!=0);
#endif
            V ox=load(oxs),oy=load(oys),oz=load(ozs),dx=load(dxs),dy=load(dys),dz=load(dzs);
            V nearest=broadcast(INFINITY),bu=broadcast(0),bv=broadcast(0),zero=broadcast(0),one=broadcast(1);
            V direction_norm=add(add(absolute(dx),absolute(dy)),absolute(dz)),unstable=zero;
            __m256i ids=_mm256_set1_epi32(-1);
            for(const auto& p:this->prepared) for(int j=0;j<8;++j) {
                if(p.id[j]<0)continue;
                work_triangles(scratch,active_count);
                V nx=broadcast(p.nx[j]),ny=broadcast(p.ny[j]),nz=broadcast(p.nz[j]);
                V det=sub(zero,dot3(nx,ny,nz,dx,dy,dz)),mask=ge(det,broadcast(1e-10f));
                unstable=_mm256_or_ps(unstable,lt(absolute(det),mul(broadcast(p.parallel[j]),direction_norm)));
                if(!_mm256_movemask_ps(mask))continue;
                V sx=sub(ox,broadcast(p.px[j])),sy=sub(oy,broadcast(p.py[j])),sz=sub(oz,broadcast(p.pz[j]));
                V numerator=dot3(nx,ny,nz,sx,sy,sz);
                mask=both(mask,both(gt(numerator,mul(broadcast(1e-4f),det)),lt(numerator,mul(nearest,det))));
                if(!_mm256_movemask_ps(mask))continue;
                work_divide(scratch);
                V t=_mm256_div_ps(numerator,_mm256_blendv_ps(one,det,mask));
                V x=mad(dx,t,sx),y=mad(dy,t,sy),z=mad(dz,t,sz);
                V u=dot3(broadcast(p.ux[j]),broadcast(p.uy[j]),broadcast(p.uz[j]),x,y,z);
                V v=dot3(broadcast(p.vx[j]),broadcast(p.vy[j]),broadcast(p.vz[j]),x,y,z);
                mask=both(mask,both(ge(u,zero),ge(v,zero)));
                mask=both(mask,both(le(add(u,v),one),lt(t,nearest)));
                nearest=_mm256_blendv_ps(nearest,t,mask);bu=_mm256_blendv_ps(bu,u,mask);bv=_mm256_blendv_ps(bv,v,mask);
                ids=_mm256_blendv_epi8(ids,_mm256_set1_epi32(p.id[j]),_mm256_castps_si256(mask));
            }
            alignas(32) float ts[8],us[8],vs[8];alignas(32) int is[8];
            _mm256_store_ps(ts,nearest);_mm256_store_ps(us,bu);_mm256_store_ps(vs,bv);_mm256_store_si256(reinterpret_cast<__m256i*>(is),ids);
            for(int j=0;j<size;++j)hits[first+j]={ts[j],us[j],vs[j],is[j]};
            int fallback=_mm256_movemask_ps(unstable)&((1<<size)-1);
            while(fallback){int j=__builtin_ctz(unsigned(fallback));fallback&=fallback-1;precise_intersect(*this,rays[first+j],hits[first+j],scratch);}
        }
    }
    bool intersect(Ray ray,Hit& hit,Scratch& scratch) const {intersect_batch(&ray,&hit,1,scratch);return hit.triangle>=0;}
};
} // namespace aggressive
