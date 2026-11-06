// Analytic tests exercise the exact sampler used by the OptiX ray-generation program.
#define CLOUD_ATMOSPHERE_TEST
#include "../experiments/cloud_viewer.cpp"
#include <cstdio>
#include <cstdlib>

void require(bool value,const char* message) {
    if (!value) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
float maximum(float3 p) { return fmaxf(p.x,fmaxf(p.y,p.z)); }
float error(float3 a,float3 b) { return maximum({fabsf(a.x-b.x),fabsf(a.y-b.y),fabsf(a.z-b.z)}); }
struct Box {
    float majorant=2;
    float3 sun{1,0,0};
    float3 sigma{1,1,1};
    float albedo=0, background=1, sunlight=0;
    float exit(float3 p,float3 d) const {
        float t=1e20f;
        for (int k=0;k<3;++k) {
            float o=k==0 ? p.x : k==1 ? p.y : p.z;
            float v=k==0 ? d.x : k==1 ? d.y : d.z;
            if (fabsf(v)>1e-8f) t=fminf(t,((v>0 ? .5f : -.5f)-o)/v);
        }
        return fmaxf(t,0);
    }
    transport::Material material(float3) const {
        return {sigma,{sigma*albedo,{0,0,0},{0,0,0}}};
    }
    float cloudExtinction(float3) const { return sigma.y; }
    float shadow(float3 p,float3 d,transport::Rng& rng) const {
        return transport::ratioTracking(*this,p,d,exit(p,d),majorant,rng);
    }
    float3 sunRadiance(float3) const { return {sunlight,sunlight,sunlight}; }
    float3 environment(float3,float3,bool) const { return {background,background,background}; }
};
float3 estimate(const Box& box,int paths,int depth=0) {
    double x=0,y=0,z=0;
    for (int i=0;i<paths;++i) {
        transport::Rng rng(unsigned(i)*0x9e3779b9u+73u);
        auto v=transport::trace(box,{-0.5f,0,0},{1,0,0},rng,depth);
        require(std::isfinite(v.x+v.y+v.z) && v.x>=0 && v.y>=0 && v.z>=0,"invalid path radiance");
        x+=v.x; y+=v.y; z+=v.z;
    }
    return {float(x/paths),float(y/paths),float(z/paths)};
}
int main() {
    constexpr int n=300000;
    transport::Rng rng(51), same(51);
    double freeFlight=0;
    for (int i=0;i<n;++i) {
        float u=rng.uniform();require(u==same.uniform(),"seed is not repeatable");
        require(u>0 && u<1,"random sample is not in the open unit interval");
        freeFlight+=-log1pf(-u)/2;
    }
    require(fabs(freeFlight/n-.5)<.004,"exponential mean free path is incorrect");
    for(int kind=0;kind<3;++kind) {
        double mean=0,second=0;
        for (int i=0;i<n;++i) {
            auto d=transport::samplePhase({0,0,1},kind,rng);
            require(fabsf(dot(d,d)-1)<1e-5f,"phase direction is not normalized");
            mean+=d.z;second+=d.z*d.z;
        }
        float g=kind==0 ? .35f : .8f;
        require(fabs(mean/n-(kind==1 ? 0 : g))<.006,"phase first moment disagrees with analytic distribution");
        require(fabs(second/n-(kind==1 ? .4f : (1+2*g*g)/3))<.006,"phase second moment disagrees with analytic distribution");
    }
    struct Ramp { float cloudExtinction(float3 p) const { return .2f+.8f*(p.x+.5f); } } ramp;
    for (float bound : {1.f,3.f}) {
        double mean=0;
        for (int i=0;i<n;++i) mean+=transport::ratioTracking(ramp,{-.5f,0,0},{1,0,0},1,bound,rng);
        require(fabs(mean/n-expf(-.6f))<.004,"heterogeneous ratio tracking disagrees with analytic optical depth");
    }
    Box box;box.sigma={.3f,.7f,1.2f};
    auto value=estimate(box,n);
    auto expected=air::expMinus(box.sigma);
    require(error(value,expected)<.006,"colored absorption disagrees with Beer-Lambert law");
    box.majorant=5;
    require(error(estimate(box,n),expected)<.006,"changing majorant changes expected transport");
    box.sigma={0,0,0};
    require(error(estimate(box,1000),{1,1,1})==0,"vacuum does not preserve environment radiance");
    box.sigma={1,1,1};box.albedo=.8f;box.background=0;box.sunlight=1;box.majorant=2;
    expected={.8f*expf(-1)*air::phase(1,.35f),0,0};expected.y=expected.z=expected.x;
    value=estimate(box,n,1);
    require(error(value,expected)<.003,"single scattering through homogeneous slab disagrees with analytic integral");
    auto multiple=estimate(box,n);
    require(multiple.x>value.x+.01f,"multiple scattering does not add indirect illumination");
    box.sunlight=0;box.background=1;box.albedo=1;
    auto equilibrium=estimate(box,n);
    require(error(equilibrium,{1,1,1})<.01,"conservative scattering fails constant-environment equilibrium");
    std::printf("Transport passed: RNG/free paths, phase moments, heterogeneous ratio tracking, RGB absorption, majorant invariance, vacuum, analytic single scattering, multiple scattering and conservative equilibrium.\n");
    std::printf("Slab single %.6f (analytic %.6f), multiple %.6f, equilibrium %.6f\n",value.x,expected.x,multiple.x,equilibrium.x);
}
