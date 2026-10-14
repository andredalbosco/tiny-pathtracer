// Uniform Cornell path tracer, in the style of pathtracer.cpp.
// C arrays, PCG, compiler-generated SIMD and OpenMP; no STL.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new> // placement construction only
#include <omp.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "stb_image_write.h"
#pragma GCC diagnostic pop

const int max_depth = 24;
const int samples_per_pixel = 128;
const int image_width = 320;
float maximum(float a, float b) { return a < b ? b : a; }

struct Float2 {
    float x, y;

    Float2(float x = 0, float y = 0) : x(x), y(y) {}
    Float2 operator+(Float2 b) const { return {x + b.x, y + b.y}; }
    Float2 operator-(Float2 b) const { return {x - b.x, y - b.y}; }
    Float2 operator*(Float2 b) const { return {x * b.x, y * b.y}; }
    Float2 operator*(float scale) const { return {x * scale, y * scale}; }
};

struct Float3 {
    float x, y, z;

    Float3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    Float3 operator+(Float3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    Float3 operator-(Float3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    Float3 operator*(Float3 b) const { return {x * b.x, y * b.y, z * b.z}; }
    Float3 operator*(float scale) const { return {x * scale, y * scale, z * scale}; }
    Float3 operator+=(Float3 b) { *this = *this + b; return *this; }
    Float3 operator*=(Float3 b) { *this = *this * b; return *this; }
};

float dot(Float3 a, Float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Float3 cross(Float3 a, Float3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Float3 normalize(Float3 v) {
    return v * (1 / sqrtf(dot(v, v)));
}

struct Ray { Float3 origin, direction; };

struct Hit {
    float t = INFINITY, u = 0, v = 0;
    int triangle = -1;
};

struct Triangle {
    Float3 p, e1, e2;
    Float3 normal;
    Float3 albedo, emission;

    Triangle() = default;
    Triangle(Float3 p, Float3 a, Float3 b, Float3 color, Float3 light = {})
        : p(p), e1(a - p), e2(b - p),
          normal(normalize(cross(e1, e2))),
          albedo(color), emission(light) {}


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

struct Float3Array {
    float x[64], y[64], z[64];
    Float3 get(int i) const { return {x[i], y[i], z[i]}; }
    void set(int i, Float3 v) { x[i] = v.x; y[i] = v.y; z[i] = v.z; }
};

struct Triangles {
    struct Surface { Float3 normal, albedo, emission; };
    Surface surfaces[64];
    Float3Array p, e1, e2;

    void set(int i, const Triangle& t) {
        p.set(i, t.p); e1.set(i, t.e1); e2.set(i, t.e2);
        surfaces[i] = {t.normal, t.albedo, t.emission};
    }
    Triangle operator[](int i) const {
        Triangle t;
        t.p = p.get(i); t.e1 = e1.get(i); t.e2 = e2.get(i);
        t.normal = surfaces[i].normal; t.albedo = surfaces[i].albedo;
        t.emission = surfaces[i].emission;
        return t;
    }
};

struct Scene {
    Triangles triangles;
    int count = 0;
    Camera camera;

    // Test one triangle against eight rays. Clang vectorizes the inner loop.
    void intersect(const Ray* rays, Hit* hits) const {
        float ox[8], oy[8], oz[8], dx[8], dy[8], dz[8], ts[8], us[8]{}, vs[8]{};
        int ids[8];
        for (int r = 0; r < 8; ++r) {
            ox[r] = rays[r].origin.x; oy[r] = rays[r].origin.y; oz[r] = rays[r].origin.z;
            dx[r] = rays[r].direction.x; dy[r] = rays[r].direction.y; dz[r] = rays[r].direction.z;
            ts[r] = INFINITY; ids[r] = -1;
        }
        for (int i = 0; i < count; ++i) {
            Triangle triangle = triangles[i];
            for (int r = 0; r < 8; ++r) {
                Float3 direction{dx[r], dy[r], dz[r]};
                Float3 h = cross(direction, triangle.e2);
                float determinant = dot(triangle.e1, h);
                float inverse = 1 / maximum(determinant, 1e-10f);
                Float3 offset = Float3{ox[r], oy[r], oz[r]} - triangle.p;
                float u = dot(offset, h) * inverse;
                Float3 q = cross(offset, triangle.e1);
                float v = dot(direction, q) * inverse, t = dot(triangle.e2, q) * inverse;
                bool valid = (determinant >= 1e-10f) & (u >= 0) & (u <= 1) & (v >= 0)
                           & (u + v <= 1) & (t > 1e-4f) & (t < ts[r]);
                ts[r] = valid ? t : ts[r]; us[r] = valid ? u : us[r];
                vs[r] = valid ? v : vs[r]; ids[r] = valid ? i : ids[r];
            }
        }
        for (int r = 0; r < 8; ++r) hits[r] = {ts[r], us[r], vs[r], ids[r]};
    }
};

void add_quad(Scene& scene, Float3 p, Float3 u, Float3 v,
              Float3 color, Float3 emission = {}) {
    if (scene.count > 62) abort();
    scene.triangles.set(scene.count++, Triangle(p, p + u, p + u + v, color, emission));
    scene.triangles.set(scene.count++, Triangle(p, p + u + v, p + v, color, emission));
}

void add_block(Scene& scene, Float3 center, Float3 size, float degrees,
               Float3 color, Float3 emission = {}) {
    float angle = degrees * M_PI / 180;
    float c = cosf(angle), s = sinf(angle);
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

Scene cornell_box() {
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

    return scene;
}

// PCG XSH RR, adapted from M. E. O'Neill's Apache-2.0 minimal implementation.
// See licenses/PCG-NOTICE and licenses/Apache-2.0.txt.
struct Random {
    uint64_t state = 0, increment = 1;
    uint32_t next() {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + increment;
        uint32_t x = uint32_t(((old >> 18) ^ old) >> 27), rotate = old >> 59;
        return (x >> rotate) | (x << ((-rotate) & 31));
    }
    void seed(uint64_t stream) {
        increment = (stream << 1) | 1; state = 0;
        next(); state += 42; next();
    }
    float uniform() { return float(next() >> 8) * 0x1p-24f; }
};

Float3 sample_hemisphere(Float3 normal, Random& random) {
    float z = 2 * random.uniform() - 1, phi = 2 * M_PI * random.uniform();
    float r = sqrtf(maximum(0, 1 - z * z)), sine, cosine;
#ifdef __GLIBC__
    __builtin_sincosf(phi, &sine, &cosine);
#else
    sine = sinf(phi); cosine = cosf(phi);
#endif
    Float3 direction{r * cosine, r * sine, z};
    return dot(direction, normal) < 0 ? direction * -1 : direction;
}

struct Path {
    Ray ray;
    Random random;
    Float3 weight{1, 1, 1}, color{};
    int sample = 0, depth = 0;
    bool active = false;
};

Path camera_path(const Camera& camera, int x, int y, int sample,
                 int width, int height, float focal_length) {
    Path path;
    path.active = true; path.sample = sample;
    path.random.seed((uint64_t(y * width + x) << 32) | uint32_t(sample));
    Float2 sample_pos = Float2{float(x), float(y)}
                     + Float2{path.random.uniform(), path.random.uniform()} * 0.5f;
    Float2 uv = sample_pos * Float2{1.0f / width, 1.0f / height};
    Float3 direction = camera.u * (2 * uv.x - 1) * camera.aspect
                     + camera.v * (-2 * uv.y + 1) + camera.w * focal_length;
    path.ray = {camera.position, normalize(direction)};
    return path;
}

Float3 pathtrace(const Scene& scene, int x, int y, int samples, int width, int height,
                float focal_length, Float3* sample_colors) {
    Path paths[8];
    int next_sample = 0;
    for (int r = 0; r < 8 && next_sample < samples; ++r)
        paths[r] = camera_path(scene.camera, x, y, next_sample++, width, height, focal_length);

    for (;;) {
        Ray rays[8]{}; Hit hits[8]; int live = 0;
        for (int r = 0; r < 8; ++r) if (paths[r].active) {
            rays[r] = paths[r].ray; ++live;
        }
        if (!live) break;
        scene.intersect(rays, hits);

        for (int r = 0; r < 8; ++r) {
            Path path = paths[r];
            if (!path.active) continue;
            Hit hit = hits[r];
            if (hit.triangle < 0) path.active = false;
            else {
                Triangle triangle = scene.triangles[hit.triangle];
                if (dot(triangle.emission, triangle.emission) > 0) {
                    path.color = path.weight * triangle.emission;
                    path.active = false;
                } else {
                    Float3 position = triangle.p + triangle.e1 * hit.u + triangle.e2 * hit.v;
                    Float3 direction = sample_hemisphere(triangle.normal, path.random);
                    path.weight *= triangle.albedo * (2 * maximum(0, dot(triangle.normal, direction)));
                    path.ray = {position + triangle.normal * 1e-4f, direction};
                }
            }
            if (++path.depth == max_depth) path.active = false;
            if (!path.active) {
                sample_colors[path.sample] = path.color;
                if (next_sample < samples)
                    path = camera_path(scene.camera, x, y, next_sample++, width, height, focal_length);
            }
            paths[r] = path;
        }
    }
    Float3 pixel_color{};
    for (int s = 0; s < samples; ++s) pixel_color += sample_colors[s];
    return pixel_color * (1.0f / samples);
}

bool render(const Scene& scene, int samples, int width, int height, float* pixels) {
    float focal_length = 1 / tan(scene.camera.fovy * 0.5f * M_PI / 180);
    int failed = 0;
#pragma omp parallel
    {
        Float3* sample_colors = (Float3*)malloc(size_t(samples) * sizeof(Float3));
        if (!sample_colors) {
#pragma omp atomic write
            failed = 1;
        } else for (int s = 0; s < samples; ++s) ::new (sample_colors + s) Float3;
#pragma omp for collapse(2) schedule(dynamic, 16)
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (!sample_colors) continue;
                Float3 color = pathtrace(scene, x, y, samples, width, height, focal_length, sample_colors);
                memcpy(pixels + 3 * (size_t(y) * width + x), &color, sizeof(color));
            }
        }
        free(sample_colors);
    }
    return !failed;
}

#ifndef PATHTRACER_CLANG_NO_MAIN
int main(int argc, char** argv) {
    int samples = argc > 1 ? atoi(argv[1]) : samples_per_pixel;
    int width = argc > 2 ? atoi(argv[2]) : image_width;
    const char* filename = argc > 3 ? argv[3] : "build/clang.hdr";
    int threads = argc > 4 ? atoi(argv[4]) : omp_get_max_threads();
    if (samples < 1 || width < 1 || width > 32768 || threads < 1 || argc > 5) {
        fprintf(stderr, "Usage: %s [samples] [width] [output.hdr] [threads]\n", argv[0]);
        return 1;
    }
    omp_set_dynamic(0); omp_set_num_threads(threads);
    Scene scene = cornell_box();
    int height = int(width / scene.camera.aspect);
    float* pixels = (float*)calloc(size_t(width) * height * 3, sizeof(float));
    if (!pixels) { perror("pixels"); return 1; }
    fprintf(stderr, "Rendering %dx%d at %d spp with %d threads\n", width, height, samples, threads);
    bool rendered = render(scene, samples, width, height, pixels);
    int written = rendered && stbi_write_hdr(filename, width, height, 3, pixels);
    free(pixels);
    if (!written) { fprintf(stderr, "Cannot render or write %s\n", filename); return 1; }
    fprintf(stderr, "Saved %s\n", filename);
    return 0;
}
#endif
