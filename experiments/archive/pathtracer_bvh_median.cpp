#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include <Eigen/Geometry>

using Eigen::Vector2f;
using Eigen::Vector3f;

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

const int max_depth = 24;
const int samples_per_pixel = 32;
const int image_width = 768;

struct Ray { Vector3f origin, direction; };

struct Hit {
    float t = INFINITY, u = 0, v = 0;
    int triangle = -1;
};

struct Triangle {
    Vector3f p, e1, e2;
    Vector3f normal;
    Vector3f albedo, emission;

    Triangle(Vector3f p, Vector3f a, Vector3f b, Vector3f color, Vector3f light = Vector3f::Zero())
        : p(p), e1(a - p), e2(b - p),
          normal(e1.cross(e2).normalized()),
          albedo(color), emission(light) {}

    bool intersect(Ray ray, Hit& hit) const {
        Vector3f h = ray.direction.cross(e2);
        float determinant = e1.dot(h);
        if (determinant < 1e-10f) return false;

        float inverse = 1 / determinant;
        Vector3f offset = ray.origin - p;
        float u = offset.dot(h) * inverse;
        if (u < 0 || u > 1) return false;

        Vector3f q = offset.cross(e1);
        float v = ray.direction.dot(q) * inverse;
        if (v < 0 || u + v > 1) return false;

        float t = e2.dot(q) * inverse;
        if (t <= 1e-4f || t >= hit.t) return false;
        hit.t = t;
        hit.u = u;
        hit.v = v;
        return true;
    }
};

struct Camera {
    Vector3f position, target, up;
    float fovy, aspect;
    Vector3f u = Vector3f::Zero(), v = Vector3f::Zero(), w = Vector3f::Zero();

    void compute_basis() {
        w = (target - position).normalized();
        u = up.cross(w).normalized();
        v = w.cross(u);
    }
};

struct Bounds {
    Vector3f lo {INFINITY, INFINITY, INFINITY};
    Vector3f hi {-INFINITY, -INFINITY, -INFINITY};

    void grow(Vector3f p) {
        lo = lo.cwiseMin(p);
        hi = hi.cwiseMax(p);
    }

    // Intersect the ray interval with the slab between each pair of box faces.
    bool intersect(Ray ray, float limit) const {
        double near = 1e-4f, far = limit;
        for (int axis = 0; axis < 3; ++axis) {
            // Round bounds outward; handle parallel rays without 0 * infinity.
            double a = std::nextafter(lo[axis], -INFINITY);
            double b = std::nextafter(hi[axis], INFINITY);
            double o = ray.origin[axis], d = ray.direction[axis];
            if (d == 0) {
                if (o < a || o > b) return false;
                continue;
            }
            double t0 = (a - o) / d, t1 = (b - o) / d;
            if (t0 > t1) std::swap(t0, t1);
            near = std::max(near, t0);
            far = std::min(far, t1);
            if (near > far) return false;
        }
        return true;
    }
};

struct BVHNode {
    Bounds bounds;
    int left = -1, right = -1; // Internal node: two child indices.
    int first = 0, count = 0;  // Leaf: a range in indices (count > 0).
};

struct Scene {

    std::vector<Triangle> triangles;
    std::vector<int> indices; // Reorder IDs during construction, not triangles.
    std::vector<BVHNode> nodes;
    Camera camera;
    Vector3f light_position = Vector3f::Zero();
    Vector3f light_energy = Vector3f::Zero(); // Zero energy selects the original path tracer.

    int build_node(int first, int count) {
        int index = int(nodes.size());
        nodes.emplace_back();
        Bounds bounds;
        for (int i = first; i < first + count; ++i) {
            const Triangle& t = triangles[indices[i]];
            bounds.grow(t.p);
            bounds.grow(t.p + t.e1);
            bounds.grow(t.p + t.e2);
        }
        nodes[index].bounds = bounds;

        // Stop splitting when a leaf has only a few triangles.
        if (count <= 4) {
            nodes[index].first = first;
            nodes[index].count = count;
            return index;
        }

        // Sort triangle centers along the box's longest axis; split in half.
        Vector3f extent = bounds.hi - bounds.lo;
        int axis = extent.y() > extent.x() ? 1 : 0;
        if (extent.z() > extent[axis]) axis = 2;
        std::sort(indices.begin() + first, indices.begin() + first + count,
                  [&](int a, int b) {
            const Triangle& x = triangles[a];
            const Triangle& y = triangles[b];
            float ca = (x.p + (x.e1 + x.e2) * (1.0f / 3))[axis];
            float cb = (y.p + (y.e1 + y.e2) * (1.0f / 3))[axis];
            return ca == cb ? a < b : ca < cb;
        });
        int middle = first + count / 2;
        int left = build_node(first, middle - first);
        int right = build_node(middle, first + count - middle);
        // Recursion can reallocate nodes, so keep an index rather than a reference.
        nodes[index].left = left;
        nodes[index].right = right;
        return index;
    }

