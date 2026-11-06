#pragma once

#include <cuda_runtime.h>
#include <optix.h>

struct Camera {
    float3 position;
    float3 forward, right, up;
    float tan_half_fov;
};

struct Surface {
    float3 normal;
    float3 albedo;
    float3 emission;
};

struct PointLight {
    float3 position;
    float3 intensity;
};

struct LaunchParams {
    uchar3* pixels;
    OptixTraversableHandle scene;
    const Surface* surfaces;
    Camera camera;
    PointLight light;
};
