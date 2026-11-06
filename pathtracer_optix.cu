#include "pathtracer_optix.h"
#include "external/cuda/helper_math.h"

extern "C" { __constant__ LaunchParams params; }

constexpr unsigned samples_per_pixel = 64;

__device__ float random_float(unsigned& state) {
    state = state * 747796405u + 2891336453u;
    unsigned word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    word = (word >> 22u) ^ word;
    return float(word >> 8u) * (1.0f / 16777216.0f);
}

__device__ float3 sample_cosine_hemisphere(float3 normal, unsigned& state) {
    float radius = sqrtf(random_float(state));
    float angle = 2 * float(M_PI) * random_float(state);
    float3 axis = fabsf(normal.z) < 0.999f ? make_float3(0, 0, 1) : make_float3(0, 1, 0);
    float3 tangent = normalize(cross(axis, normal));
    float3 bitangent = cross(normal, tangent);

    return tangent * (radius * cosf(angle))
         + bitangent * (radius * sinf(angle))
         + normal * sqrtf(fmaxf(0, 1 - radius * radius));
}

extern "C" __global__ void __miss__color() {
    optixSetPayload_0(0);
    optixSetPayload_1(0);
}

extern "C" __global__ void __closesthit__color() {
    optixSetPayload_0(optixGetPrimitiveIndex() + 1);
    optixSetPayload_1(__float_as_uint(optixGetRayTmax()));
}

__device__ float3 direct_lighting(const Surface& surface, float3 position) {
    float3 to_light = params.light.position - position;
    float distance_squared = dot(to_light, to_light);
    float distance = sqrtf(distance_squared);
    if (distance <= 0.02f) return make_float3(0);

    float3 light_direction = to_light / distance;
    float cosine = fmaxf(0, dot(surface.normal, light_direction));
    if (cosine <= 0) return make_float3(0);

    float3 shadow_origin = position + surface.normal * 0.01f;
    float3 shadow_direction = params.light.position - shadow_origin;
    float shadow_distance = length(shadow_direction);
    shadow_direction = shadow_direction / shadow_distance;

    unsigned occluded = 0, shadow_distance_bits = 0;
    optixTrace(params.scene, shadow_origin, shadow_direction,
               0.0f, shadow_distance - 0.01f, 0.0f, 255,
               OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT,
               0, 1, 0, occluded, shadow_distance_bits);

    if (occluded) return make_float3(0);

    return surface.albedo * params.light.intensity
         * (cosine / (float(M_PI) * distance_squared));
}

__device__ float3 trace_path(float3 origin, float3 direction, unsigned& state) {
    float3 radiance = make_float3(0);
    float3 throughput = make_float3(1);

    for (unsigned bounce = 0; ; ++bounce) {
        unsigned hit = 0, distance_bits = 0;
        optixTrace(params.scene, origin, direction,
                   0.0f, 2000.0f, 0.0f, 255, OPTIX_RAY_FLAG_NONE,
                   0, 1, 0, hit, distance_bits);
        if (!hit) return radiance;

        Surface surface = params.surfaces[hit - 1];
        float3 position = origin + direction * __uint_as_float(distance_bits);
        if (dot(surface.normal, direction) > 0) surface.normal = -surface.normal;

        radiance += throughput * (surface.emission + direct_lighting(surface, position));
        throughput *= surface.albedo;

        float survival = fminf(0.95f, fmaxf(throughput.x, fmaxf(throughput.y, throughput.z)));
        if (survival <= 0) return radiance;

        if (bounce >= 3) {
            if (random_float(state) >= survival) return radiance;
            throughput /= survival;
        }

        origin = position + surface.normal * 0.01f;
        direction = sample_cosine_hemisphere(surface.normal, state);
    }
}

__device__ float3 gamma_correct(float3 color) {
    return {
        powf(color.x, 1.0f/2.2f),
        powf(color.y, 1.0f/2.2f),
        powf(color.z, 1.0f/2.2f)
    };
}

extern "C" __global__ void __raygen__render() {
    uint3 index = optixGetLaunchIndex();
    uint3 size = optixGetLaunchDimensions();
    unsigned state = index.y * size.x + index.x;
    const Camera& camera = params.camera;
    float3 color = make_float3(0);

    for (unsigned sample_index = 0; sample_index < samples_per_pixel; ++sample_index) {
        float jitterX = random_float(state);
        float jitterY = random_float(state);
        float2 pixelPos = make_float2(index.x + jitterX, index.y + jitterY);
        float2 clipPos = pixelPos / make_float2(size.x, size.y) * make_float2(2, -2) + make_float2(-1, 1);
        float sx = camera.tan_half_fov * float(size.x) / size.y * clipPos.x;
        float sy = camera.tan_half_fov * clipPos.y;
        float3 direction = normalize(camera.forward + camera.right * sx + camera.up * sy);

        color += trace_path(camera.position, direction, state);
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
