// One C++ source: OptiX rendering, raylib display. Build: make cloud-viewer
#include <cuda_runtime.h>
#include <optix.h>
#include <nanovdb/NanoVDB.h>
#include <cmath>

// Distances are kilometres. RGB atmosphere; selectable single/multiple scattering.
#ifdef CLOUD_DEVICE
#define HD __host__ __device__
#else
#define HD
#endif
HD inline float3 operator+(float3 a, float3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
HD inline float3 operator-(float3 a, float3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
HD inline float3 operator*(float3 a, float b) { return {a.x*b,a.y*b,a.z*b}; }
HD inline float3 operator*(float3 a, float3 b) { return {a.x*b.x,a.y*b.y,a.z*b.z}; }
HD inline float dot(float3 a, float3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
HD inline float3 normalize(float3 a) { return a*(1/sqrtf(dot(a,a))); }
HD inline float3 cross(float3 a, float3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }

namespace air {
constexpr float Pi=3.14159265359f, Radius=6360, Top=6460, SunRadius=0.004675f;
constexpr int SkySamples=32, SkyResolution=32;
constexpr float3 Solar {24,24,24}, Rayleigh {0.005802f,0.013558f,0.0331f};
constexpr float3 MieScatter {0.003996f,0.003996f,0.003996f};
constexpr float3 MieExtinction {0.00444f,0.00444f,0.00444f}, Ozone {0.00065f,0.001881f,0.000085f};
struct Segment { float3 radiance {0,0,0}, transmittance {1,1,1}; };
HD inline float clamp(float x,float a,float b) { return fminf(b,fmaxf(a,x)); }
HD inline float3 expMinus(float3 a) { return {expf(-a.x),expf(-a.y),expf(-a.z)}; }
HD inline float phase(float mu,float g) { return (1-g*g)/(4*Pi*powf(1+g*g-2*g*mu,1.5f)); }
HD inline float rayleighPhase(float mu) { return 3*(1+mu*mu)/(16*Pi); }
HD inline float3 profile(float3 p,float haze) {
    float h=fmaxf(0,sqrtf(dot(p,p))-Radius);
    return {expf(-h/8),haze*expf(-h/1.2f),clamp(fminf((h-10)/15,(40-h)/15),0,1)};
}
HD inline float3 extinction(float3 density) {
    return Rayleigh*density.x+MieExtinction*density.y+Ozone*density.z;
}
HD inline float boundary(float3 p,float3 direction,float radius,bool near) {
    float b=dot(p,direction), c=dot(p,p)-radius*radius;
    float discriminant=b*b-c;
    if (discriminant<0) return -1;
    return -b+(near ? -1 : 1)*sqrtf(discriminant);
}
HD inline float3 opticalDepth(float3 p,float3 direction,float distance,float haze,int count) {
    float3 result {0,0,0};
    for (int i=0;i<count;++i) {
        float a=distance*(float(i)/count)*(float(i)/count);
        float b=distance*(float(i+1)/count)*(float(i+1)/count);
        result=result+extinction(profile(p+direction*((a+b)*0.5f),haze))*(b-a);
    }
    return result;
}
HD inline float3 sunTransmittance(float3 p,float3 sun,float haze,int count) {
    float radius=sqrtf(dot(p,p));
    float mu=dot(p,sun)/radius;
    float horizon=-sqrtf(fmaxf(0,1-(Radius/radius)*(Radius/radius)));
    float visible=clamp((mu-horizon+SunRadius)/(2*SunRadius),0,1);
    visible=visible*visible*(3-2*visible);
    if (visible<=0) return {0,0,0};
    // Approximate the visible solar-disc fraction near the curved horizon.
    if (mu<horizon+1e-5f) {
        float3 up=p*(1/radius), tangent=normalize(sun-up*mu);
        mu=horizon+1e-5f;
        sun=up*mu+tangent*sqrtf(fmaxf(0,1-mu*mu));
    }
    return expMinus(opticalDepth(p,sun,boundary(p,sun,Top,false),haze,count))*visible;
}
HD inline float weight(float sigma,float distance) {
    return sigma>1e-8f ? -expm1f(-sigma*distance)/sigma : distance;
}
HD inline float3 integrate(float3 source,float3 sigma,float distance) {
    return {source.x*weight(sigma.x,distance),source.y*weight(sigma.y,distance),source.z*weight(sigma.z,distance)};
}
HD inline float3 scatterSource(float3 density,float mu) {
    return Rayleigh*(density.x*rayleighPhase(mu))+MieScatter*(density.y*phase(mu,0.8f));
}
HD inline Segment trace(float3 p,float3 direction,float distance,float3 sun,float haze,int count,int sunCount,bool disc=true) {
    Segment result;
    float top=boundary(p,direction,Top,false);
    float ground=boundary(p,direction,Radius,true);
    bool hitGround=ground>0 && ground<top && ground<=distance;
    distance=fminf(distance,hitGround ? ground : top);
    float mu=dot(direction,sun);
    for (int i=0;i<count;++i) {
        float a=distance*(float(i)/count)*(float(i)/count);
        float b=distance*(float(i+1)/count)*(float(i+1)/count);
        float3 q=p+direction*((a+b)*0.5f), density=profile(q,haze), sigma=extinction(density);
        float3 source=Solar*sunTransmittance(q,sun,haze,sunCount)*scatterSource(density,mu);
        result.radiance=result.radiance+result.transmittance*integrate(source,sigma,b-a);
        result.transmittance=result.transmittance*expMinus(sigma*(b-a));
    }
    if (hitGround) {
        float3 q=p+direction*distance;
        float cosine=fmaxf(0,dot(normalize(q),sun));
        result.radiance=result.radiance+result.transmittance*Solar*sunTransmittance(q,sun,haze,sunCount)*(0.12f*cosine/Pi);
    } else if (disc && distance>=top-0.001f && mu>cosf(SunRadius)) {
        result.radiance=result.radiance+result.transmittance*Solar*(1/(Pi*SunRadius*SunRadius));
    }
    return result;
}
HD inline float3 skyDirection(int i) {
    float y=1-2*(i+0.5f)/SkySamples, r=sqrtf(fmaxf(0,1-y*y)), angle=i*2.39996323f;
    return {r*cosf(angle),y,r*sinf(angle)};
}
} // namespace air

// Shared by the interactive viewer and the independent PBRT scene exporter.
namespace cloud {
struct Preset {
    float yaw=0, pitch=-0.25f, distance=3.2f;
    float sunYaw=2.75f, sunPitch=0.55f, extinction=4, exposure=-1, altitude=3, haze=1;
    float albedo=0.95f;
    HD float3 camera() const {
        return {distance*cosf(pitch)*cosf(yaw),distance*sinf(pitch),distance*cosf(pitch)*sinf(yaw)};
    }
    HD float3 sun() const { return {cosf(sunPitch)*cosf(sunYaw),sinf(sunPitch),cosf(sunPitch)*sinf(sunYaw)}; }
};
inline Preset preset(int i) {
    Preset p;
    if (i==1) { p.pitch=-0.6f; p.sunYaw=air::Pi; p.sunPitch=0.034907f; p.altitude=2; p.haze=0.15f; }
    if (i==2) { p.pitch=-0.025f; p.sunYaw=air::Pi; p.sunPitch=0.034907f; p.altitude=2; p.haze=1; p.exposure=0; p.extinction=16; }
    return p;
}
} // namespace cloud

// Sampling code is also compiled on the CPU for analytic transport tests.
namespace transport {
struct Rng {
    unsigned state;
    HD explicit Rng(unsigned seed) : state(seed) {}
    HD float uniform() {
        state=state*747796405u+2891336453u;
        unsigned word=((state>>((state>>28u)+4u))^state)*277803737u;
        return (((word>>22u)^word)>>9u)*0x1p-23f+0x1p-24f;
    }
    HD float distance(float majorant) { return -log1pf(-uniform())/majorant; }
};
HD inline float3 around(float3 direction,float mu,float phi) {
    float3 axis=fabsf(direction.y)<0.99f ? float3{0,1,0} : float3{1,0,0};
    float3 right=normalize(cross(direction,axis)), up=cross(right,direction);
    float r=sqrtf(fmaxf(0,1-mu*mu));
    return direction*mu+right*(r*cosf(phi))+up*(r*sinf(phi));
}
HD inline float3 samplePhase(float3 direction,int kind,Rng& rng) {
    float mu, g=kind==0 ? 0.35f : 0.8f;
    if (kind==1) { // Rayleigh: p(mu) = 3(1+mu^2)/8.
        do { mu=2*rng.uniform()-1; } while (rng.uniform()>0.5f*(1+mu*mu));
    } else {
        float ratio=(1-g*g)/(1-g+2*g*rng.uniform());
        mu=air::clamp((1+g*g-ratio*ratio)/(2*g),-1,1);
    }
    return around(direction,mu,2*air::Pi*rng.uniform());
}
HD inline float phase(float mu,int kind) {
    return kind==1 ? air::rayleighPhase(mu) : air::phase(mu,kind==0 ? 0.35f : 0.8f);
}
struct Material { float3 extinction, scatter[3]; }; // Cloud, Rayleigh, Mie.

#ifdef CLOUD_DEVICE
#define PATH __device__
#else
#define PATH
#endif
// Unbiased cloud transmittance estimator, shared with the analytic CPU checks.
template<class Density>
PATH float ratioTracking(const Density& density,float3 p,float3 direction,float limit,float bound,Rng& rng) {
    float distance=0, transmission=1;
    while (bound>0 && (distance+=rng.distance(bound))<limit)
        transmission*=fmaxf(0,1-density.cloudExtinction(p+direction*distance)/bound);
    return transmission;
}
// Green-channel event probabilities; RGB likelihood weights retain colored extinction.
// A null event changes throughput, but does not count as a scattering bounce.
template<class Scene>
PATH float3 trace(const Scene& scene,float3 origin,float3 direction,Rng& rng,int maxDepth=0) {
    float3 radiance{0,0,0}, beta{1,1,1};
    int depth=0;
    for (;;) {
        float distance=0, exit=scene.exit(origin,direction);
        for (;;) {
            distance+=rng.distance(scene.majorant);
            if (distance>=exit) {
                float3 p=origin+direction*exit;
                return radiance+beta*scene.environment(p,direction,depth==0);
            }
            float3 p=origin+direction*distance;
            auto material=scene.material(p);
            float event=rng.uniform()*scene.majorant;
            if (event>=material.extinction.y) {
                float denominator=scene.majorant-material.extinction.y;
                beta=beta*float3{(scene.majorant-material.extinction.x)/denominator,1,
                                 (scene.majorant-material.extinction.z)/denominator};
                continue;
            }
            if (maxDepth && depth>=maxDepth) return radiance;
            int kind=0;
            while (kind<3 && event>=material.scatter[kind].y) {
                event-=material.scatter[kind].y;
                ++kind;
            }
            if (kind==3) return radiance; // Absorption.
            beta=beta*(material.scatter[kind]*(1/material.scatter[kind].y));
            radiance=radiance+beta*scene.sunRadiance(p)*scene.shadow(p,scene.sun,rng)
                              *phase(dot(direction,scene.sun),kind);
            ++depth;
            // No fixed cutoff in reference mode; roulette preserves the expectation.
            if (depth>=4) {
                float survive=air::clamp(fmaxf(beta.x,fmaxf(beta.y,beta.z)),0.05f,0.95f);
                if (rng.uniform()>=survive) return radiance;
                beta=beta*(1/survive);
            }
            direction=samplePhase(direction,kind,rng);
            origin=p;
            break;
        }
    }
}
#undef PATH
} // namespace transport

struct Parameters {
    OptixTraversableHandle volume;
    const nanovdb::FloatGrid* grid;
    uchar4* pixels;
    float4* accumulation;
    float3 center, halfBounds, cameraPosition, sunDirection, cloudTint, skyTint;
    float scale, extinction, exposure, cloudAltitude, haze, maxDensity, albedo;
    float3 skyLight[air::SkySamples];
    float* skyVisibility;
    float4* skyVolume;
    int precompute, reference, maxDepth;
    unsigned sampleIndex, seed;
    int steps, shadowSteps, atmosphereSteps, sunSteps;
};

#ifdef CLOUD_DEVICE

extern "C" { __constant__ Parameters params; }

__device__ float2 intersect_bounds(float3 origin, float3 direction) {
    float enter = 0, exit = 1e20f;

    for (int axis = 0; axis < 3; ++axis) {
        float o = axis == 0 ? origin.x : axis == 1 ? origin.y : origin.z;
        float d = axis == 0 ? direction.x : axis == 1 ? direction.y : direction.z;
        float h = axis == 0 ? params.halfBounds.x : axis == 1 ? params.halfBounds.y : params.halfBounds.z;

        if (fabsf(d) < 1e-8f) {
            if (fabsf(o) > h) return {1, 0};
        } else {
            float a = (-h-o)/d, b = (h-o)/d;
            enter = fmaxf(enter, fminf(a, b));
            exit = fminf(exit, fmaxf(a, b));
        }
    }

    return {enter, exit};
}

__device__ float density(float3 p) {
    float3 world = params.center + p * params.scale;
    auto q = params.grid->worldToIndexF(nanovdb::Vec3f(world.x, world.y, world.z));
    auto accessor = params.grid->getAccessor();
    int x = int(floorf(q[0])), y = int(floorf(q[1])), z = int(floorf(q[2]));
    float fx = q[0]-x, fy = q[1]-y, fz = q[2]-z;
    float value = 0;

    for (int k = 0; k < 2; ++k)
        for (int j = 0; j < 2; ++j)
            for (int i = 0; i < 2; ++i)
                value += accessor.getValue(nanovdb::Coord(x+i, y+j, z+k))
                       * (i ? fx : 1-fx) * (j ? fy : 1-fy) * (k ? fz : 1-fz);

    return fmaxf(0, value);
}

__device__ float sunlight(float3 p, float3 sun) {
    float2 interval = intersect_bounds(p, sun);
    float step = fmaxf(0, interval.y-interval.x) / params.shadowSteps;
    float opticalDepth = 0;

    for (int i = 0; i < params.shadowSteps; ++i)
        opticalDepth += density(p + sun * (interval.x + (i+0.5f)*step)) * params.extinction * step;

    return expf(-opticalDepth);
}

extern "C" __global__ void __intersection__volume() {
    float2 interval = intersect_bounds(optixGetObjectRayOrigin(), optixGetObjectRayDirection());
    if (interval.y > interval.x)
        optixReportIntersection(interval.x, 0, __float_as_uint(interval.y));
}

extern "C" __global__ void __closesthit__volume() {
    optixSetPayload_0(__float_as_uint(optixGetRayTmax()));
    optixSetPayload_1(optixGetAttribute_0());
}

extern "C" __global__ void __miss__volume() {
    optixSetPayload_0(__float_as_uint(1.0f));
    optixSetPayload_1(0);
}

__device__ unsigned char toByte(float value) {
    value = fmaxf(0, value * exp2f(params.exposure));
    value = value <= 0.0031308f ? 12.92f*value : 1.055f*powf(value, 1/2.4f)-0.055f;
    return static_cast<unsigned char>(roundf(255*fminf(1, value)));
}

__device__ float3 atmospherePosition(float3 local) {
    return local+float3{0,air::Radius+params.cloudAltitude,0};
}

struct ReferenceScene {
    float majorant;
    float3 sun;
    __device__ float exit(float3 p,float3 direction) const { return intersect_bounds(p,direction).y; }
    __device__ float cloudExtinction(float3 p) const { return density(p)*params.extinction; }
    __device__ transport::Material material(float3 p) const {
        float cloud=cloudExtinction(p);
        float3 d=air::profile(atmospherePosition(p),params.haze);
        return {air::extinction(d)+float3{cloud,cloud,cloud},
                {{params.albedo*cloud,params.albedo*cloud,params.albedo*cloud},air::Rayleigh*d.x,air::MieScatter*d.y}};
    }
    __device__ float shadow(float3 p,float3 direction,transport::Rng& rng) const {
        // Ratio tracking for cloud extinction. Air is integrated separately to the sun.
        return transport::ratioTracking(*this,p,direction,exit(p,direction),
                                        params.maxDensity*params.extinction,rng);
    }
    __device__ float3 sunRadiance(float3 p) const {
        return air::Solar*air::sunTransmittance(atmospherePosition(p),sun,params.haze,params.sunSteps);
    }
    __device__ float3 environment(float3 p,float3 direction,bool camera) const {
        // Explicit sun evaluation above handles scattered sunlight; avoid counting its disc twice.
        return air::trace(atmospherePosition(p),direction,1e20f,sun,params.haze,
                          params.atmosphereSteps,params.sunSteps,camera).radiance;
    }
};

// Cache skylight visibility through the cloud, then integrate its changing sky colors.
__device__ float3 skyAmbient(float3 p) {
    const int n=air::SkyResolution;
    float3 q {(p.x/params.halfBounds.x+1)*0.5f*(n-1),
              (p.y/params.halfBounds.y+1)*0.5f*(n-1),
              (p.z/params.halfBounds.z+1)*0.5f*(n-1)};
    q={air::clamp(q.x,0,n-1),air::clamp(q.y,0,n-1),air::clamp(q.z,0,n-1)};
    int x=int(air::clamp(floorf(q.x),0,n-2)), y=int(air::clamp(floorf(q.y),0,n-2)), z=int(air::clamp(floorf(q.z),0,n-2));
    float3 f {q.x-x,q.y-y,q.z-z}, result {0,0,0};
    for (int k=0;k<2;++k) for (int j=0;j<2;++j) for (int i=0;i<2;++i) {
        float4 v=params.skyVolume[(x+i)+n*((y+j)+n*(z+k))];
        result=result+float3{v.x,v.y,v.z}*((i ? f.x : 1-f.x)*(j ? f.y : 1-f.y)*(k ? f.z : 1-f.z));
    }
    return result;
}

extern "C" __global__ void __raygen__render() {
    uint3 pixel=optixGetLaunchIndex(), size=optixGetLaunchDimensions();
    if (params.precompute) {
        const int n=air::SkyResolution, cells=n*n*n;
        int z=pixel.z%n, index=pixel.x+n*(pixel.y+n*z);
        if (params.precompute==1) {
            float3 p {(2.0f*pixel.x/(n-1)-1)*params.halfBounds.x,
                      (2.0f*pixel.y/(n-1)-1)*params.halfBounds.y,
                      (2.0f*z/(n-1)-1)*params.halfBounds.z};
            int direction=pixel.z/n;
            params.skyVisibility[index+cells*direction]=sunlight(p,air::skyDirection(direction));
        } else {
            float3 light {0,0,0};
            for (int i=0;i<air::SkySamples;++i)
                light=light+params.skyLight[i]*(params.skyVisibility[index+cells*i]/air::SkySamples);
            params.skyVolume[index]=make_float4(light.x,light.y,light.z,1);
        }
        return;
    }
    unsigned index=pixel.y*size.x+pixel.x;
    transport::Rng rng(params.seed^(index*0x9e3779b9u)^(params.sampleIndex*0x85ebca6bu));
    float dx=params.reference ? rng.uniform() : 0.5f, dy=params.reference ? rng.uniform() : 0.5f;
    float x=(2*(pixel.x+dx)/size.x-1)*float(size.x)/size.y*0.414213562f;
    float y=(1-2*(pixel.y+dy)/size.y)*0.414213562f;
    float3 origin=params.cameraPosition, forward=normalize(origin*-1);
    float3 right=normalize(cross(forward,float3{0,1,0})), up=cross(right,forward);
    float3 direction=normalize(forward+right*x+up*y), sun=normalize(params.sunDirection);
    float3 world=atmospherePosition(origin);
    unsigned enter=__float_as_uint(1.0f), exit=0;
    optixTrace(params.volume,origin,direction,0.0f,1e20f,0.0f,255,
               OPTIX_RAY_FLAG_DISABLE_ANYHIT,0,1,0,enter,exit);
    float t0=__uint_as_float(enter), t1=__uint_as_float(exit);
    float3 color;
    if (t1<=t0) {
        color=air::trace(world,direction,1e20f,sun,params.haze,params.atmosphereSteps,params.sunSteps).radiance*params.skyTint;
    } else if (params.reference) {
        auto front=air::trace(world,direction,t0,sun,params.haze,16,params.sunSteps,false);
        // Global majorant bounds cloud plus all three RGB atmospheric coefficients.
        float airBound=air::Rayleigh.z+air::MieExtinction.x*params.haze+air::Ozone.y;
        ReferenceScene scene{(params.maxDensity*params.extinction+airBound)*1.0001f,sun};
        color=front.radiance+front.transmittance*transport::trace(scene,origin+direction*t0,direction,rng,params.maxDepth);
    } else {
        auto front=air::trace(world,direction,t0,sun,params.haze,16,params.sunSteps,false);
        auto back=air::trace(atmospherePosition(origin+direction*t1),direction,1e20f,sun,
                             params.haze,params.atmosphereSteps,params.sunSteps);
        float3 radiance {0,0,0}, transmission {1,1,1};
        float step=(t1-t0)/params.steps, mu=dot(direction,sun), phase=air::phase(mu,0.35f);
        for (int i=0;i<params.steps;++i) {
            float3 p=origin+direction*(t0+(i+0.5f)*step), q=atmospherePosition(p);
            float sigmaCloud=density(p)*params.extinction;
            float3 airDensity=air::profile(q,params.haze), sigma=air::extinction(airDensity)+float3{sigmaCloud,sigmaCloud,sigmaCloud};
            float3 sunLight=air::Solar*air::sunTransmittance(q,sun,params.haze,params.sunSteps);
            float shadow=sigmaCloud>1e-5f ? sunlight(p,sun) : 1;
            float3 source=sunLight*air::scatterSource(airDensity,mu)*shadow*params.skyTint
                         +(sunLight*(phase*shadow)+skyAmbient(p))*(params.albedo*sigmaCloud)*params.cloudTint;
            radiance=radiance+transmission*air::integrate(source,sigma,step);
            transmission=transmission*air::expMinus(sigma*step);
            if (fmaxf(transmission.x,fmaxf(transmission.y,transmission.z))<1e-5f) break;
        }
        color=front.radiance*params.skyTint+front.transmittance*(radiance+transmission*back.radiance*params.skyTint);
    }
    float4 previous=params.sampleIndex ? params.accumulation[index] : make_float4(0,0,0,0);
    float3 mean=params.sampleIndex ? float3{previous.x,previous.y,previous.z} : color;
    if (params.sampleIndex) mean=mean+(color-mean)*(1.0f/(params.sampleIndex+1));
    params.accumulation[index]=make_float4(mean.x,mean.y,mean.z,1);
    params.pixels[index]=make_uchar4(toByte(mean.x),toByte(mean.y),toByte(mean.z),255);
}

#elif !defined(CLOUD_ATMOSPHERE_TEST)
#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <nanovdb/util/IO.h>
#include <raylib.h>
#include <imgui.h>
#include <rlImGui.h>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

void check(cudaError_t error) {
    if (error != cudaSuccess) throw std::runtime_error(cudaGetErrorString(error));
}
void check(OptixResult error) {
    if (error != OPTIX_SUCCESS) throw std::runtime_error(optixGetErrorName(error));
}

struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) Record { char header[OPTIX_SBT_RECORD_HEADER_SIZE]; };
struct Renderer {
    OptixDeviceContext context;
    OptixModule module;
    OptixProgramGroup groups[3];
    OptixPipeline pipeline;
    OptixShaderBindingTable table {};
    Record* records;
    void* acceleration;
    Parameters* deviceParameters;
};

Renderer makeRenderer(Parameters& params, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read " + filename);
    std::string ptx {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    check(cudaFree(nullptr));
    check(optixInit());
    Renderer r {};
    check(optixDeviceContextCreate(nullptr, nullptr, &r.context));

    OptixAabb bounds {-params.halfBounds.x, -params.halfBounds.y, -params.halfBounds.z,
                      params.halfBounds.x,  params.halfBounds.y,  params.halfBounds.z};
    OptixAabb* deviceBounds;
    check(cudaMalloc(&deviceBounds, sizeof(bounds)));
    check(cudaMemcpy(deviceBounds, &bounds, sizeof(bounds), cudaMemcpyHostToDevice));
    CUdeviceptr boundsBuffer = reinterpret_cast<CUdeviceptr>(deviceBounds);
    unsigned flags = OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT;
    OptixBuildInput input {};
    input.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
    input.customPrimitiveArray.aabbBuffers = &boundsBuffer;
    input.customPrimitiveArray.numPrimitives = 1;
    input.customPrimitiveArray.flags = &flags;
    input.customPrimitiveArray.numSbtRecords = 1;
    OptixAccelBuildOptions buildOptions {};
    buildOptions.operation = OPTIX_BUILD_OPERATION_BUILD;
    OptixAccelBufferSizes sizes {};
    check(optixAccelComputeMemoryUsage(r.context, &buildOptions, &input, 1, &sizes));
    void* scratch;
    check(cudaMalloc(&scratch, sizes.tempSizeInBytes));
    check(cudaMalloc(&r.acceleration, sizes.outputSizeInBytes));
    check(optixAccelBuild(r.context, nullptr, &buildOptions, &input, 1,
        reinterpret_cast<CUdeviceptr>(scratch), sizes.tempSizeInBytes,
        reinterpret_cast<CUdeviceptr>(r.acceleration), sizes.outputSizeInBytes,
        &params.volume, nullptr, 0));
    check(cudaDeviceSynchronize());
    check(cudaFree(scratch));
    check(cudaFree(deviceBounds));

    OptixModuleCompileOptions moduleOptions {};
    OptixPipelineCompileOptions compileOptions {};
    compileOptions.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS;
    compileOptions.numPayloadValues = 2;
    compileOptions.numAttributeValues = 1;
    compileOptions.pipelineLaunchParamsVariableName = "params";
    compileOptions.usesPrimitiveTypeFlags = OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM;
    check(optixModuleCreate(r.context, &moduleOptions, &compileOptions,
                            ptx.data(), ptx.size(), nullptr, nullptr, &r.module));
    OptixProgramGroupDesc descriptions[3] {};
    descriptions[0].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    descriptions[0].raygen.module = r.module;
    descriptions[0].raygen.entryFunctionName = "__raygen__render";
    descriptions[1].kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
    descriptions[1].miss.module = r.module;
    descriptions[1].miss.entryFunctionName = "__miss__volume";
    descriptions[2].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    descriptions[2].hitgroup.moduleIS = r.module;
    descriptions[2].hitgroup.entryFunctionNameIS = "__intersection__volume";
    descriptions[2].hitgroup.moduleCH = r.module;
    descriptions[2].hitgroup.entryFunctionNameCH = "__closesthit__volume";
    OptixProgramGroupOptions groupOptions {};
    check(optixProgramGroupCreate(r.context, descriptions, 3, &groupOptions, nullptr, nullptr, r.groups));
    OptixPipelineLinkOptions linkOptions {};
    linkOptions.maxTraceDepth = 1;
    check(optixPipelineCreate(r.context, &compileOptions, &linkOptions, r.groups, 3, nullptr, nullptr, &r.pipeline));

    Record records[3] {};
    for (int i = 0; i < 3; ++i) check(optixSbtRecordPackHeader(r.groups[i], &records[i]));
    check(cudaMalloc(&r.records, sizeof(records)));
    check(cudaMemcpy(r.records, records, sizeof(records), cudaMemcpyHostToDevice));
    r.table.raygenRecord = reinterpret_cast<CUdeviceptr>(r.records);
    r.table.missRecordBase = reinterpret_cast<CUdeviceptr>(r.records+1);
    r.table.missRecordCount = 1;
    r.table.missRecordStrideInBytes = sizeof(Record);
    r.table.hitgroupRecordBase = reinterpret_cast<CUdeviceptr>(r.records+2);
    r.table.hitgroupRecordCount = 1;
    r.table.hitgroupRecordStrideInBytes = sizeof(Record);
    check(cudaMalloc(&r.deviceParameters, sizeof(Parameters)));
    std::cout << "OptiX renderer ready (cloud bounds GAS + NanoVDB volume transport)\n";
    return r;
}

void saveScreenshot() {
    Image image = LoadImageFromScreen();
    bool saved = ExportImage(image, "build/cloud-viewer.png");
    UnloadImage(image);
    if (!saved) throw std::runtime_error("Cannot save build/cloud-viewer.png");
}

void saveRender(const std::string& filename,int width,int height,const Parameters& params,
                const std::vector<uchar4>& pixels) {
    Image image{const_cast<uchar4*>(pixels.data()),width,height,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    if (!ExportImage(image,filename.c_str())) throw std::runtime_error("Cannot save "+filename);
    std::vector<float4> mean(pixels.size());
    check(cudaMemcpy(mean.data(),params.accumulation,mean.size()*sizeof(float4),cudaMemcpyDeviceToHost));
    std::ofstream linear(filename+".f32",std::ios::binary);
    for (auto value : mean) { float rgb[]={value.x,value.y,value.z}; linear.write(reinterpret_cast<const char*>(rgb),sizeof(rgb)); }
    if (!linear) throw std::runtime_error("Cannot save linear RGB "+filename+".f32");
}

int main(int argc, char** argv) try {
    const char* filename = "../volume-assets/wdas-cloud/wdas_cloud_quarter.nvdb";
    bool smoke = false, haveFilename = false;
    int initialPreset=0, targetSamples=0, fixedWidth=0, maxDepth=0;
    unsigned seed=1;
    bool reference=false, neutral=false, customExposure=false;
    float initialExposure=-1, initialAlbedo=-1, initialDensity=-1;
    std::string output="build/cloud-render.png";
    const char* usage=" [cloud.nvdb] [--mode march|reference] [--preset day|warm|cool] [--neutral]\n"
                      "  [--samples N --width N --output image.png] [--exposure EV] [--bounces N] [--seed N]\n"
                      "  [--albedo 0..1] [--density 0.05..40] [--smoke]\n";
    auto integer=[](const std::string& value,int minimum,int maximum) {
        size_t end=0; int result=std::stoi(value,&end);
        if (end!=value.size() || result<minimum || result>maximum) throw std::runtime_error("Invalid numeric argument: "+value);
        return result;
    };
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << usage;
            return 0;
        } else if (arg=="--mode" && i+1<argc) {
            std::string mode=argv[++i];
            if (mode!="march" && mode!="reference") throw std::runtime_error("Mode must be march or reference");
            reference=mode=="reference";
        } else if (arg=="--samples" && i+1<argc) targetSamples=integer(argv[++i],1,10000000);
        else if (arg=="--width" && i+1<argc) fixedWidth=integer(argv[++i],16,4096);
        else if (arg=="--bounces" && i+1<argc) maxDepth=integer(argv[++i],0,1024);
        else if (arg=="--seed" && i+1<argc) seed=unsigned(integer(argv[++i],0,2147483647));
        else if (arg=="--exposure" && i+1<argc) {
            std::string value=argv[++i]; size_t end=0;
            initialExposure=std::stof(value,&end);
            if (end!=value.size() || !std::isfinite(initialExposure) || initialExposure < -6 || initialExposure > 4)
                throw std::runtime_error("Exposure must be finite and between -6 and 4");
            customExposure=true;
        } else if ((arg=="--albedo" || arg=="--density") && i+1<argc) {
            std::string value=argv[++i]; size_t end=0;
            float v=std::stof(value,&end), lo=arg=="--albedo" ? 0 : 0.05f, hi=arg=="--albedo" ? 1 : 40;
            if (end!=value.size() || !std::isfinite(v) || v<lo || v>hi)
                throw std::runtime_error("Invalid "+arg+": "+value);
            (arg=="--albedo" ? initialAlbedo : initialDensity)=v;
        } else if (arg=="--output" && i+1<argc) output=argv[++i];
        else if (arg=="--neutral") neutral=true;
        else if (arg=="--preset" && i+1<argc) {
            std::string name=argv[++i];
            if (name!="day" && name!="warm" && name!="cool") throw std::runtime_error("Preset must be day, warm, or cool");
            initialPreset=name=="day" ? 0 : name=="warm" ? 1 : 2;
        } else if (arg == "--smoke") smoke = true;
        else if (!haveFilename && arg.rfind("--", 0) != 0) { filename = argv[i]; haveFilename = true; }
        else throw std::runtime_error(std::string("Usage: cloud_viewer")+usage);
    }

    constexpr size_t maxCloudBytes=100000000;
    if (std::filesystem::file_size(filename)>maxCloudBytes)
        throw std::runtime_error("Cloud file exceeds 100 MB; use wdas_cloud_quarter.nvdb or a smaller grid");
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    constexpr int panelWidth=640;
    InitWindow(panelWidth+960, 720, "WDAS Cloud Viewer - OptiX");
    if (!IsWindowReady()) throw std::runtime_error("Cannot open viewer window");
    SetWindowMinSize(panelWidth+640, 600);
    SetTargetFPS(60);
    BeginDrawing(); ClearBackground(BLACK); DrawText("Loading WDAS cloud...", 24, 24, 24, WHITE); EndDrawing();

    auto handle = nanovdb::io::readGrid(filename, std::string("density"));
    if (handle.size()>maxCloudBytes) throw std::runtime_error("Decoded cloud grid exceeds 100 MB");
    const auto* grid = handle.grid<float>();
    if (!grid || !grid->isFogVolume() || grid->activeVoxelCount() == 0 || grid->tree().background() != 0)
        throw std::runtime_error("Expected a nonempty, zero-background float fog grid named density");
    if (!grid->hasMinMax() || !std::isfinite(grid->tree().root().maximum()) || grid->tree().root().maximum()<=0)
        throw std::runtime_error("Volume transport requires finite positive NanoVDB min/max statistics");
    auto bounds = grid->worldBBox();
    auto extent = bounds.max()-bounds.min();
    auto center = (bounds.max()+bounds.min())*0.5;
    Parameters params {};
    params.maxDensity=grid->tree().root().maximum()*1.0001f;
    params.seed=seed;
    params.maxDepth=maxDepth;
    std::cout << "Density majorant: " << params.maxDensity << '\n';
    params.scale = float(std::max({extent[0], extent[1], extent[2]}))*0.5f;
    if (!std::isfinite(params.scale) || params.scale <= 0)
        throw std::runtime_error("Expected finite, nonempty cloud bounds");
    params.center = {float(center[0]), float(center[1]), float(center[2])};
    params.halfBounds = {float(extent[0])/(2*params.scale), float(extent[1])/(2*params.scale), float(extent[2])/(2*params.scale)};
    Renderer renderer = makeRenderer(params, std::string(argv[0])+".ptx");
    nanovdb::FloatGrid* deviceGrid;
    check(cudaMalloc(&deviceGrid, handle.size()));
    check(cudaMemcpy(deviceGrid, handle.data(), handle.size(), cudaMemcpyHostToDevice));
    params.grid = deviceGrid;
    size_t skyCells=air::SkyResolution*air::SkyResolution*air::SkyResolution;
    check(cudaMalloc(&params.skyVisibility,skyCells*air::SkySamples*sizeof(float)));
    check(cudaMalloc(&params.skyVolume,skyCells*sizeof(float4)));
    std::cout << "Loaded " << filename << " (" << handle.size() << " bytes)\n";

    std::vector<uchar4> pixels;
    Texture2D texture {};
    float yaw=0, pitch=-0.25f, distance=3.2f;
    float sunYaw=2.75f, sunPitch=0.55f, extinction=4, exposure=-1, altitude=3, haze=1, albedo=0.95f;
    bool refined=true, dirty=true, lightingDirty=true, skyCacheDirty=true;
    float visibilityExtinction=-1;
    unsigned samples=0;
    const float3 cloudColors[]={{1,1,1},{0.15f,0.9f,6.0f},{1.4f,0.85f,0.5f}};
    const float3 skyColors[]={{1,1,1},{0.5f,0.85f,1.8f},{1.5f,0.9f,0.5f}};
    int cloudColor=0, skyColor=0;
    auto preset=[&](int i) {
        auto p=cloud::preset(i);
        yaw=p.yaw; pitch=p.pitch; distance=p.distance; sunYaw=p.sunYaw; sunPitch=p.sunPitch;
        extinction=p.extinction; exposure=p.exposure; altitude=p.altitude; haze=p.haze; albedo=p.albedo;
        cloudColor=i==1 ? 2 : i==2 ? 1 : 0; skyColor=i==1 ? 1 : i==2 ? 2 : 0;
        if (reference || neutral) cloudColor=skyColor=0;
        if (customExposure) exposure=initialExposure;
        if (initialAlbedo>=0) albedo=initialAlbedo;
        if (initialDensity>=0) extinction=initialDensity;
        dirty=true; lightingDirty=true;
    };
    preset(initialPreset);
    rlImGuiSetup(true);
    ImGui::GetStyle().ScaleAllSizes(2.0f);
    ImGui::GetStyle().FontScaleMain=2.0f;
    ImGui::GetIO().IniFilename=nullptr;
    bool showControls=true;
    int frames = 0;
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        rlImGuiBegin();
        if (IsKeyPressed(KEY_F1)) showControls=!showControls;
        bool saveRequested=false, uiChanged=false;
        if (showControls) {
            ImGui::SetNextWindowPos({0,0});
            ImGui::SetNextWindowSize({float(panelWidth),float(GetScreenHeight())});
            ImGui::Begin("WDAS Cloud / OptiX",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse);
            ImGui::PushItemWidth(-8*ImGui::GetFontSize());
            ImGui::TextUnformatted(GetFileName(filename));
            ImGui::Text("%.1f MB / 100 MB limit",handle.size()/1e6);
            ImGui::Text("%u spp   %d x %d   %.1f FPS",samples,texture.width,texture.height,ImGui::GetIO().Framerate);
            ImGui::Separator();
            int mode=reference ? 1 : 0;
            if (ImGui::Combo("Method",&mode,"Ray march\0Reference path trace\0")) {
                reference=mode==1; cloudColor=skyColor=0; uiChanged=true;
            }
            if (ImGui::Button("Day")) preset(0);
            ImGui::SameLine(); if (ImGui::Button("Warm")) preset(1);
            ImGui::SameLine(); if (ImGui::Button("Cool")) preset(2);
            ImGui::SeparatorText("Light and atmosphere");
            bool lightChanged=ImGui::SliderAngle("Sun azimuth",&sunYaw,-180,180);
            lightChanged|=ImGui::SliderAngle("Sun elevation",&sunPitch,-8.5f,86);
            lightChanged|=ImGui::SliderFloat("Altitude (km)",&altitude,0.65f,12,"%.2f");
            lightChanged|=ImGui::SliderFloat("Haze",&haze,0,6,"%.2f");
            lightingDirty|=lightChanged; uiChanged|=lightChanged;
            uiChanged|=ImGui::SliderFloat("Density",&extinction,0.05f,40,"%.2f",ImGuiSliderFlags_Logarithmic);
            uiChanged|=ImGui::SliderFloat("Exposure (EV)",&exposure,-6,4,"%+.2f");
            uiChanged|=ImGui::SliderFloat("Cloud albedo",&albedo,0,1,"%.4f",ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fraction of cloud extinction that scatters light.\nTry 0.995 in Reference mode for brighter interiors.");
            ImGui::SeparatorText("Rendering");
            uiChanged|=ImGui::Checkbox("Refine when idle",&refined);
            bool automatic=fixedWidth==0;
            if (ImGui::Checkbox("Automatic resolution",&automatic)) {
                fixedWidth=automatic ? 0 : 240; uiChanged=true;
            }
            if (!automatic) uiChanged|=ImGui::SliderInt("Render width",&fixedWidth,16,1024);
            if (reference) {
                uiChanged|=ImGui::SliderInt("Bounce limit",&maxDepth,0,64);
                ImGui::TextDisabled("0 = unlimited, Russian roulette");
            }
            if (ImGui::CollapsingHeader("Camera")) {
                uiChanged|=ImGui::SliderAngle("Yaw",&yaw,-180,180);
                uiChanged|=ImGui::SliderAngle("Pitch",&pitch,-83,83);
                uiChanged|=ImGui::SliderFloat("Distance",&distance,1.4f,12,"%.2f");
            }
            if (!reference && ImGui::CollapsingHeader("Color overrides")) {
                uiChanged|=ImGui::Combo("Cloud",&cloudColor,"Neutral\0Blue\0Orange\0");
                uiChanged|=ImGui::Combo("Sky",&skyColor,"Neutral\0Blue\0Orange\0");
                if (ImGui::Button("Clear overrides")) { cloudColor=skyColor=0; uiChanged=true; }
            }
            ImGui::Separator();
            if (ImGui::Button("Reset")) preset(0);
            ImGui::SameLine(); if (ImGui::Button("Save image")) saveRequested=true;
            if (reference && ImGui::Button("Restart accumulation")) uiChanged=true;
            ImGui::TextWrapped("Left drag: orbit | Wheel: zoom\nRight drag: sun | P: method\nF1: hide controls | S: screenshot");
            ImGui::PopItemWidth();
            ImGui::End();
        }
        dirty|=uiChanged;
        bool captureMouse=ImGui::GetIO().WantCaptureMouse;
        auto keyDown=[](int key) { return !ImGui::GetIO().WantCaptureKeyboard && IsKeyDown(key); };
        auto keyPressed=[](int key) { return !ImGui::GetIO().WantCaptureKeyboard && IsKeyPressed(key); };

        Vector2 delta = GetMouseDelta();
        float wheel = captureMouse ? 0 : GetMouseWheelMove();
        bool drag = !captureMouse && (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
                    && (delta.x != 0 || delta.y != 0);
        bool adjusting = keyDown(KEY_LEFT_BRACKET) || keyDown(KEY_RIGHT_BRACKET)
                         || keyDown(KEY_MINUS) || keyDown(KEY_EQUAL);
        bool atmosphereChanged=keyDown(KEY_PAGE_UP) || keyDown(KEY_PAGE_DOWN)
                               || keyDown(KEY_COMMA) || keyDown(KEY_PERIOD);
        adjusting=adjusting || atmosphereChanged;
        bool moving = drag || wheel != 0 || adjusting || uiChanged;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && drag) {
            yaw += delta.x*0.005f;
            pitch = std::clamp(pitch+delta.y*0.005f, -1.45f, 1.45f);
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && drag) {
            sunYaw -= delta.x*0.005f;
            sunPitch = std::clamp(sunPitch+delta.y*0.002f, -0.15f, 1.5f);
            lightingDirty=true;
        }
        if (wheel != 0) distance = std::clamp(distance*std::exp(-wheel*0.1f), 1.4f, 12.0f);
        float dt = std::min(GetFrameTime(), 0.05f);
        altitude=std::clamp(altitude+dt*(keyDown(KEY_PAGE_UP)-keyDown(KEY_PAGE_DOWN)),0.65f,12.0f);
        haze=std::clamp(haze+dt*(keyDown(KEY_PERIOD)-keyDown(KEY_COMMA)),0.0f,6.0f);
        if (atmosphereChanged) lightingDirty=true;
        extinction = std::clamp(extinction*std::exp(dt*(keyDown(KEY_RIGHT_BRACKET)-keyDown(KEY_LEFT_BRACKET))), 0.05f, 40.0f);
        exposure = std::clamp(exposure+2*dt*(keyDown(KEY_EQUAL)-keyDown(KEY_MINUS)), -6.0f, 4.0f);
        if (keyPressed(KEY_SPACE)) { refined = !refined; dirty = true; }
        if (keyPressed(KEY_P)) { reference=!reference; cloudColor=skyColor=0; dirty=true; }
        if (keyPressed(KEY_R)) preset(0);
        for (int i=0;i<3;++i) if (keyPressed(KEY_ONE+i)) preset(i);
        if (!reference && keyPressed(KEY_C)) { cloudColor=(cloudColor+1)%3; dirty=true; }
        if (!reference && keyPressed(KEY_B)) { skyColor=(skyColor+1)%3; dirty=true; }
        if (keyPressed(KEY_ZERO)) { cloudColor=skyColor=0; dirty=true; }

        int width = fixedWidth ? fixedWidth : smoke ? 160 : (refined && !moving ? 480 : 240);
        int viewportLeft=showControls ? panelWidth : 0;
        int viewportWidth=std::max(1,GetScreenWidth()-viewportLeft);
        int height = std::max(1, width*GetScreenHeight()/viewportWidth);
        if (texture.width != width || texture.height != height) {
            if (texture.id) UnloadTexture(texture);
            check(cudaFree(params.pixels));
            check(cudaFree(params.accumulation));
            pixels.resize(size_t(width)*height);
            check(cudaMalloc(&params.pixels, pixels.size()*sizeof(uchar4)));
            check(cudaMalloc(&params.accumulation,pixels.size()*sizeof(float4)));
            Image image {pixels.data(), width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            texture = LoadTextureFromImage(image);
            if (!IsTextureValid(texture)) throw std::runtime_error("Cannot create display texture");
            SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
            dirty = true;
        }
        if (dirty || moving) samples=0;
        if (dirty || moving || reference || targetSamples) {
            params.cameraPosition = {distance*std::cos(pitch)*std::cos(yaw), distance*std::sin(pitch),
                                     distance*std::cos(pitch)*std::sin(yaw)};
            params.sunDirection = {std::cos(sunPitch)*std::cos(sunYaw), std::sin(sunPitch),
                                   std::cos(sunPitch)*std::sin(sunYaw)};
            params.cameraPosition.y=std::max(params.cameraPosition.y,0.02f-altitude);
            params.cloudAltitude=altitude;
            params.cloudTint=cloudColors[cloudColor];
            params.skyTint=skyColors[skyColor];
            params.haze=haze;
            bool visibilityDirty=visibilityExtinction!=extinction;
            if (lightingDirty || visibilityDirty) skyCacheDirty=true;
            if (lightingDirty) {
                float3 p {0,air::Radius+altitude,0};
                for (int i=0;i<air::SkySamples;++i)
                    params.skyLight[i]=air::trace(p,air::skyDirection(i),1e20f,params.sunDirection,haze,64,32,false).radiance;
                lightingDirty=false;
            }
            params.reference=reference;
            params.maxDepth=maxDepth;
            params.sampleIndex=reference ? samples : 0;
            params.atmosphereSteps=reference ? 64 : smoke ? 48 : (refined && !moving ? 64 : 32);
            params.sunSteps=reference ? 32 : smoke ? 24 : (refined && !moving ? 32 : 16);
            params.extinction = extinction;
            params.albedo = albedo;
            params.exposure = exposure;
            params.steps = smoke ? 64 : (refined && !moving ? 192 : 96);
            params.shadowSteps = smoke ? 16 : (refined && !moving ? 32 : 16);
            if (!reference && skyCacheDirty) {
                int viewShadows=params.shadowSteps;
                params.shadowSteps=24;
                for (int pass=1;pass<=2;++pass) {
                    if (pass==1 && !visibilityDirty) continue;
                    params.precompute=pass;
                    check(cudaMemcpy(renderer.deviceParameters,&params,sizeof(params),cudaMemcpyHostToDevice));
                    check(optixLaunch(renderer.pipeline,nullptr,reinterpret_cast<CUdeviceptr>(renderer.deviceParameters),
                                      sizeof(params),&renderer.table,air::SkyResolution,air::SkyResolution,
                                      air::SkyResolution*(pass==1 ? air::SkySamples : 1)));
                }
                params.precompute=0;
                params.shadowSteps=viewShadows;
                visibilityExtinction=extinction;
                skyCacheDirty=false;
            }
            check(cudaMemcpy(renderer.deviceParameters, &params, sizeof(params), cudaMemcpyHostToDevice));
            check(optixLaunch(renderer.pipeline, nullptr, reinterpret_cast<CUdeviceptr>(renderer.deviceParameters),
                              sizeof(params), &renderer.table, width, height, 1));
            check(cudaMemcpy(pixels.data(), params.pixels, pixels.size()*sizeof(uchar4), cudaMemcpyDeviceToHost));
            UpdateTexture(texture, pixels.data());
            ++samples;
            dirty = false;
        }

        DrawTexturePro(texture, {0,0,float(width),float(height)},
                       {float(viewportLeft),0,float(viewportWidth),float(GetScreenHeight())}, {0,0},0,WHITE);
        rlImGuiEnd();
        EndDrawing();
        if (saveRequested || keyPressed(KEY_S) || (smoke && frames==2)) saveScreenshot();
        if (targetSamples && samples>=unsigned(targetSamples)) {
            saveRender(output,width,height,params,pixels);
            std::cout << "Saved " << output << " (" << samples << " spp, " << width << "x" << height << ")\n";
            break;
        }
        if (smoke && ++frames >= 3 && !targetSamples) break;
    }
    rlImGuiShutdown();
    UnloadTexture(texture);
    check(cudaFree(params.pixels));
    check(cudaFree(params.accumulation));
    check(cudaFree(deviceGrid));
    check(cudaFree(params.skyVisibility));
    check(cudaFree(params.skyVolume));
    check(cudaFree(renderer.deviceParameters));
    check(cudaFree(renderer.records));
    check(cudaFree(renderer.acceleration));
    check(optixPipelineDestroy(renderer.pipeline));
    for (auto group : renderer.groups) check(optixProgramGroupDestroy(group));
    check(optixModuleDestroy(renderer.module));
    check(optixDeviceContextDestroy(renderer.context));
    CloseWindow();
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    if (ImGui::GetCurrentContext()) rlImGuiShutdown();
    if (IsWindowReady()) CloseWindow();
    return 1;
}

#endif
