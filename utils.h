#pragma once

#include <cstdio>
#include <cstdlib>

#ifdef __CUDACC__
#include <cuda_runtime.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "external/pcg/pcg32.h"

#ifndef __CUDACC__
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#endif

#ifdef __CUDACC__
#define HOST_DEVICE __host__ __device__
#else
#define HOST_DEVICE
#endif

// Debug

HOST_DEVICE inline void check_failed(const char* condition, const char* file, int line) {
#ifdef __CUDA_ARCH__
    __assert_fail(condition, file, unsigned(line), "CHECK");
#else
    std::fprintf(stderr, "%s:%d: Check failed: %s\n", file, line, condition);
    std::abort();
#endif
}

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition))                                                      \
            ::check_failed(#condition, __FILE__, __LINE__);                    \
    } while (false)

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_NE(a, b) CHECK((a) != (b))
#define CHECK_LT(a, b) CHECK((a) < (b))
#define CHECK_LE(a, b) CHECK((a) <= (b))
#define CHECK_GT(a, b) CHECK((a) > (b))
#define CHECK_GE(a, b) CHECK((a) >= (b))

// Math

HOST_DEVICE inline float clamp(float v, float lower, float upper) {
    float bounded = v < lower ? lower : v;
    return upper < bounded ? upper : bounded;
}

struct Float2 {
    float x, y;

    HOST_DEVICE constexpr Float2(float x = 0, float y = 0) : x(x), y(y) {}

    HOST_DEVICE float& operator[](std::size_t index) {
        if (index == 0) return x;
        return y;
    }

    HOST_DEVICE const float& operator[](std::size_t index) const {
        if (index == 0) return x;
        return y;
    }

    HOST_DEVICE constexpr Float2 operator+() const { return *this; }
    HOST_DEVICE constexpr Float2 operator-() const { return {-x, -y}; }

    HOST_DEVICE constexpr Float2 operator+(Float2 b) const { return {x + b.x, y + b.y}; }
    HOST_DEVICE constexpr Float2 operator-(Float2 b) const { return {x - b.x, y - b.y}; }
    HOST_DEVICE constexpr Float2 operator*(Float2 b) const { return {x * b.x, y * b.y}; }
    HOST_DEVICE constexpr Float2 operator/(Float2 b) const { return {x / b.x, y / b.y}; }
    HOST_DEVICE constexpr Float2 operator*(float scale) const { return {x * scale, y * scale}; }
    HOST_DEVICE constexpr Float2 operator/(float scale) const { return {x / scale, y / scale}; }

    HOST_DEVICE Float2& operator+=(Float2 b) { *this = *this + b; return *this; }
    HOST_DEVICE Float2& operator-=(Float2 b) { *this = *this - b; return *this; }
    HOST_DEVICE Float2& operator*=(Float2 b) { *this = *this * b; return *this; }
    HOST_DEVICE Float2& operator/=(Float2 b) { *this = *this / b; return *this; }
    HOST_DEVICE Float2& operator*=(float scale) { *this = *this * scale; return *this; }
    HOST_DEVICE Float2& operator/=(float scale) { *this = *this / scale; return *this; }
};

struct Float3 {
    float x, y, z;

