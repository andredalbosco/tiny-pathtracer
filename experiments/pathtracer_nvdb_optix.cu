#include "pathtracer_nvdb_optix.h"
#include "../external/cuda/helper_math.h"
#include <nanovdb/util/HDDA.h>
#include <nanovdb/util/Ray.h>

extern "C" { __constant__ Parameters params; }

extern "C" __global__ void __intersection__cloud() {
    float3 origin = optixGetObjectRayOrigin();
    float3 direction = optixGetObjectRayDirection();
    const auto& grid = *params.grid;
    nanovdb::Ray<float> ray(
        grid.worldToIndexF(nanovdb::Vec3f(origin.x, origin.y, origin.z)),
        grid.worldToIndexDirF(nanovdb::Vec3f(direction.x, direction.y, direction.z)),
        optixGetRayTmin(), optixGetRayTmax());

    auto accessor = grid.getAccessor();
    if (!ray.clip(accessor.root().bbox())) return;

    auto voxel = nanovdb::RoundDown<nanovdb::Coord>(ray.start());
    nanovdb::HDDA<nanovdb::Ray<float>> hdda(ray, accessor.getDim(voxel, ray));

    for (;;) {
        voxel = hdda.voxel();
        if (hdda.update(ray, accessor.getDim(voxel, ray))) continue;

        if (accessor.getValue(voxel) > 0) {
            optixReportIntersection(hdda.time(), 0);
            return;
        }
        if (!hdda.step()) return;
    }
}

extern "C" __global__ void __closesthit__cloud() {
    optixSetPayload_0(1);
}

extern "C" __global__ void __miss__cloud() {
    optixSetPayload_0(0);
}

extern "C" __global__ void __raygen__render() {
    uint3 index = optixGetLaunchIndex();
    uint3 size = optixGetLaunchDimensions();
    float2 pixelPos = make_float2(index.x + 0.5f, index.y + 0.5f);
    float2 clipPos = pixelPos / make_float2(size.x, size.y) * make_float2(2, -2) + make_float2(-1, 1);

    const Camera& camera = params.camera;
    float sx = camera.tanHalfFov * float(size.x) / size.y * clipPos.x;
    float sy = camera.tanHalfFov * clipPos.y;
    float3 direction = normalize(camera.forward + camera.right * sx + camera.up * sy);

    unsigned hit = 0;
    optixTrace(params.scene, camera.position, direction,
               0.0f, 1e16f, 0.0f, 255, OPTIX_RAY_FLAG_DISABLE_ANYHIT,
               0, 1, 0, hit);

    unsigned char value = hit ? 255 : 0;
    params.pixels[index.y * size.x + index.x] = make_uchar3(value, value, value);
}