    // Rebuild after adding or modifying geometry. Triangle order stays intact.
    void build_bvh() {
        indices.resize(triangles.size());
        std::iota(indices.begin(), indices.end(), 0);
        nodes.clear();
        if (!triangles.empty()) build_node(0, int(triangles.size()));
    }

    void intersect_node(int index, Ray ray, Hit& hit) const {
        const BVHNode& node = nodes[index];
        // A box miss skips every triangle below this node.
        if (!node.bounds.intersect(ray, hit.t)) return;
        if (node.count) {
            for (int i = node.first; i < node.first + node.count; ++i) {
                int id = indices[i];
                // Preserve the original renderer's input-order rule for ties.
                Hit candidate;
                if (triangles[id].intersect(ray, candidate)
                    && (candidate.t < hit.t || (candidate.t == hit.t && id < hit.triangle))) {
                    hit = candidate;
                    hit.triangle = id;
                }
            }
        } else {
            intersect_node(node.left, ray, hit);
            intersect_node(node.right, ray, hit);
        }
    }

    bool intersect(Ray ray, Hit& hit) const {
        hit = {};
        if (!nodes.empty()) intersect_node(0, ray, hit);
        return hit.triangle >= 0;
    }
};

#include "../../utils_obj.hpp"

void add_quad(Scene& scene, Vector3f p, Vector3f u, Vector3f v,
              Vector3f color, Vector3f emission = Vector3f::Zero()) {
    scene.triangles.emplace_back(p, p + u, p + u + v, color, emission);
    scene.triangles.emplace_back(p, p + u + v, p + v, color, emission);
}

void add_block(Scene& scene, Vector3f center, Vector3f size, float degrees,
               Vector3f color, Vector3f emission = Vector3f::Zero()) {
    float angle = degrees * M_PI / 180;
    float c = std::cos(angle), s = std::sin(angle);
    Vector3f u{size.x() * c, 0, -size.x() * s};
    Vector3f v{size.z() * s, 0, size.z() * c};
    Vector3f up{0, size.y(), 0};
    Vector3f p = center - (u + v + up) * 0.5f;

    add_quad(scene, p, up, u, color, emission);
    add_quad(scene, p + v, u, up, color, emission);
    add_quad(scene, p, v, up, color, emission);
    add_quad(scene, p + u, up, v, color, emission);
    add_quad(scene, p + up, v, u, color, emission);
    add_quad(scene, p, u, v, color, emission);
}

Scene cornell_box(const char* mesh = nullptr) {
    Scene scene;
    scene.camera = {
        {278, 278, -800}, {278, 278, 278}, {0, 1, 0}, 40, 1.0f
    };
    scene.camera.compute_basis();

    Vector3f white{0.73f, 0.73f, 0.73f};
    Vector3f red{0.65f, 0.05f, 0.05f};
    Vector3f green{0.12f, 0.45f, 0.15f};

    add_quad(scene, {0, 0, 0}, {0, 555, 0}, {0, 0, 555}, red);
    add_quad(scene, {555, 0, 0}, {0, 0, 555}, {0, 555, 0}, green);
    add_quad(scene, {0, 0, 0}, {0, 0, 555}, {555, 0, 0}, white);
    add_quad(scene, {0, 555, 0}, {555, 0, 0}, {0, 0, 555}, white);
    add_quad(scene, {0, 0, 555}, {0, 555, 0}, {555, 0, 0}, white);

    add_quad(scene, {213, 554, 227}, {130, 0, 0}, {0, 0, 105}, Vector3f::Zero(), {15, 15, 15});

    if (mesh) add_mesh(scene, mesh, white);
    else {
        add_block(scene, {370, 82.5f, 169}, {165, 165, 165}, 18, white);
        add_block(scene, {187, 165, 351}, {165, 330, 165}, -15, white);
    }
    scene.build_bvh();

    return scene;
}

// Camera, diffuse material and point light from Nori scenes/pa3/ajax-simple.xml.
Scene ajax_scene(const char* mesh) {
    Scene scene;
    scene.camera = {
        {-65.6055f, 47.5762f, 24.3583f},
        {-64.8161f, 47.2211f, 23.8576f},
        {0.299858f, 0.934836f, -0.190177f}, 30, 1.0f
    };
    scene.camera.compute_basis();
    // Nori maps the left edge of the image to positive camera-space x.
    scene.camera.u = scene.camera.u * -1;
    scene.light_position = {-20, 40, 20};
    scene.light_energy = {3.76e4f, 3.76e4f, 3.76e4f};
    add_mesh(scene, mesh, {0.5f, 0.5f, 0.5f});
    scene.build_bvh();
    return scene;
}

float random_float() {
    static std::mt19937_64 engine{42};
    static std::uniform_real_distribution<float> distribution{0.0f, 1.0f};
    return distribution(engine);
}

Vector2f random_float2() { return {random_float(), random_float()}; }

