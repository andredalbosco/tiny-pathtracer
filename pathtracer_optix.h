#pragma once

#include "utils.h"
#include <cuda_runtime.h>
#include <optix.h>

struct Surface {
    Float3 normal;
    Float3 albedo;
    Float3 emission;
};

struct PointLight {
    Float3 position;
    Float3 intensity;
};

struct LaunchParams {
    uchar3* pixels;
    OptixTraversableHandle scene;
    const Surface* surfaces;
    Camera camera;
    PointLight light;
};
