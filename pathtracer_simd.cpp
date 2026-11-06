#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <immintrin.h>

#include "utils_linalg.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

const int max_depth = 24;
const int samples_per_pixel = 128;
const int image_width = 320;

struct Ray { Float3 origin, direction; };

struct Hit {
    float t = INFINITY, u = 0, v = 0;
    int triangle_index = -1;
};

struct Triangle {
    Float3 p, e1, e2;
    Float3 normal;
    Float3 albedo, emission;

    Triangle(Float3 p, Float3 a, Float3 b, Float3 color, Float3 light = {})
        : p(p), e1(a - p), e2(b - p),
          normal(normalize(cross(e1, e2))),
          albedo(color), emission(light) {}

    bool intersect(Ray ray, Hit& hit) const {
        Float3 h = cross(ray.direction, e2);
        float determinant = dot(e1, h);
        if (determinant < 1e-10f) return false;

        float inverse = 1 / determinant;
        Float3 offset = ray.origin - p;
        float u = dot(offset, h) * inverse;
        if (u < 0 || u > 1) return false;

        Float3 q = cross(offset, e1);
        float v = dot(ray.direction, q) * inverse;
        if (v < 0 || u + v > 1) return false;

        float t = dot(e2, q) * inverse;
        if (t <= 1e-4f || t >= hit.t) return false;
        hit.t = t;
        hit.u = u;
        hit.v = v;
        return true;
    }
};

struct Camera {
    Float3 position, target, up;
    float fovy, aspect;
    Float3 u, v, w;

    void compute_basis() {
        w = normalize(target - position);
        u = normalize(cross(up, w));
        v = cross(w, u);
    }
};

// Each AVX2 lane holds one triangle; rays and the path sampler stay scalar.
struct alignas(32) TrianglePacket {
    float px[8]{}, py[8]{}, pz[8]{};
    float e1x[8]{}, e1y[8]{}, e1z[8]{};
    float e2x[8]{}, e2y[8]{}, e2z[8]{};
};

struct Scene {
    std::vector<Triangle> triangles;
    Camera camera;
    std::vector<TrianglePacket> packets;

    void pack_triangles() {
        packets.assign((triangles.size() + 7) / 8, {});
        for (int i = 0; i < int(triangles.size()); ++i) {
            TrianglePacket& packet = packets[i / 8];
            int lane = i % 8;
            const Triangle& triangle = triangles[i];
            packet.px[lane] = triangle.p.x;
            packet.py[lane] = triangle.p.y;
            packet.pz[lane] = triangle.p.z;
            packet.e1x[lane] = triangle.e1.x;
            packet.e1y[lane] = triangle.e1.y;
            packet.e1z[lane] = triangle.e1.z;
            packet.e2x[lane] = triangle.e2.x;
            packet.e2y[lane] = triangle.e2.y;
            packet.e2z[lane] = triangle.e2.z;
        }
    }

