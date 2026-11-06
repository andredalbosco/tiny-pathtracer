#pragma once

#include <cuda_runtime.h>
#include <optix.h>
#include <nanovdb/NanoVDB.h>

struct CloudParameters {
    OptixTraversableHandle volume;
    const nanovdb::FloatGrid* grid;
    float4* pixels;
    float3 center;
    float scale;
    float3 halfBounds;
    int steps, shadowSteps;
    float extinction;
};
