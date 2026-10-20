#pragma once

#include <cuda_runtime.h>
#include <optix.h>

struct Parameters {
    uchar3* pixels;
    OptixTraversableHandle triangle;
};

