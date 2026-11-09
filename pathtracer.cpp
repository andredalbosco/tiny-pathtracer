#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "utils.h"

const int max_depth = 24;
const int samples_per_pixel = 128;
const int image_width = 320;

struct Scene {
    std::vector<Triangle> triangles;
    Camera camera;

    bool intersect(Ray ray, Hit& hit) const {
        hit = {};
        for (int i = 0; i < int(triangles.size()); ++i) {
            if (triangles[i].intersect(ray, hit)) hit.triangle_index = i;
        }
        return hit.triangle_index >= 0;
    }
};

Float3 trace_path(const Scene& scene, Ray ray, pcg32& random) {
    Float3 path_weight {1, 1, 1};

    for (int depth = 0; depth < max_depth; ++depth) {
        Hit hit;
        if (!scene.intersect(ray, hit)) return {};

        const Triangle& triangle = scene.triangles[hit.triangle_index];
        if (dot(triangle.emission, triangle.emission) > 0)
            return path_weight * triangle.emission;

        Float3 position = triangle.p + triangle.e1 * hit.u + triangle.e2 * hit.v;
        Float3 origin = position + triangle.normal * 1e-4f;

        Float3 direction = sample_uniform_hemisphere(triangle.normal, random);
        float cosine = std::max(0.0f, dot(triangle.normal, direction));
        path_weight *= triangle.albedo * (2 * cosine);

        ray = {origin, direction};
    }
    return {};
}

void render(const Scene& scene, int samples, int width, int height, float* pixels) {
    const Camera& camera = scene.camera;

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic, 1)
#endif
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Float3 pixel_color {};
            std::uint64_t pixel_index = std::uint64_t(y) * width + x;

            for (int s = 0; s < samples; ++s) {
                std::uint64_t stream_seed = (pixel_index << 32) | std::uint64_t(s);
                pcg32 random(42, stream_seed);

                Float2 sample_pos = Float2 {float(x), float(y)} + 0.5 * random_float2(random);
                Float2 sample_uv = sample_pos * Float2 {1.0f / width, 1.0f / height};

                Ray ray = camera.sample_ray(sample_uv);

                pixel_color += trace_path(scene, ray, random);
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

    Scene scene {};
    scene.triangles = make_cornell_box();
    scene.camera = make_camera({278, 278, -800}, {278, 278, 278}, {0, 1, 0}, 40);
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
