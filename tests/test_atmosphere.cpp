// Check transport against analytic columns, geometry, and converged integration.
#define CLOUD_ATMOSPHERE_TEST
#include "../experiments/cloud_viewer.cpp"
#include <cstdio>
#include <cstdlib>

void require(bool condition,const char* message) {
    if (!condition) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
float error(float3 a,float3 b) {
    return fmaxf(fabsf(a.x-b.x),fmaxf(fabsf(a.y-b.y),fabsf(a.z-b.z)));
}
int main() {
    using namespace air;
    float3 up{0,1,0}, ground{0,Radius,0};
    auto measured=opticalDepth(ground,up,100,1,512);
    auto analytic=Rayleigh*(8*(1-expf(-100.f/8)))+MieExtinction*(1.2f*(1-expf(-100.f/1.2f)))+Ozone*15;
    require(error(measured,analytic)<0.0003f,"vertical optical depth disagrees with analytic atmosphere column");
    auto reverse=opticalDepth(ground+up*100,up*-1,100,1,2048);
    require(error(measured,reverse)<0.0004f,"optical depth changes when path is reversed");
    float angle=-2*Pi/180;
    float3 sun{cosf(angle),sinf(angle),0};
    require(error(sunTransmittance({0,Radius+.1f,0},sun,1,128),{0,0,0})==0,"Earth does not shadow low observer");
    require(sunTransmittance({0,Radius+12,0},sun,1,128).x>0,"elevated observer should see sun above curved horizon");
    auto sunset=sunTransmittance({0,Radius+2,0},{1,0,0},1,128);
    require(sunset.x>20*sunset.z,"long atmospheric path does not redden sunlight");
    float3 p{0,Radius+2,0}, direction=normalize(float3{-1,.1f,0});
    sun=normalize(float3{-1,-.01f,0});
    auto coarse=trace(p,direction,1e20f,sun,1,32,16,false);
    auto fine=trace(p,direction,1e20f,sun,1,128,64,false);
    auto reference=trace(p,direction,1e20f,sun,1,256,128,false);
    require(error(fine.radiance,reference.radiance)<error(coarse.radiance,reference.radiance),"integration does not converge");
    require(error(fine.radiance,reference.radiance)<0.001f,"high-quality radiance differs from reference");
    for(float elevation:{-.08f,0.f,.3f,1.f}) for(float haze:{0.f,1.f,6.f}) {
        sun={cosf(elevation),sinf(elevation),0};
        auto v=trace(p,direction,1e20f,sun,haze,64,32);
        require(std::isfinite(v.radiance.x+v.radiance.y+v.radiance.z),"nonfinite sky radiance");
        require(v.radiance.x>=0 && v.radiance.y>=0 && v.radiance.z>=0,"negative sky radiance");
        require(v.transmittance.x>=0 && v.transmittance.x<=1 && v.transmittance.y>=0 && v.transmittance.y<=1 && v.transmittance.z>=0 && v.transmittance.z<=1,"invalid transmittance");
    }
    auto full=trace(p,direction,10,sun,1,256,64,false);
    auto front=trace(p,direction,4,sun,1,128,64,false);
    auto back=trace(p+direction*4,direction,6,sun,1,128,64,false);
    require(error(full.radiance,front.radiance+front.transmittance*back.radiance)<0.0002f,"segmented aerial perspective does not compose");
    sun=normalize(float3{1,1,0});
    auto disc=trace(p,sun,1e20f,sun,1,64,32,true);
    auto noDisc=trace(p,sun,1e20f,sun,1,64,32,false);
    require(disc.radiance.x>1000*noDisc.radiance.x,"solar disc missing");
    auto empty=trace(p,direction,0,sun,1,64,32,false);
    require(error(empty.radiance,{0,0,0})==0 && error(empty.transmittance,{1,1,1})==0,"zero-length air segment is not identity");
    std::puts("Atmosphere: analytic column, reciprocity, Earth shadow, sunset color, convergence, finite radiance, composition, and sun disc passed.");
}
