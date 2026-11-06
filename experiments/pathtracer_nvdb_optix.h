#pragma once

#include <cuda_runtime.h>
#include <optix.h>
#include <nanovdb/NanoVDB.h>

struct Camera {
    float3 position;
    float3 forward, right, up;
    float tanHalfFov;
};

struct Parameters {
    uchar3* pixels;
    OptixTraversableHandle scene;
    const nanovdb::FloatGrid* grid;
    Camera camera;
};
