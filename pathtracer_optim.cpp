#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

const int max_depth = 24;
const int samples_per_pixel = 128;
const int image_width = 320;

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
    return v * (1 / std::sqrt(dot(v, v)));
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

// Separate triangle coordinates let the compiler vectorize intersection work.
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
        for (int i = 0; i < int(packets.size()); ++i) {
            const TrianglePacket& packet = packets[i];
            float ts[8], us[8], vs[8];
            float closest = hit.t;

            // Compute independent candidates first, without early returns or
            // a loop-carried nearest-hit update. This is ordinary scalar C++.
            for (int lane = 0; lane < 8; ++lane) {
                Float3 e1{packet.e1x[lane], packet.e1y[lane], packet.e1z[lane]};
                Float3 e2{packet.e2x[lane], packet.e2y[lane], packet.e2z[lane]};
                Float3 h = cross(ray.direction, e2);
                float determinant = dot(e1, h);
                float inverse = 1 / std::max(determinant, 1e-10f);
                Float3 offset = ray.origin - Float3{packet.px[lane], packet.py[lane], packet.pz[lane]};
                float u = dot(offset, h) * inverse;
                Float3 q = cross(offset, e1);
                float v = dot(ray.direction, q) * inverse;
                float t = dot(e2, q) * inverse;
                // Bitwise AND evaluates every comparison without short-circuit branches.
                bool valid = (determinant >= 1e-10f) & (u >= 0) & (u <= 1)
                           & (v >= 0) & (u + v <= 1) & (t > 1e-4f) & (t < closest);
                ts[lane] = valid ? t : INFINITY;
                us[lane] = u;
                vs[lane] = v;
            }

            // Reduce in triangle order to preserve the original tie rule.
            for (int lane = 0; lane < 8; ++lane) {
                if (ts[lane] < hit.t) hit = {ts[lane], us[lane], vs[lane], 8 * i + lane};
            }
        }
        return hit.triangle >= 0;
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

    scene.pack_triangles();
    return scene;
}

float random_float() {
    static std::mt19937_64 engine{42};
    static std::uniform_real_distribution<float> distribution{0.0f, 1.0f};
    return distribution(engine);
}

Float2 random_float2() { return {random_float(), random_float()}; }

Float3 sample_hemisphere(Float3 normal) {
    Float2 sample = random_float2();
    float z = 2 * sample.x - 1;
    float phi = 2 * M_PI * sample.y;
    float r = std::sqrt(std::max(0.0f, 1 - z * z));
    Float3 direction{r * std::cos(phi), r * std::sin(phi), z};
    return dot(direction, normal) < 0 ? direction * -1 : direction;
}

Float3 pathtrace(const Scene& scene, Ray ray) {
    Float3 path_weight{1, 1, 1};

    for (int depth = 0; depth < max_depth; ++depth) {
        Hit hit;
        if (!scene.intersect(ray, hit)) return {};

        const Triangle& triangle = scene.triangles[hit.triangle];
        if (dot(triangle.emission, triangle.emission) > 0)
            return path_weight * triangle.emission;

        Float3 position = triangle.p + triangle.e1 * hit.u + triangle.e2 * hit.v;
        Float3 origin = position + triangle.normal * 1e-4f;

        Float3 direction = sample_hemisphere(triangle.normal);
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

                pixel_color += pathtrace(scene, ray);
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

    Scene scene = cornell_box();
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
