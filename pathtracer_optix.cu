#include "pathtracer_optix.h"

extern "C" { __constant__ Parameters params; }

extern "C" __global__ void __miss__color() {
    optixSetPayload_0(0);
}

extern "C" __global__ void __closesthit__color() {
    optixSetPayload_0(1);
}

extern "C" __global__ void __raygen__render() {
    uint3 pixel = optixGetLaunchIndex();
    uint3 size = optixGetLaunchDimensions();

    float x = (2 * (pixel.x + 0.5f) / size.x - 1) * float(size.x) / size.y;
    float y = 1 - 2 * (pixel.y + 0.5f) / size.y;

    unsigned hit = 0;
    optixTrace(params.triangle, make_float3(x, y, -1), make_float3(0, 0, 1),
               0.0f, 10.0f, 0.0f, 255, OPTIX_RAY_FLAG_NONE,
               0, 1, 0, hit);

    params.pixels[pixel.y * size.x + pixel.x] =
        hit ? make_uchar3(255, 160, 40) : make_uchar3(0, 0, 0);
}