    bool intersect(Ray ray, Hit& hit) const {
        hit = {};
        __m256 dx = _mm256_set1_ps(ray.direction.x);
        __m256 dy = _mm256_set1_ps(ray.direction.y);
        __m256 dz = _mm256_set1_ps(ray.direction.z);
        __m256 ox = _mm256_set1_ps(ray.origin.x);
        __m256 oy = _mm256_set1_ps(ray.origin.y);
        __m256 oz = _mm256_set1_ps(ray.origin.z);
        __m256 zero = _mm256_setzero_ps(), one = _mm256_set1_ps(1);

        for (int i = 0; i < int(packets.size()); ++i) {
            const TrianglePacket& packet = packets[i];
            __m256 e1x = _mm256_load_ps(packet.e1x);
            __m256 e1y = _mm256_load_ps(packet.e1y);
            __m256 e1z = _mm256_load_ps(packet.e1z);
            __m256 e2x = _mm256_load_ps(packet.e2x);
            __m256 e2y = _mm256_load_ps(packet.e2y);
            __m256 e2z = _mm256_load_ps(packet.e2z);

            __m256 hx = _mm256_sub_ps(_mm256_mul_ps(dy, e2z), _mm256_mul_ps(dz, e2y));
            __m256 hy = _mm256_sub_ps(_mm256_mul_ps(dz, e2x), _mm256_mul_ps(dx, e2z));
            __m256 hz = _mm256_sub_ps(_mm256_mul_ps(dx, e2y), _mm256_mul_ps(dy, e2x));
            __m256 determinant = _mm256_add_ps(_mm256_add_ps(
                _mm256_mul_ps(e1x, hx), _mm256_mul_ps(e1y, hy)), _mm256_mul_ps(e1z, hz));
            __m256 mask = _mm256_cmp_ps(determinant, _mm256_set1_ps(1e-10f), _CMP_GE_OQ);
            if (!_mm256_movemask_ps(mask)) continue;

            // Inactive and padded lanes divide by one, avoiding division by zero.
            __m256 inverse = _mm256_div_ps(one, _mm256_blendv_ps(one, determinant, mask));
            __m256 sx = _mm256_sub_ps(ox, _mm256_load_ps(packet.px));
            __m256 sy = _mm256_sub_ps(oy, _mm256_load_ps(packet.py));
            __m256 sz = _mm256_sub_ps(oz, _mm256_load_ps(packet.pz));
            __m256 u = _mm256_mul_ps(_mm256_add_ps(_mm256_add_ps(
                _mm256_mul_ps(sx, hx), _mm256_mul_ps(sy, hy)), _mm256_mul_ps(sz, hz)), inverse);
            mask = _mm256_and_ps(mask, _mm256_cmp_ps(u, zero, _CMP_GE_OQ));
            mask = _mm256_and_ps(mask, _mm256_cmp_ps(u, one, _CMP_LE_OQ));
            if (!_mm256_movemask_ps(mask)) continue;

            __m256 qx = _mm256_sub_ps(_mm256_mul_ps(sy, e1z), _mm256_mul_ps(sz, e1y));
            __m256 qy = _mm256_sub_ps(_mm256_mul_ps(sz, e1x), _mm256_mul_ps(sx, e1z));
            __m256 qz = _mm256_sub_ps(_mm256_mul_ps(sx, e1y), _mm256_mul_ps(sy, e1x));
            __m256 v = _mm256_mul_ps(_mm256_add_ps(_mm256_add_ps(
                _mm256_mul_ps(dx, qx), _mm256_mul_ps(dy, qy)), _mm256_mul_ps(dz, qz)), inverse);
            mask = _mm256_and_ps(mask, _mm256_cmp_ps(v, zero, _CMP_GE_OQ));
            mask = _mm256_and_ps(mask, _mm256_cmp_ps(_mm256_add_ps(u, v), one, _CMP_LE_OQ));
            if (!_mm256_movemask_ps(mask)) continue;

            __m256 t = _mm256_mul_ps(_mm256_add_ps(_mm256_add_ps(
                _mm256_mul_ps(e2x, qx), _mm256_mul_ps(e2y, qy)), _mm256_mul_ps(e2z, qz)), inverse);
            mask = _mm256_and_ps(mask, _mm256_cmp_ps(t, _mm256_set1_ps(1e-4f), _CMP_GT_OQ));
            mask = _mm256_and_ps(mask, _mm256_cmp_ps(t, _mm256_set1_ps(hit.t), _CMP_LT_OQ));
            int lanes = _mm256_movemask_ps(mask);
            if (!lanes) continue;

            alignas(32) float ts[8], us[8], vs[8];
            _mm256_store_ps(ts, t);
            _mm256_store_ps(us, u);
            _mm256_store_ps(vs, v);
            // Visit lanes in triangle order, preserving the scalar tie rule.
            while (lanes) {
                int lane = __builtin_ctz(unsigned(lanes));
                lanes &= lanes - 1;
                if (ts[lane] >= hit.t) continue;
                hit = {ts[lane], us[lane], vs[lane], 8 * i + lane};
            }
        }
        return hit.triangle_index >= 0;
    }
};

void add_quad(Scene& scene, Float3 p, Float3 u, Float3 v,
              Float3 color, Float3 emission = {}) {
    scene.triangles.emplace_back(p, p + u, p + u + v, color, emission);
    scene.triangles.emplace_back(p, p + u + v, p + v, color, emission);
}

void add_block(Scene& scene, Float3 center, Float3 size, float degrees,
               Float3 color, Float3 emission = {}) {
    float angle = degrees * M_PI / 180;
    float c = std::cos(angle), s = std::sin(angle);
    Float3 u{size.x * c, 0, -size.x * s};
    Float3 v{size.z * s, 0, size.z * c};
    Float3 up{0, size.y, 0};
    Float3 p = center - (u + v + up) * 0.5f;

    add_quad(scene, p, up, u, color, emission);
    add_quad(scene, p + v, u, up, color, emission);
    add_quad(scene, p, v, up, color, emission);
    add_quad(scene, p + u, up, v, color, emission);
    add_quad(scene, p + up, v, u, color, emission);
    add_quad(scene, p, u, v, color, emission);
}

