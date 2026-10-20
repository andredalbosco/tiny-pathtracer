struct Camera {
    float3 position;
    float3 forward;
    float3 right;
    float3 up;
    float tanHalfFov;
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

struct Parameters {
    Camera camera;
    PointLight light;
};

struct Payload {
    uint hit;
    float distance;
};

ConstantBuffer<Parameters> params;
RaytracingAccelerationStructure scene;
StructuredBuffer<Surface> surfaces;
RWTexture2D<float4> pixels;

static const float pi = 3.14159265358979323846f;

[shader("miss")]
void missColor(inout Payload payload) {
    payload.hit = 0;
    payload.distance = 0;
}

[shader("closesthit")]
void closestHitColor(
    inout Payload payload,
    BuiltInTriangleIntersectionAttributes attributes
) {
    payload.hit = PrimitiveIndex() + 1;
    payload.distance = RayTCurrent();
}

float3 shade(uint hit, float3 position) {
    if (!hit) return float3(0);

    Surface surface = surfaces[hit - 1];
    float3 toLight = params.light.position - position;
    float distanceSquared = dot(toLight, toLight);
    float distance = sqrt(distanceSquared);
    if (distance <= 0.02f) return surface.emission;

    float3 lightDirection = toLight / distance;
    float cosine = max(0.0f, dot(surface.normal, lightDirection));
    if (cosine <= 0) return surface.emission;

    float3 shadowOrigin = position + surface.normal * 0.01f;
    float3 shadowDirection = params.light.position - shadowOrigin;
    float shadowDistance = length(shadowDirection);

    RayDesc shadowRay;
    shadowRay.Origin = shadowOrigin;
    shadowRay.Direction = shadowDirection / shadowDistance;
    shadowRay.TMin = 0.0f;
    shadowRay.TMax = shadowDistance - 0.01f;

    Payload shadowPayload = {};
    TraceRay(scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 255,
             0, 1, 0, shadowRay, shadowPayload);

    if (shadowPayload.hit) return surface.emission;

    return surface.emission + surface.albedo * params.light.intensity
         * (cosine / (pi * distanceSquared));
}

float3 gammaCorrect(float3 color) {
    return pow(color, float3(1.0f / 2.2f));
}

[shader("raygeneration")]
void render() {
    uint2 index = DispatchRaysIndex().xy;
    uint2 size = DispatchRaysDimensions().xy;

    float2 pixelPos = float2(index) + 0.5f;
    float2 clipPos = pixelPos / float2(size) * float2(2, -2) + float2(-1, 1);

    Camera camera = params.camera;
    float sx = camera.tanHalfFov * float(size.x) / float(size.y) * clipPos.x;
    float sy = camera.tanHalfFov * clipPos.y;
    float3 direction = camera.forward + camera.right * sx + camera.up * sy;

    RayDesc ray;
    ray.Origin = camera.position;
    ray.Direction = direction;
    ray.TMin = 0.0f;
    ray.TMax = 2000.0f;

    Payload payload = {};
    TraceRay(scene, RAY_FLAG_NONE, 255, 0, 1, 0, ray, payload);

    float3 position = camera.position + direction * payload.distance;
    float3 color = shade(payload.hit, position);

    color = gammaCorrect(saturate(color));
    pixels[index] = float4(color, 1.0f);
}
