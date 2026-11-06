#include "pathtracer_optix.h"
#include "external/cuda/helper_math.h"

extern "C" { __constant__ Parameters params; }

constexpr unsigned samplesPerPixel = 64;

__device__ float randomFloat(unsigned& state) {
    state = state * 747796405u + 2891336453u;
    unsigned word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    word = (word >> 22u) ^ word;
    return float(word >> 8u) * (1.0f / 16777216.0f);
}

__device__ float3 sampleHemisphere(float3 normal, unsigned& state) {
    float radius = sqrtf(randomFloat(state));
    float angle = 2 * float(M_PI) * randomFloat(state);
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

__device__ float3 directLighting(const Surface& surface, float3 position) {
    float3 toLight = params.light.position - position;
    float distanceSquared = dot(toLight, toLight);
    float distance = sqrtf(distanceSquared);
    if (distance <= 0.02f) return make_float3(0);

    float3 lightDirection = toLight / distance;
    float cosine = fmaxf(0, dot(surface.normal, lightDirection));
    if (cosine <= 0) return make_float3(0);

    float3 shadowOrigin = position + surface.normal * 0.01f;
    float3 shadowDirection = params.light.position - shadowOrigin;
    float shadowDistance = length(shadowDirection);
    shadowDirection = shadowDirection / shadowDistance;

    unsigned occluded = 0, shadowDistanceBits = 0;
    optixTrace(params.scene, shadowOrigin, shadowDirection,
               0.0f, shadowDistance - 0.01f, 0.0f, 255,
               OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT,
               0, 1, 0, occluded, shadowDistanceBits);

    if (occluded) return make_float3(0);

    return surface.albedo * params.light.intensity
         * (cosine / (float(M_PI) * distanceSquared));
}

__device__ float3 tracePath(float3 origin, float3 direction, unsigned& state) {
    float3 radiance = make_float3(0);
    float3 throughput = make_float3(1);

    for (unsigned bounce = 0; ; ++bounce) {
        unsigned hit = 0, distanceBits = 0;
        optixTrace(params.scene, origin, direction,
                   0.0f, 2000.0f, 0.0f, 255, OPTIX_RAY_FLAG_NONE,
                   0, 1, 0, hit, distanceBits);
        if (!hit) return radiance;

        Surface surface = params.surfaces[hit - 1];
        float3 position = origin + direction * __uint_as_float(distanceBits);
        if (dot(surface.normal, direction) > 0) surface.normal = -surface.normal;

        radiance += throughput * (surface.emission + directLighting(surface, position));
        throughput *= surface.albedo;

        float survival = fminf(0.95f, fmaxf(throughput.x, fmaxf(throughput.y, throughput.z)));
        if (survival <= 0) return radiance;

        if (bounce >= 3) {
            if (randomFloat(state) >= survival) return radiance;
            throughput /= survival;
        }

        origin = position + surface.normal * 0.01f;
        direction = sampleHemisphere(surface.normal, state);
    }
}

__device__ float3 gammaCorrect(float3 color) {
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

    for (unsigned sampleIndex = 0; sampleIndex < samplesPerPixel; ++sampleIndex) {
        float jitterX = randomFloat(state);
        float jitterY = randomFloat(state);
        float2 pixelPos = make_float2(index.x + jitterX, index.y + jitterY);
        float2 clipPos = pixelPos / make_float2(size.x, size.y) * make_float2(2, -2) + make_float2(-1, 1);
        float sx = camera.tanHalfFov * float(size.x) / size.y * clipPos.x;
        float sy = camera.tanHalfFov * clipPos.y;
        float3 direction = normalize(camera.forward + camera.right * sx + camera.up * sy);

        color += tracePath(camera.position, direction, state);
    }

    color /= float(samplesPerPixel);
    color = clamp(color, 0.0f, 1.0f);
    color = gammaCorrect(color);
    params.pixels[index.y * size.x + index.x] = make_uchar3(
        255 * color.x,
        255 * color.y,
        255 * color.z
    );
}