Scene make_cornell_box() {
    Scene scene;
    scene.camera = {
        {278, 278, -800}, {278, 278, 278}, {0, 1, 0}, 40, 1.0f, {}, {}, {}
    };
    scene.camera.compute_basis();

    Float3 white{0.73f, 0.73f, 0.73f};
    Float3 red{0.65f, 0.05f, 0.05f};
    Float3 green{0.12f, 0.45f, 0.15f};

    add_quad(scene, {0, 0, 0}, {0, 555, 0}, {0, 0, 555}, red);
    add_quad(scene, {555, 0, 0}, {0, 0, 555}, {0, 555, 0}, green);
    add_quad(scene, {0, 0, 0}, {0, 0, 555}, {555, 0, 0}, white);
    add_quad(scene, {0, 555, 0}, {555, 0, 0}, {0, 0, 555}, white);
    add_quad(scene, {0, 0, 555}, {0, 555, 0}, {555, 0, 0}, white);

    add_quad(scene, {213, 554, 227}, {130, 0, 0}, {0, 0, 105}, {}, {15, 15, 15});

    add_block(scene, {370, 82.5f, 169}, {165, 165, 165}, 18, white);
    add_block(scene, {187, 165, 351}, {165, 330, 165}, -15, white);

    scene.pack_triangles();
    return scene;
}

float random_float() {
    static std::mt19937_64 engine{42};
    static std::uniform_real_distribution<float> distribution{0.0f, 1.0f};
    return distribution(engine);
}

Float2 random_float2() { return {random_float(), random_float()}; }

Float3 sample_uniform_hemisphere(Float3 normal) {
    Float2 sample = random_float2();
    float z = 2 * sample.x - 1;
    float phi = 2 * M_PI * sample.y;
    float r = std::sqrt(std::max(0.0f, 1 - z * z));
    Float3 direction{r * std::cos(phi), r * std::sin(phi), z};
    return dot(direction, normal) < 0 ? direction * -1 : direction;
}

Float3 trace_path(const Scene& scene, Ray ray) {
    Float3 path_weight{1, 1, 1};

    for (int depth = 0; depth < max_depth; ++depth) {
        Hit hit;
        if (!scene.intersect(ray, hit)) return {};

        const Triangle& triangle = scene.triangles[hit.triangle_index];
        if (dot(triangle.emission, triangle.emission) > 0)
            return path_weight * triangle.emission;

        Float3 position = triangle.p + triangle.e1 * hit.u + triangle.e2 * hit.v;
        Float3 origin = position + triangle.normal * 1e-4f;

        Float3 direction = sample_uniform_hemisphere(triangle.normal);
        float cosine = std::max(0.0f, dot(triangle.normal, direction));
        path_weight *= triangle.albedo * (2 * cosine);

        ray = {origin, direction};
    }
    return {};
}

void render(const Scene& scene, int samples, int width, int height, float* pixels) {
    const Camera& camera = scene.camera;
    float focal_length = 1 / std::tan(camera.fovy * 0.5f * M_PI / 180);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Float3 pixel_color{};

            for (int s = 0; s < samples; ++s) {
                Float2 sample_pos = Float2{float(x), float(y)} + random_float2() * 0.5f;
                Float2 sample_uv = sample_pos * Float2{1.0f / width, 1.0f / height};

                Float3 direction = camera.u * (2 * sample_uv.x - 1) * camera.aspect
                                 + camera.v * (-2 * sample_uv.y + 1)
                                 + camera.w * focal_length;
                Ray ray{camera.position, normalize(direction)};

                pixel_color += trace_path(scene, ray);
            }

            pixel_color = pixel_color * (1.0f / samples);
            int i = 3 * (y * width + x);
            std::memcpy(pixels + i, &pixel_color, sizeof(pixel_color));
        }
    }

}

int main(int argc, char** argv) {
    int samples = samples_per_pixel;
    int width = image_width;
    std::string filename = argc > 3 ? argv[3] : "cornell_box.hdr";

    try {
        if (argc > 1) samples = std::stoi(argv[1]);
        if (argc > 2) width = std::stoi(argv[2]);
    } catch (const std::exception&) {
        std::cerr << "Usage: " << argv[0] << " [samples] [width] [output.hdr]\n";
        return 1;
    }

    if (samples < 1 || width < 1 || argc > 4) {
        std::cerr << "Samples and width must be positive.\n";
        return 1;
    }

    Scene scene = make_cornell_box();
    int height = int(width / scene.camera.aspect);
    std::vector<float> pixels(width * height * 3);
    std::cerr << "Rendering " << width << 'x' << height << " at " << samples << " spp\n";
    render(scene, samples, width, height, pixels.data());
    if (!stbi_write_hdr(filename.c_str(), width, height, 3, pixels.data())) {
        std::cerr << "Cannot write " << filename << '\n';
        return 1;
    }
    std::cerr << "Saved " << filename << '\n';
    return 0;
}