Vector3f sample_hemisphere(Vector3f normal) {
    Vector2f sample = random_float2();
    float z = 2 * sample.x() - 1;
    float phi = 2 * M_PI * sample.y();
    float r = std::sqrt(std::max(0.0f, 1 - z * z));
    Vector3f direction{r * std::cos(phi), r * std::sin(phi), z};
    if (direction.dot(normal) < 0) direction = -direction;
    return direction;
}

Vector3f pathtrace(const Scene& scene, Ray ray) {
    // Nori's simple integrator: one diffuse hit and one point-light shadow ray.
    if (scene.light_energy.squaredNorm() > 0) {
        Hit hit;
        if (!scene.intersect(ray, hit)) return Vector3f::Zero();
        const Triangle& triangle = scene.triangles[hit.triangle];
        Vector3f position = triangle.p + triangle.e1 * hit.u + triangle.e2 * hit.v;
        Vector3f origin = position + triangle.normal * 1e-4f;
        Vector3f to_light = scene.light_position - origin;
        float distance_squared = to_light.squaredNorm();
        float distance = std::sqrt(distance_squared);
        Vector3f direction = to_light * (1 / distance);
        float cosine = std::max(0.0f, triangle.normal.dot(direction));
        if (cosine == 0) return Vector3f::Zero();
        Hit shadow;
        if (scene.intersect({origin, direction}, shadow) && shadow.t < distance - 1e-4f)
            return Vector3f::Zero();
        // Isotropic point power / (4*pi*r^2), multiplied by Lambertian albedo/pi.
        return triangle.albedo.cwiseProduct(scene.light_energy)
             * (cosine / (4 * M_PI * M_PI * distance_squared));
    }

    Vector3f path_weight{1, 1, 1};

    for (int depth = 0; depth < max_depth; ++depth) {
        Hit hit;
        if (!scene.intersect(ray, hit)) return Vector3f::Zero();

        const Triangle& triangle = scene.triangles[hit.triangle];
        if (triangle.emission.squaredNorm() > 0)
            return path_weight.cwiseProduct(triangle.emission);

        Vector3f position = triangle.p + triangle.e1 * hit.u + triangle.e2 * hit.v;
        Vector3f origin = position + triangle.normal * 1e-4f;

        Vector3f direction = sample_hemisphere(triangle.normal);
        float cosine = std::max(0.0f, triangle.normal.dot(direction));
        path_weight = path_weight.cwiseProduct(triangle.albedo * (2 * cosine));

        ray = {origin, direction};
    }
    return Vector3f::Zero();
}

void render(const Scene& scene, int samples, int width, int height, float* pixels) {
    const Camera& camera = scene.camera;
    float focal_length = 1 / std::tan(camera.fovy * 0.5f * M_PI / 180);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Vector3f pixel_color = Vector3f::Zero();

            for (int s = 0; s < samples; ++s) {
                Vector2f sample_pos = Vector2f{float(x), float(y)} + random_float2() * 0.5f;
                Vector2f sample_uv = sample_pos.cwiseProduct(Vector2f{1.0f / width, 1.0f / height});

                Vector3f direction = camera.u * (2 * sample_uv.x() - 1) * camera.aspect
                                 + camera.v * (-2 * sample_uv.y() + 1)
                                 + camera.w * focal_length;
                Ray ray{camera.position, direction.normalized()};

                pixel_color += pathtrace(scene, ray);
            }

            pixel_color = pixel_color * (1.0f / samples);
            size_t i = 3 * (size_t(y) * width + x);
            std::memcpy(pixels + i, pixel_color.data(), 3 * sizeof(float));
        }
    }
}

int main(int argc, char** argv) {
    int samples = samples_per_pixel;
    int width = image_width;
    std::string filename = argc > 3 ? argv[3] : "build/ajax.hdr";

    try {
        if (argc > 1) samples = std::stoi(argv[1]);
        if (argc > 2) width = std::stoi(argv[2]);
    } catch (const std::exception&) {
        std::cerr << "Usage: " << argv[0] << " [samples] [width] [output.hdr] [mesh.obj]\n";
        return 1;
    }

    if (samples < 1 || width < 1 || width > 16384 || argc > 5) {
        std::cerr << "Samples must be positive; width must be between 1 and 16384.\n";
        return 1;
    }

    Scene scene;
    try {
        scene = ajax_scene(argc > 4 ? argv[4] : "../nori/scenes/pa2/ajax.obj");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    std::cerr << scene.triangles.size() << " triangles, " << scene.nodes.size() << " BVH nodes\n";
    int height = int(width / scene.camera.aspect);
    std::vector<float> pixels(size_t(width) * height * 3);
    std::cerr << "Rendering " << width << 'x' << height << " at " << samples << " spp\n";
    render(scene, samples, width, height, pixels.data());
    if (!stbi_write_hdr(filename.c_str(), width, height, 3, pixels.data())) {
        std::cerr << "Cannot write " << filename << '\n';
        return 1;
    }
    std::cerr << "Saved " << filename << '\n';
    return 0;
}
