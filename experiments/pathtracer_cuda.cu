// Minimal CUDA Cornell path tracer: one thread per pixel.
#include <cmath>
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cuda_runtime.h>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

const int max_depth = 24;
const int samples_per_pixel = 128;
const int image_width = 320;

struct Float2 {
    float x, y;

    __host__ __device__ Float2(float x = 0, float y = 0) : x(x), y(y) {}
    __host__ __device__ Float2 operator+(Float2 b) const { return {x + b.x, y + b.y}; }
    __host__ __device__ Float2 operator-(Float2 b) const { return {x - b.x, y - b.y}; }
    __host__ __device__ Float2 operator*(Float2 b) const { return {x * b.x, y * b.y}; }
    __host__ __device__ Float2 operator*(float scale) const { return {x * scale, y * scale}; }
};

struct Float3 {
    float x, y, z;

    __host__ __device__ Float3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    __host__ __device__ Float3 operator+(Float3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    __host__ __device__ Float3 operator-(Float3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    __host__ __device__ Float3 operator*(Float3 b) const { return {x * b.x, y * b.y, z * b.z}; }
    __host__ __device__ Float3 operator*(float scale) const { return {x * scale, y * scale, z * scale}; }
    __host__ __device__ Float3 operator+=(Float3 b) { *this = *this + b; return *this; }
    __host__ __device__ Float3 operator*=(Float3 b) { *this = *this * b; return *this; }
};

__host__ __device__ float dot(Float3 a, Float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

__host__ __device__ Float3 cross(Float3 a, Float3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

__host__ __device__ Float3 normalize(Float3 v) {
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

    __device__ bool intersect(Ray ray, Hit& hit) const {
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

struct Scene {
    Triangle triangles[36];
    int triangle_count = 0;
    Camera camera;

    __device__ bool intersect(Ray ray, Hit& hit) const {
        hit = {};
        for (int i = 0; i < triangle_count; ++i) {
            if (triangles[i].intersect(ray, hit)) hit.triangle = i;
        }
        return hit.triangle >= 0;
    }
};

void add_quad(Scene& scene, Float3 p, Float3 u, Float3 v,
              Float3 color, Float3 emission = {}) {
    scene.triangles[scene.triangle_count++] = Triangle(p, p + u, p + u + v, color, emission);
    scene.triangles[scene.triangle_count++] = Triangle(p, p + u + v, p + v, color, emission);
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

    return scene;
}

// PCG XSH RR, adapted from M. E. O'Neill's Apache-2.0 minimal implementation.
// See licenses/PCG-NOTICE and licenses/Apache-2.0.txt.
struct Random {
    uint64_t state = 0, increment = 1;
    __device__ uint32_t next() {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + increment;
        uint32_t x = uint32_t(((old >> 18) ^ old) >> 27), rotate = old >> 59;
        return (x >> rotate) | (x << ((-rotate) & 31));
    }
    __device__ void seed(uint64_t stream) {
        increment = (stream << 1) | 1; state = 0;
        next(); state += 42; next();
    }
    __device__ float uniform() { return float(next() >> 8) * 0x1p-24f; }
};

__device__ Float3 sample_hemisphere(Float3 normal, Random& random) {
    float z = 2 * random.uniform() - 1;
    float phi = 2 * M_PI * random.uniform();
    float r = sqrtf(fmaxf(0.0f, 1 - z * z));
    Float3 direction{r * cosf(phi), r * sinf(phi), z};
    return dot(direction, normal) < 0 ? direction * -1 : direction;
}

__device__ Float3 pathtrace(const Scene& scene, Ray ray, Random& random) {
    Float3 path_weight{1, 1, 1};

    for (int depth = 0; depth < max_depth; ++depth) {
        Hit hit;
        if (!scene.intersect(ray, hit)) return {};

        const Triangle& triangle = scene.triangles[hit.triangle];
        if (dot(triangle.emission, triangle.emission) > 0)
            return path_weight * triangle.emission;

        Float3 position = triangle.p + triangle.e1 * hit.u + triangle.e2 * hit.v;
        Float3 origin = position + triangle.normal * 1e-4f;

        Float3 direction = sample_hemisphere(triangle.normal, random);
        float cosine = fmaxf(0.0f, dot(triangle.normal, direction));
        path_weight *= triangle.albedo * (2 * cosine);

        ray = {origin, direction};
    }
    return {};
}

__global__ void render(const Scene* scene, int samples, int width, int height,
                       float* pixels) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;

    const Camera& camera = scene->camera;
    float focal_length = 1 / tanf(camera.fovy * 0.5f * M_PI / 180);
    Float3 pixel_color{};

    for (int s = 0; s < samples; ++s) {
        Random random;
        random.seed((uint64_t(y * width + x) << 32) | uint32_t(s));
        Float2 sample_pos = Float2{float(x), float(y)}
                         + Float2{random.uniform(), random.uniform()} * 0.5f;
        Float2 uv = sample_pos * Float2{1.0f / width, 1.0f / height};
        Float3 direction = camera.u * (2 * uv.x - 1) * camera.aspect
                         + camera.v * (-2 * uv.y + 1)
                         + camera.w * focal_length;
        Ray ray{camera.position, normalize(direction)};
        pixel_color += pathtrace(*scene, ray, random);
    }

    pixel_color = pixel_color * (1.0f / samples);
    size_t i = 3 * (size_t(y) * width + x);
    pixels[i] = pixel_color.x;
    pixels[i + 1] = pixel_color.y;
    pixels[i + 2] = pixel_color.z;
}

void cuda_check(cudaError_t error) {
    if (error != cudaSuccess) {
        std::cerr << "CUDA: " << cudaGetErrorString(error) << '\n';
        std::exit(1);
    }
}

int main(int argc, char** argv) {
    int samples = samples_per_pixel;
    int width = image_width;
    std::string filename = argc > 3 ? argv[3] : "build/cuda.hdr";

    try {
        if (argc > 1) samples = std::stoi(argv[1]);
        if (argc > 2) width = std::stoi(argv[2]);
    } catch (const std::exception&) {
        std::cerr << "Usage: " << argv[0] << " [samples] [width] [output.hdr]\n";
        return 1;
    }

    if (samples < 1 || width < 1 || width > 16384 || argc > 4) {
        std::cerr << "Samples must be positive; width must be between 1 and 16384.\n";
        return 1;
    }

    Scene scene = cornell_box();
    int height = int(width / scene.camera.aspect);
    size_t pixel_count = size_t(width) * height * 3;
    std::vector<float> pixels(pixel_count);
    Scene* device_scene = nullptr;
    float* device_pixels = nullptr;
    cuda_check(cudaMalloc(&device_scene, sizeof(Scene)));
    cuda_check(cudaMalloc(&device_pixels, pixel_count * sizeof(float)));
    cuda_check(cudaMemcpy(device_scene, &scene, sizeof(Scene), cudaMemcpyHostToDevice));
    std::cerr << "Rendering " << width << 'x' << height << " at " << samples << " spp\n";
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
    render<<<grid, block>>>(device_scene, samples, width, height, device_pixels);
    cuda_check(cudaGetLastError());
    cuda_check(cudaDeviceSynchronize());
    cuda_check(cudaMemcpy(pixels.data(), device_pixels, pixel_count * sizeof(float),
                          cudaMemcpyDeviceToHost));
    cuda_check(cudaFree(device_pixels));
    cuda_check(cudaFree(device_scene));
    if (!stbi_write_hdr(filename.c_str(), width, height, 3, pixels.data())) {
        std::cerr << "Cannot write " << filename << '\n';
        return 1;
    }
    std::cerr << "Saved " << filename << '\n';
    return 0;
}
