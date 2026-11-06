#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <Eigen/Geometry>

using Eigen::Vector2f;
using Eigen::Vector3f;
#include "utils_image.hpp"

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
    int triangle = -1;        // Leaf: one triangle in the original input order.

    bool is_leaf() const { return triangle >= 0; }
};

struct Scene {

    std::vector<Triangle> triangles;
    std::vector<BVHNode> nodes;
    Camera camera;

    int build_node(int first, int last) {
        int index = int(nodes.size());
        nodes.emplace_back();
        Bounds bounds;
        for (int i = first; i < last; ++i) {
            const Triangle& t = triangles[i];
            bounds.grow(t.p);
            bounds.grow(t.p + t.e1);
            bounds.grow(t.p + t.e2);
        }
        nodes[index].bounds = bounds;

        if (last - first == 1) {
            nodes[index].triangle = first;
        } else {
            int middle = first + (last - first) / 2;
            int left = build_node(first, middle);
            int right = build_node(middle, last);
            // Recursion can reallocate nodes; keep indices rather than references.
            nodes[index].left = left;
            nodes[index].right = right;
        }
        return index;
    }

    // Rebuild after geometry changes. Split input ranges without reordering them.
    void build_bvh() {
        nodes.clear();
        if (!triangles.empty()) build_node(0, int(triangles.size()));
    }

    void intersect_node(int index, Ray ray, Hit& hit) const {
        const BVHNode& node = nodes[index];
        if (!node.bounds.intersect(ray, hit.t)) return;

        if (node.is_leaf()) {
            // Left-first traversal visits triangles in input order, preserving ties.
            if (triangles[node.triangle].intersect(ray, hit))
                hit.triangle = node.triangle;
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

#include "utils_obj.hpp"

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

// Camera and diffuse material from Nori scenes/pa3/ajax-simple.xml.
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
    add_mesh(scene, mesh, {0.5f, 0.5f, 0.5f});
    // Downward-facing area light at the former point-light position.
    add_quad(scene, {-25, 40, 15}, {10, 0, 0}, {0, 0, 10},
             Vector3f::Zero(), {120, 120, 120});
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
