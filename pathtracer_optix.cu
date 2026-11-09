#include "pathtracer_optix.h"

extern "C" { __constant__ LaunchParams params {}; }

constexpr unsigned samples_per_pixel = 64;

extern "C" __global__ void __miss__color() {
    optixSetPayload_0(0);
    optixSetPayload_1(0);
}

extern "C" __global__ void __closesthit__color() {
    optixSetPayload_0(optixGetPrimitiveIndex() + 1);
    optixSetPayload_1(__float_as_uint(optixGetRayTmax()));
}

__device__ Float3 direct_lighting(const Surface& surface, Float3 position) {
    Float3 to_light = params.light.position - position;
    float distance_squared = dot(to_light, to_light);
    float distance = sqrtf(distance_squared);
    if (distance <= 0.02f) return Float3 {};

    Float3 light_direction = to_light / distance;
    float cosine = fmaxf(0, dot(surface.normal, light_direction));
    if (cosine <= 0) return Float3 {};

    Float3 shadow_origin = position + surface.normal * 0.01f;
    Float3 shadow_direction = params.light.position - shadow_origin;
    float shadow_distance = length(shadow_direction);
    shadow_direction = shadow_direction / shadow_distance;

    unsigned occluded = 0, shadow_distance_bits = 0;
    optixTrace(params.scene, make_float3(shadow_origin.x, shadow_origin.y, shadow_origin.z),
               make_float3(shadow_direction.x, shadow_direction.y, shadow_direction.z),
               0.0f, shadow_distance - 0.01f, 0.0f, 255,
               OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT,
               0, 1, 0, occluded, shadow_distance_bits);

    if (occluded) return Float3 {};

    return surface.albedo * params.light.intensity
         * (cosine / (float(M_PI) * distance_squared));
}

__device__ Float3 trace_path(Float3 origin, Float3 direction, pcg32& random) {
    Float3 radiance {};
    Float3 throughput {1, 1, 1};

    for (unsigned bounce = 0; ; ++bounce) {
        unsigned hit = 0, distance_bits = 0;
        optixTrace(params.scene, make_float3(origin.x, origin.y, origin.z),
                   make_float3(direction.x, direction.y, direction.z),
                   0.0f, 2000.0f, 0.0f, 255, OPTIX_RAY_FLAG_NONE,
                   0, 1, 0, hit, distance_bits);
        if (!hit) return radiance;

        Surface surface = params.surfaces[hit - 1];
        Float3 position = origin + direction * __uint_as_float(distance_bits);
        if (dot(surface.normal, direction) > 0) surface.normal = -surface.normal;

        radiance += throughput * (surface.emission + direct_lighting(surface, position));
        throughput *= surface.albedo;

        float survival = fminf(0.95f, fmaxf(throughput.x, fmaxf(throughput.y, throughput.z)));
        if (survival <= 0) return radiance;

        if (bounce >= 3) {
            if (random_float(random) >= survival) return radiance;
            throughput /= survival;
        }

        origin = position + surface.normal * 0.01f;
        direction = sample_cosine_hemisphere(surface.normal, random);
    }
}

extern "C" __global__ void __raygen__render() {
    uint3 index = optixGetLaunchIndex();
    uint3 size = optixGetLaunchDimensions();
    std::uint64_t pixel_index = std::uint64_t(index.y) * size.x + index.x;
    const Camera& camera = params.camera;
    Float3 color {};

    for (unsigned sample_index = 0; sample_index < samples_per_pixel; ++sample_index) {
        std::uint64_t stream_seed = (pixel_index << 32) | std::uint64_t(sample_index);
        pcg32 random(42, stream_seed);
        Float2 sample_pos = Float2 {float(index.x), float(index.y)} + random_float2(random);
        Float2 sample_uv = sample_pos * Float2 {1.0f / size.x, 1.0f / size.y};
        Ray ray = camera.sample_ray(sample_uv);
        color += trace_path(ray.origin, ray.direction, random);
    }

    color /= float(samples_per_pixel);
    color = clamp(color, 0.0f, 1.0f);
    color = gamma_correct(color);
    params.pixels[index.y * size.x + index.x] = make_uchar3(
        255 * color.x,
        255 * color.y,
        255 * color.z
    );
}
