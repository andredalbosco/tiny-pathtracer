#include "pathtracer_cloud.h"

extern "C" { __constant__ CloudParameters params; }

__device__ float3 operator+(float3 a, float3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
__device__ float3 operator-(float3 a, float3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
__device__ float3 operator*(float3 a, float b) { return {a.x*b, a.y*b, a.z*b}; }
__device__ float dot(float3 a, float3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
__device__ float3 normalize(float3 a) { return a * rsqrtf(dot(a, a)); }

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

extern "C" __global__ void __raygen__render() {
    uint3 pixel = optixGetLaunchIndex(), size = optixGetLaunchDimensions();
    float x = (2*(pixel.x+0.5f)/size.x-1) * float(size.x)/size.y * 0.414213562f;
    float y = (1-2*(pixel.y+0.5f)/size.y) * 0.414213562f;
    float3 origin {3.2f, 0.32f, 0};
    float3 forward = normalize(origin * -1);
    float3 right {0, 0, -1};
    float3 up {forward.y, -forward.x, 0};
    float3 direction = normalize(forward + right*x + up*y);

    unsigned enter = __float_as_uint(1.0f), exit = 0;
    optixTrace(params.volume, origin, direction, 0.0f, 1e20f, 0.0f, 255,
               OPTIX_RAY_FLAG_DISABLE_ANYHIT, 0, 1, 0, enter, exit);

    float t0 = __uint_as_float(enter), t1 = __uint_as_float(exit);
    float skyMix = fminf(1, fmaxf(0, 0.5f+0.5f*direction.y));
    float3 background = float3{0.12f, 0.21f, 0.38f} * (1-skyMix)
                      + float3{0.65f, 0.78f, 0.95f} * skyMix;
    float3 color {0, 0, 0};
    float transmittance = 1;
    float3 sun = normalize(float3{-0.6f, 0.8f, 0.5f});
    float g = 0.35f, mu = dot(direction, sun);
    float phase = (1-g*g) / (12.566370614f * powf(1+g*g-2*g*mu, 1.5f));
    float step = fmaxf(0, t1-t0) / params.steps;

    if (params.extinction > 0 && t1 > t0) {
        for (int i = 0; i < params.steps; ++i) {
            float3 p = origin + direction*(t0+(i+0.5f)*step);
            float sigma = density(p) * params.extinction;
            float attenuation = expf(-sigma*step);

            if (sigma > 1e-5f) {
                float shadow = sunlight(p, sun);
                float3 source = (float3{30, 28, 25} * (phase*shadow)
                               + float3{0.07f, 0.09f, 0.12f}) * 0.95f;
                color = color + source * (transmittance*(1-attenuation));
            }

            transmittance *= attenuation;
            if (transmittance < 1e-5f) break;
        }
    }

    color = color + background * transmittance;
    params.pixels[pixel.y*size.x+pixel.x] = make_float4(color.x, color.y, color.z, 1);
}