    HOST_DEVICE constexpr Float3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}

    HOST_DEVICE float& operator[](std::size_t index) {
        if (index == 0) return x;
        if (index == 1) return y;
        return z;
    }

    HOST_DEVICE const float& operator[](std::size_t index) const {
        if (index == 0) return x;
        if (index == 1) return y;
        return z;
    }

    HOST_DEVICE constexpr Float3 operator+() const { return *this; }
    HOST_DEVICE constexpr Float3 operator-() const { return {-x, -y, -z}; }

    HOST_DEVICE constexpr Float3 operator+(Float3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    HOST_DEVICE constexpr Float3 operator-(Float3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    HOST_DEVICE constexpr Float3 operator*(Float3 b) const { return {x * b.x, y * b.y, z * b.z}; }
    HOST_DEVICE constexpr Float3 operator/(Float3 b) const { return {x / b.x, y / b.y, z / b.z}; }
    HOST_DEVICE constexpr Float3 operator*(float scale) const { return {x * scale, y * scale, z * scale}; }
    HOST_DEVICE constexpr Float3 operator/(float scale) const { return {x / scale, y / scale, z / scale}; }

    HOST_DEVICE Float3& operator+=(Float3 b) { *this = *this + b; return *this; }
    HOST_DEVICE Float3& operator-=(Float3 b) { *this = *this - b; return *this; }
    HOST_DEVICE Float3& operator*=(Float3 b) { *this = *this * b; return *this; }
    HOST_DEVICE Float3& operator/=(Float3 b) { *this = *this / b; return *this; }
    HOST_DEVICE Float3& operator*=(float scale) { *this = *this * scale; return *this; }
    HOST_DEVICE Float3& operator/=(float scale) { *this = *this / scale; return *this; }
};

HOST_DEVICE constexpr Float2 operator*(float scale, Float2 v) { return v * scale; }

HOST_DEVICE constexpr Float3 operator*(float scale, Float3 v) { return v * scale; }

HOST_DEVICE constexpr Float2 operator/(float scale, Float2 v) { return {scale / v.x, scale / v.y}; }

HOST_DEVICE constexpr Float3 operator/(float scale, Float3 v) { return {scale / v.x, scale / v.y, scale / v.z}; }

HOST_DEVICE constexpr bool operator==(Float2 a, Float2 b) {
    return a.x == b.x && a.y == b.y;
}

HOST_DEVICE constexpr bool operator==(Float3 a, Float3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

HOST_DEVICE constexpr bool operator!=(Float2 a, Float2 b) { return !(a == b); }

HOST_DEVICE constexpr bool operator!=(Float3 a, Float3 b) { return !(a == b); }

HOST_DEVICE constexpr float dot(Float2 a, Float2 b) {
    return a.x * b.x + a.y * b.y;
}

HOST_DEVICE constexpr float dot(Float3 a, Float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

HOST_DEVICE constexpr Float3 cross(Float3 a, Float3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

HOST_DEVICE constexpr float length_squared(Float2 v) { return dot(v, v); }

HOST_DEVICE constexpr float length_squared(Float3 v) { return dot(v, v); }

HOST_DEVICE inline float length(Float2 v) { return sqrtf(length_squared(v)); }

HOST_DEVICE inline float length(Float3 v) { return sqrtf(length_squared(v)); }

HOST_DEVICE inline float distance(Float2 a, Float2 b) { return length(a - b); }

HOST_DEVICE inline float distance(Float3 a, Float3 b) { return length(a - b); }

HOST_DEVICE constexpr bool is_normalized(Float2 v) {
    float squared_length = length_squared(v);
    return squared_length >= 1 - 1e-4f && squared_length <= 1 + 1e-4f;
}

HOST_DEVICE constexpr bool is_normalized(Float3 v) {
    float squared_length = length_squared(v);
    return squared_length >= 1 - 1e-4f && squared_length <= 1 + 1e-4f;
}

HOST_DEVICE inline Float2 normalize(Float2 v) {
    float vector_length = length(v);
    Float2 direction = v * (1 / vector_length);
    return direction;
}

HOST_DEVICE inline Float3 normalize(Float3 v) {
    float vector_length = length(v);
    Float3 direction = v * (1 / vector_length);
    return direction;
}

HOST_DEVICE constexpr Float2 reflect(Float2 direction, Float2 normal) {
    return direction - normal * (2 * dot(direction, normal));
}

HOST_DEVICE constexpr Float3 reflect(Float3 direction, Float3 normal) {
    return direction - normal * (2 * dot(direction, normal));
}

HOST_DEVICE inline Float2 min(Float2 a, Float2 b) {
    return {(b.x < a.x ? b.x : a.x), (b.y < a.y ? b.y : a.y)};
}

HOST_DEVICE inline Float3 min(Float3 a, Float3 b) {
    return {(b.x < a.x ? b.x : a.x), (b.y < a.y ? b.y : a.y), (b.z < a.z ? b.z : a.z)};
}

HOST_DEVICE inline Float2 max(Float2 a, Float2 b) {
    return {(a.x < b.x ? b.x : a.x), (a.y < b.y ? b.y : a.y)};
}

HOST_DEVICE inline Float3 max(Float3 a, Float3 b) {
    return {(a.x < b.x ? b.x : a.x), (a.y < b.y ? b.y : a.y), (a.z < b.z ? b.z : a.z)};
}

HOST_DEVICE inline Float2 abs(Float2 v) {
    return {fabsf(v.x), fabsf(v.y)};
}

HOST_DEVICE inline Float3 abs(Float3 v) {
    return {fabsf(v.x), fabsf(v.y), fabsf(v.z)};
}

HOST_DEVICE inline Float2 clamp(Float2 v, Float2 lower, Float2 upper) {
    return min(max(v, lower), upper);
}

HOST_DEVICE inline Float2 clamp(Float2 v, float lower, float upper) {
    return {
        clamp(v.x, lower, upper),
        clamp(v.y, lower, upper)
    };
}

HOST_DEVICE inline Float3 clamp(Float3 v, Float3 lower, Float3 upper) {
    return min(max(v, lower), upper);
}

HOST_DEVICE inline Float3 clamp(Float3 v, float lower, float upper) {
    return {
        clamp(v.x, lower, upper),
        clamp(v.y, lower, upper),
        clamp(v.z, lower, upper)
    };
}

HOST_DEVICE constexpr Float2 lerp(Float2 a, Float2 b, float t) {
    return a * (1 - t) + b * t;
}

HOST_DEVICE constexpr Float3 lerp(Float3 a, Float3 b, float t) {
    return a * (1 - t) + b * t;
}

// Scene

struct Ray { Float3 origin, direction; };

struct Hit {
    float t = INFINITY, u = 0, v = 0;
    int triangle_index = -1;
};

inline bool intersect_triangle(Ray ray, Float3 p, Float3 e1, Float3 e2, float max_t, Hit& hit) {
    Float3 h = cross(ray.direction, e2);
    float determinant = dot(e1, h);
    bool valid = determinant >= 1e-10f;
    float inverse = 1 / std::max(determinant, 1e-10f);

    Float3 offset = ray.origin - p;
    float u = dot(offset, h) * inverse;

    Float3 q = cross(offset, e1);
    float v = dot(ray.direction, q) * inverse;
    float t = dot(e2, q) * inverse;

    valid = valid & (u >= 0) & (u <= 1) & (v >= 0) & (u + v <= 1)
                  & (t > 1e-4f) & (t < max_t);

    hit.t = t;
    hit.u = u;
    hit.v = v;

    return valid;
}

struct Triangle {
    Float3 p, e1, e2;
    Float3 normal;
    Float3 albedo, emission;

    Triangle(Float3 p, Float3 a, Float3 b, Float3 color, Float3 light = {})
        : p(p), e1(a - p), e2(b - p),
          normal(normalize(cross(e1, e2))),
          albedo(color), emission(light) {}

    bool intersect(Ray ray, Hit& hit) const {
        Hit candidate = hit;
        bool valid = intersect_triangle(ray, p, e1, e2, hit.t, candidate);

        if (valid) hit = candidate;

        return valid;
    }
};

struct Camera {
    Float3 position, target, up;
    float tan_half_fovy, aspect;
    Float3 u, v, w;

    HOST_DEVICE Ray sample_ray(Float2 sample_uv) const {
        float x = (2.0f * sample_uv.x - 1.0f) * tan_half_fovy * aspect;
        float y = (1.0f - 2.0f * sample_uv.y) * tan_half_fovy;

        Float3 direction = normalize(u * x + v * y + w);
        return {position, direction};
    }
};

inline Camera make_camera(Float3 position, Float3 target, Float3 up,
                          float fovy, float aspect = 1) {
    Camera camera;

    camera.position = position;
    camera.target = target;
    camera.up = up;
    camera.tan_half_fovy = tan(0.5f * fovy * M_PI / 180);
    camera.aspect = aspect;

    camera.w = normalize(target - position);
    camera.u = normalize(cross(up, camera.w));
    camera.v = cross(camera.w, camera.u);

    return camera;
}

inline void add_quad(std::vector<Triangle>& triangles, Float3 p, Float3 u, Float3 v,
                     Float3 color, Float3 emission = {}) {
    triangles.emplace_back(p, p + u, p + u + v, color, emission);
    triangles.emplace_back(p, p + u + v, p + v, color, emission);
}

inline void add_block(std::vector<Triangle>& triangles, Float3 center, Float3 size, float degrees,
                      Float3 color, Float3 emission = {}) {
    float angle = degrees * M_PI / 180;
    float c = std::cos(angle), s = std::sin(angle);
    Float3 u {size.x * c, 0, -size.x * s};
    Float3 v {size.z * s, 0, size.z * c};
    Float3 up {0, size.y, 0};
    Float3 p = center - (u + v + up) * 0.5f;

    add_quad(triangles, p, up, u, color, emission);
    add_quad(triangles, p + v, u, up, color, emission);
    add_quad(triangles, p, v, up, color, emission);
    add_quad(triangles, p + u, up, v, color, emission);
    add_quad(triangles, p + up, v, u, color, emission);
    add_quad(triangles, p, u, v, color, emission);
}

inline std::vector<Triangle> make_cornell_box() {
    std::vector<Triangle> triangles;

    Float3 white {0.73f, 0.73f, 0.73f};
    Float3 red {0.65f, 0.05f, 0.05f};
    Float3 green {0.12f, 0.45f, 0.15f};

    add_quad(triangles, {0, 0, 0}, {0, 555, 0}, {0, 0, 555}, red);
    add_quad(triangles, {555, 0, 0}, {0, 0, 555}, {0, 555, 0}, green);
    add_quad(triangles, {0, 0, 0}, {0, 0, 555}, {555, 0, 0}, white);
    add_quad(triangles, {0, 555, 0}, {555, 0, 0}, {0, 0, 555}, white);
    add_quad(triangles, {0, 0, 555}, {0, 555, 0}, {555, 0, 0}, white);

    add_quad(triangles, {213, 554, 227}, {130, 0, 0}, {0, 0, 105}, {}, {15, 15, 15});

    add_block(triangles, {370, 82.5f, 169}, {165, 165, 165}, 18, white);
    add_block(triangles, {187, 165, 351}, {165, 330, 165}, -15, white);

    return triangles;
}

// Sampling

HOST_DEVICE inline float random_float(pcg32& random) {
    return float(random() >> 8) * 0x1p-24f;
}

HOST_DEVICE inline Float2 random_float2(pcg32& random) {
    float x = random_float(random);
    float y = random_float(random);
    return Float2(x, y);
}

HOST_DEVICE inline Float3 sample_uniform_hemisphere(Float3 normal, Float2 sample) {
    float z = 2 * sample.x - 1;
    float phi = 2 * M_PI * sample.y;
    float r = sqrtf(fmaxf(0.0f, 1 - z * z));
    Float3 direction {r * cosf(phi), r * sinf(phi), z};
    return dot(direction, normal) < 0 ? direction * -1 : direction;
}

HOST_DEVICE inline Float3 sample_uniform_hemisphere(Float3 normal, pcg32& random) {
    return sample_uniform_hemisphere(normal, random_float2(random));
}

HOST_DEVICE inline Float3 sample_cosine_hemisphere(Float3 normal, pcg32& random) {
    float radius = sqrtf(random_float(random));
    float angle = 2 * float(M_PI) * random_float(random);
    Float3 axis = fabsf(normal.z) < 0.999f ? Float3 {0, 0, 1} : Float3 {0, 1, 0};
    Float3 tangent = normalize(cross(axis, normal));
    Float3 bitangent = cross(normal, tangent);
    Float3 direction = tangent * (radius * cosf(angle))
         + bitangent * (radius * sinf(angle))
         + normal * sqrtf(fmaxf(0.0f, 1 - radius * radius));
    return direction;
}

HOST_DEVICE inline Float3 gamma_correct(Float3 color) {
    return {
        powf(color.x, 1.0f / 2.2f),
        powf(color.y, 1.0f / 2.2f),
        powf(color.z, 1.0f / 2.2f)
    };
}

// IO

inline std::string read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot read " << filename << '\n';
        std::exit(1);
    }

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

inline void write_ppm(const char* filename, const unsigned char* pixels, int width, int height) {
    std::ofstream image(filename, std::ios::binary);
    image << "P6\n" << width << ' ' << height << "\n255\n";
    image.write(reinterpret_cast<const char*>(pixels), width * height * 3);
    image.close();

    if (!image) {
        std::cerr << "Cannot write " << filename << '\n';
        std::exit(1);
    }

    std::cout << "Saved " << filename << '\n';
}

