#include "../advanced_kernels.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

struct D3 {
    double x,y,z;
    explicit D3(Float3 a):x(a.x),y(a.y),z(a.z){}
    D3(double x,double y,double z):x(x),y(y),z(z){}
    D3 operator-(D3 b)const{return{x-b.x,y-b.y,z-b.z};}
};
double ddot(D3 a,D3 b){return (a.x*b.x+a.y*b.y)+a.z*b.z;}
D3 dcross(D3 a,D3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
struct DHit {double t=INFINITY,u=0,v=0;int triangle=-1;};
DHit reference(const SceneData& scene,Ray ray){DHit hit;D3 o(ray.origin),d(ray.direction);
    for(int i=0;i<int(scene.triangles.size());++i){const auto& tri=scene.triangles[i];D3 a(tri.e1),b(tri.e2);
        D3 h=dcross(d,b);double det=ddot(a,h);if(det<double(1e-10f))continue;
        D3 s=o-D3(tri.p);double u=ddot(s,h)/det;if(u<0||u>1)continue;
        D3 q=dcross(s,a);double v=ddot(d,q)/det;if(v<0||u+v>1)continue;
        double t=ddot(b,q)/det;if(t<=double(1e-4f)||t>=hit.t)continue;hit={t,u,v,i};}
    return hit;
}

std::vector<Ray> corpus(const SceneData& scene){std::mt19937_64 rng(20261013);
    std::uniform_real_distribution<float> unit(0,1),pos(-100,700),signed_unit(-1,1);
    std::vector<Ray> rays;rays.reserve(262144);
    for(int i=0;i<262144;++i){Float3 o,d;
        if(i%4==0){o={pos(rng),pos(rng),pos(rng)};d={signed_unit(rng),signed_unit(rng),signed_unit(rng)};}
        else if(i%4==1){o=scene.camera.position;d=Float3{555*unit(rng),555*unit(rng),555*unit(rng)}-o;}
        else if(i%4==2){const auto& t=scene.triangles[i%scene.triangles.size()];float u=unit(rng),v=unit(rng)*(1-u);
            o=t.p+t.e1*u+t.e2*v+t.normal*1e-3f;d={signed_unit(rng),signed_unit(rng),signed_unit(rng)};if(dot(d,t.normal)<0)d=d*-1;}
        else{o={555*unit(rng),555*unit(rng),555*unit(rng)};d={signed_unit(rng),signed_unit(rng),signed_unit(rng)};}
        rays.push_back({o,normalize(d)});
    }
    return rays;
}

template<class S> void trace(const S& scene,const std::vector<Ray>& rays,std::vector<Hit>& hits){typename S::Scratch scratch(scene.triangles.size());
    for(size_t i=0;i<rays.size();++i)scene.intersect(rays[i],hits[i],scratch);
}
template<class B> void trace(const aggressive::Ray8Scene<B>& scene,const std::vector<Ray>& rays,std::vector<Hit>& hits){typename aggressive::Ray8Scene<B>::Scratch scratch(scene.triangles.size());
    scene.intersect_batch(rays.data(),hits.data(),int(rays.size()),scratch);
}
template<class B> void trace(const aggressive::Ray8AffineScene<B>& scene,const std::vector<Ray>& rays,std::vector<Hit>& hits){typename aggressive::Ray8AffineScene<B>::Scratch scratch(scene.triangles.size());
    scene.intersect_batch(rays.data(),hits.data(),int(rays.size()),scratch);
}

template<class S> void evaluate(const char* name,const S& scene,const std::vector<Ray>& rays,const std::vector<DHit>& refs,int repeats){
    std::vector<Hit> hits(rays.size());trace(scene,rays,hits);int id_mismatch=0,geometric_mismatch=0;double max_t=0,max_uv=0;
    for(size_t i=0;i<rays.size();++i){auto h=hits[i];auto r=refs[i];if(h.triangle!=r.triangle){++id_mismatch;
            bool same_surface=h.triangle>=0&&r.triangle>=0&&std::fabs(h.t-r.t)<2e-3;
            bool boundary=r.triangle>=0&&(std::min({r.u,r.v,1-r.u-r.v})<2e-5||std::fabs(r.t-1e-4)<2e-4);
            if(!same_surface&&!boundary){if(geometric_mismatch<4)std::cerr<<name<<" mismatch "<<i<<" actual "<<h.triangle<<" t="<<h.t<<" reference "<<r.triangle<<" t="<<r.t<<"\n";++geometric_mismatch;}
        }else if(h.triangle>=0){max_t=std::max(max_t,std::fabs(h.t-r.t)/std::max(1.0,std::fabs(r.t)));max_uv=std::max({max_uv,std::fabs(h.u-r.u),std::fabs(h.v-r.v)});}}
    std::cout<<name<<" rays="<<rays.size()<<" id_mismatch="<<id_mismatch<<" nonboundary_mismatch="<<geometric_mismatch<<" max_relative_t="<<max_t<<" max_bary_abs="<<max_uv;
    for(int n=0;n<repeats;++n){auto begin=std::chrono::steady_clock::now();trace(scene,rays,hits);auto end=std::chrono::steady_clock::now();
        double sum=0;for(auto h:hits)if(h.triangle>=0)sum+=h.t+h.u+h.v+h.triangle;
        std::cout<<" seconds="<<std::chrono::duration<double>(end-begin).count()<<" checksum="<<sum;}
    std::cout<<'\n';
}

#include <cassert>

template<class S> void validate(const char* name) {
    SceneData full=cornell_box();auto all=corpus(full);int checked=0,mismatches=0,unexplained=0;
    for(int count:{0,1,7,8,9,17,36}) {
        SceneData data=full;data.triangles.resize(count,data.triangles.front());S scene(data);
        std::vector<Ray> rays(all.begin(),all.begin()+8193);
        for(const auto& t:data.triangles) for(Float3 q:{t.p,t.p+t.e1,t.p+t.e2,t.p+t.e1*0.5f,t.p+(t.e1+t.e2)*0.5f,t.p+(t.e1+t.e2)*(1.0f/3)}) {
            rays.push_back({q+t.normal*10,t.normal*-1});
            rays.push_back({q-t.normal*10,t.normal});
            rays.push_back({q+t.normal*10,normalize(t.e1)});
        }
        std::vector<Hit> hits(rays.size());trace(scene,rays,hits);
        for(size_t j=0;j<rays.size();++j){++checked;auto r=reference(data,rays[j]);auto h=hits[j];
            if(r.triangle==h.triangle) {
                if(h.triangle>=0 && (!std::isfinite(h.t)||!std::isfinite(h.u)||!std::isfinite(h.v)||std::fabs(h.t-r.t)>1e-3*std::max(1.0,std::fabs(r.t)))) {
                    if(unexplained<8)std::cerr<<name<<" distance count="<<count<<" ray="<<j<<" id="<<h.triangle<<" actual="<<h.t<<" ref="<<r.t<<" u="<<h.u<<" v="<<h.v<<"\n";
                    ++unexplained;
                }
                continue;
            }
            ++mismatches;
            bool boundary=false;
            if(r.triangle>=0)boundary=std::min({r.u,r.v,1-r.u-r.v})<2e-5||std::fabs(r.t-1e-4)<2e-4;
            if(h.triangle>=0)boundary=boundary||std::min({h.u,h.v,1-h.u-h.v})<2e-5f||std::fabs(h.t-1e-4f)<2e-4f;
            if(r.triangle>=0&&h.triangle>=0&&std::fabs(r.t-h.t)<2e-3)boundary=true;
            if(!boundary){if(unexplained<4)std::cerr<<name<<" unexplained count="<<count<<" ray="<<j<<" got="<<h.triangle<<" ref="<<r.triangle<<'\n';++unexplained;}
        }
    }
    // Exact simple fixtures: ties, packet tails, backfaces, parallel rays, epsilon.
    for(int n:{1,7,8,9,17}) {
        SceneData data;data.triangles.assign(n,Triangle{{0,0,0},{1,0,0},{0,1,0},{1,1,1}});S scene(data);typename S::Scratch scratch(n);Hit h;
        for(Float3 p: {Float3{.25f,.25f,1},Float3{0,0,1},Float3{.5f,.5f,1}}) {
            assert(scene.intersect({p,{0,0,-1}},h,scratch));assert(h.triangle==0);++checked;
        }
        for(Ray r:{Ray{{.25f,.25f,-1},{0,0,1}},Ray{{.25f,.25f,1},{1,0,0}},Ray{{.25f,.25f,1e-4f},{0,0,-1}},Ray{{0,0,1},{0,0,0}}}) {
            assert(!scene.intersect(r,h,scratch));++checked;
        }
    }
    std::cout<<name<<" checked="<<checked<<" boundary_id_differences="<<mismatches<<" unexplained="<<unexplained<<'\n';
    if(unexplained)std::exit(1);
}
template<class S> void check_corpus(const char* name,const SceneData& data,const std::vector<Ray>& rays,const std::vector<DHit>& refs) {
    S scene(data);std::vector<Hit> hits(rays.size());trace(scene,rays,hits);
    for(size_t i=0;i<rays.size();++i) {
        const auto& h=hits[i];const auto& r=refs[i];
        assert(h.triangle==r.triangle);
        if(h.triangle<0)continue;
        assert(std::isfinite(h.t)&&std::isfinite(h.u)&&std::isfinite(h.v));
        assert(std::fabs(h.t-r.t)<=1e-4*std::max(1.0,std::fabs(r.t)));
        assert(std::fabs(h.u-r.u)<=2e-5&&std::fabs(h.v-r.v)<=2e-5);
    }
    std::cout<<name<<" fixed_rays="<<rays.size()<<" double_reference=pass\n";
}
int main() {
    SceneData data=cornell_box();auto rays=corpus(data);std::vector<DHit> refs;
    for(auto ray:rays)refs.push_back(reference(data,ray));
    check_corpus<aggressive::AffineScene<SceneData>>("affine",data,rays,refs);
    check_corpus<aggressive::CulledScene<SceneData>>("culled",data,rays,refs);
    check_corpus<aggressive::Ray8Scene<SceneData>>("ray8",data,rays,refs);
    check_corpus<aggressive::Ray8AffineScene<SceneData>>("ray8_affine",data,rays,refs);
    validate<aggressive::AffineScene<SceneData>>("affine");
    validate<aggressive::CulledScene<SceneData>>("culled");
    validate<aggressive::Ray8Scene<SceneData>>("ray8");
    validate<aggressive::Ray8AffineScene<SceneData>>("ray8_affine");
}
