#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <fstream>
#include <numeric>
#include <sstream>
#include <stdexcept>
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

float component(Float3 v, int axis) {
    return axis == 0 ? v.x : axis == 1 ? v.y : v.z;
}

struct Bounds {
    Float3 lo{INFINITY, INFINITY, INFINITY}, hi{-INFINITY, -INFINITY, -INFINITY};

    void grow(Float3 p) {
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }

    bool intersect(Ray ray, float limit) const {
        double near = 1e-4f, far = limit;
        for (int axis = 0; axis < 3; ++axis) {
            // Round bounds outward; handle parallel rays without 0 * infinity.
            double a = std::nextafter(component(lo, axis), -INFINITY);
            double b = std::nextafter(component(hi, axis), INFINITY);
            double o = component(ray.origin, axis), d = component(ray.direction, axis);
            if (d == 0) {
                if (o < a || o > b) return false;
                continue;
            }
            double t0 = (a - o) / d, t1 = (b - o) / d;
            if (t0 > t1) std::swap(t0, t1);
            near = std::max(near, t0); far = std::min(far, t1);
            if (near > far) return false;
        }
        return true;
    }
};

struct Scene {
    struct Node {
        Bounds bounds;
        int left = -1, right = -1, first = 0, count = 0, axis = 0;
    };
    std::vector<Triangle> triangles;
    std::vector<int> indices;
    std::vector<Node> nodes;
    Camera camera;

    int build_node(int first, int count) {
        int index = int(nodes.size());
        nodes.emplace_back();
        Bounds bounds, centers;
        for (int i = first; i < first + count; ++i) {
            const Triangle& t = triangles[indices[i]];
            bounds.grow(t.p); bounds.grow(t.p + t.e1); bounds.grow(t.p + t.e2);
            centers.grow(t.p + (t.e1 + t.e2) * (1.0f / 3));
        }
        nodes[index].bounds = bounds;
        if (count <= 4) {
            nodes[index].first = first; nodes[index].count = count;
        } else {
            Float3 extent = centers.hi - centers.lo;
            int axis = extent.y > extent.x ? 1 : 0;
            if (extent.z > component(extent, axis)) axis = 2;
            int middle = first + count / 2;
            std::nth_element(indices.begin() + first, indices.begin() + middle,
                             indices.begin() + first + count, [&](int a, int b) {
                const Triangle& x = triangles[a]; const Triangle& y = triangles[b];
                float ca = component(x.p + (x.e1 + x.e2) * (1.0f / 3), axis);
                float cb = component(y.p + (y.e1 + y.e2) * (1.0f / 3), axis);
                return ca == cb ? a < b : ca < cb;
            });
            int left = build_node(first, middle - first);
            int right = build_node(middle, first + count - middle);
            nodes[index].left = left; nodes[index].right = right; nodes[index].axis = axis;
        }
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
        const Node& node = nodes[index];
        if (!node.bounds.intersect(ray, hit.t)) return;
        if (node.count) {
            for (int i = node.first; i < node.first + node.count; ++i) {
                int id = indices[i];
                Hit candidate;
                if (triangles[id].intersect(ray, candidate)
                    && (candidate.t < hit.t || (candidate.t == hit.t && id < hit.triangle))) {
                    hit = candidate; hit.triangle = id;
                }
            }
        } else {
            bool reverse = component(ray.direction, node.axis) < 0;
            intersect_node(reverse ? node.right : node.left, ray, hit);
            intersect_node(reverse ? node.left : node.right, ray, hit);
        }
    }
    bool intersect(Ray ray, Hit& hit) const {
        hit = {};
        if (!nodes.empty()) intersect_node(0, ray, hit);
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

// Position-only OBJ loading; flat diffuse triangles, as in the original renderer.
// Fit the mesh upright into the Cornell box, replacing the two blocks.
void add_mesh(Scene& scene, const char* filename, Float3 color) {
    std::ifstream file(filename);
    if (!file) throw std::runtime_error(std::string("Cannot read ") + filename);
    std::vector<Float3> vertices;
    std::vector<int> faces;
    Bounds bounds;
    std::string line, kind, token;
    int line_number = 0;
    while (std::getline(file, line)) {
        ++line_number;
        std::istringstream row(line);
        row >> kind;
        if (kind == "v") {
            Float3 p;
            if (!(row >> p.x >> p.y >> p.z) || !std::isfinite(p.x)
                || !std::isfinite(p.y) || !std::isfinite(p.z))
                throw std::runtime_error("Invalid OBJ vertex at line " + std::to_string(line_number));
            vertices.push_back(p); bounds.grow(p);
        } else if (kind == "f") {
            std::vector<int> polygon;
            while (row >> token && token[0] != '#') {
                std::string position = token.substr(0, token.find('/'));
                size_t used;
                int id = std::stoi(position, &used);
                if (used != position.size() || id == 0)
                    throw std::runtime_error("Invalid OBJ index at line " + std::to_string(line_number));
                id = id > 0 ? id - 1 : int(vertices.size()) + id;
                if (id < 0 || id >= int(vertices.size()))
                    throw std::runtime_error("OBJ index out of range at line " + std::to_string(line_number));
                polygon.push_back(id);
            }
            if (polygon.size() < 3) throw std::runtime_error("OBJ face needs three vertices");
            for (size_t i = 1; i + 1 < polygon.size(); ++i)
                faces.insert(faces.end(), {polygon[0], polygon[i], polygon[i+1]});
        }
        kind.clear();
    }
    Float3 extent = bounds.hi - bounds.lo;
    float size = std::max({extent.x, extent.y, extent.z});
    if (faces.empty() || !(size > 0) || !std::isfinite(size))
        throw std::runtime_error("OBJ has no usable mesh");
    float scale = 400 / size;
    Float3 center = (bounds.lo + bounds.hi) * 0.5f;
    for (Float3& p : vertices)
        p = (p - Float3{center.x, bounds.lo.y, center.z}) * scale + Float3{278, 1, 278};
    for (size_t i = 0; i < faces.size(); i += 3) {
        Float3 p = vertices[faces[i]], a = vertices[faces[i+1]], b = vertices[faces[i+2]];
        Float3 n = cross(a - p, b - p);
        if (dot(n, n) > 0) scene.triangles.emplace_back(p, a, b, color);
    }
}

Scene cornell_box(const char* mesh = nullptr) {
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

    if (mesh) add_mesh(scene, mesh, white);
    else {
        add_block(scene, {370, 82.5f, 169}, {165, 165, 165}, 18, white);
        add_block(scene, {187, 165, 351}, {165, 330, 165}, -15, white);
    }
    scene.build_bvh();

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
            size_t i = 3 * (size_t(y) * width + x);
            std::memcpy(pixels + i, &pixel_color, sizeof(pixel_color));
        }
    }

}

int main(int argc, char** argv) {
    int samples = samples_per_pixel;
    int width = image_width;
    std::string filename = argc > 3 ? argv[3] : "build/bvh.hdr";

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
        scene = cornell_box(argc > 4 ? argv[4] : nullptr);
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
