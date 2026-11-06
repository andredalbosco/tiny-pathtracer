#define main pathtracer_bvh_main
#include "../pathtracer_bvh.cpp"
#undef main

int checks = 0;
void check(const Scene& scene, Ray ray) {
    Hit linear, bvh;
    for (int i = 0; i < int(scene.triangles.size()); ++i)
        if (scene.triangles[i].intersect(ray, linear)) linear.triangle = i;
    scene.intersect(ray, bvh);
    if (linear.triangle != bvh.triangle || linear.t != bvh.t
        || linear.u != bvh.u || linear.v != bvh.v)
        throw std::runtime_error("BVH mismatch at ray " + std::to_string(checks));
    ++checks;
}

int main() {
    Scene cornell = cornell_box();
    std::mt19937_64 engine{123};
    std::uniform_real_distribution<float> position{-100, 700}, direction{-1, 1};
    for (int count : {0, 1, 3, 4, 5, 7, 8, 9, 17, 35, 36}) {
        Scene scene;
        scene.triangles.assign(cornell.triangles.begin(), cornell.triangles.begin() + count);
        scene.build_bvh();
        if (scene.nodes.size() != size_t(count ? 2 * count - 1 : 0))
            throw std::runtime_error("Unexpected node count for one triangle per leaf");
        std::vector<int> occurrences(count, 0);
        for (const BVHNode& node : scene.nodes) {
            if (node.is_leaf()) {
                if (node.triangle >= count || node.left != -1 || node.right != -1)
                    throw std::runtime_error("Invalid triangle leaf");
                ++occurrences[node.triangle];
            } else if (node.left < 0 || node.right < 0) {
                throw std::runtime_error("Internal node is missing a child");
            }
        }
        for (int occurrences_of_triangle : occurrences)
            if (occurrences_of_triangle != 1)
                throw std::runtime_error("Each triangle must appear in exactly one leaf");
        for (int i = 0; i < 20000; ++i) {
            Vector3f p{position(engine), position(engine), position(engine)};
            Vector3f d{direction(engine), direction(engine), direction(engine)};
            check(scene, {p, d.normalized()});
        }
        // Axis-parallel rays, including signed zero and origins on box faces.
        for (Vector3f p : {Vector3f{0, 0, 0}, Vector3f{278, 278, 278}, Vector3f{555, 555, 555}})
            for (Vector3f d : {Vector3f{1, 0, -0.0f}, Vector3f{-1, 0, 0}, Vector3f{0, 1, 0},
                            Vector3f{0, -1, 0}, Vector3f{0, 0, 1}, Vector3f{0, 0, -1}})
                check(scene, {p, d});
    }
    Scene ties;
    // Equal-distance triangles in separate leaves must preserve input order.
    for (int i = 0; i < 17; ++i) {
        float size = float(1 << (i % 5));
        ties.triangles.emplace_back(Vector3f{0, 0, 0}, Vector3f{size, 0, 0},
                                    Vector3f{0, size, 0}, Vector3f{1, 1, 1});
    }
    ties.build_bvh();
    for (Ray ray : {Ray{{0.25f, 0.25f, 1}, {0, 0, -1}},
                    Ray{{0.25f, 0.25f, -1}, {0, 0, 1}},
                    Ray{{0, 0, 1}, {0, 0, -1}},
                    Ray{{0.5f, 0.5f, 1}, {0, 0, -1}},
                    Ray{{0.25f, 0.25f, 1e-4f}, {0, 0, -1}}}) check(ties, ray);
    Hit hit;
    if (!ties.intersect({{0.25f, 0.25f, 1}, {0, 0, -1}}, hit) || hit.triangle != 0)
        throw std::runtime_error("Equal-distance triangle order changed");

    const char* mesh = "build/bvh-test.obj";
    {
        std::ofstream file(mesh);
        file << "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                "f -4/1/1 -3/1/1 -2/1/1 -1/1/1\n";
    }
    Scene loaded = cornell_box(mesh);
    if (loaded.triangles.size() != 14) throw std::runtime_error("OBJ triangulation failed");
    for (int i = 0; i < 20000; ++i) {
        Vector3f p{position(engine), position(engine), position(engine)};
        Vector3f d{direction(engine), direction(engine), direction(engine)};
        check(loaded, {p, d.normalized()});
    }
    {
        std::ofstream file(mesh);
        file << "v 0 0 0\nf 1 2 3\n";
    }
    bool rejected = false;
    try { cornell_box(mesh); } catch (const std::exception&) { rejected = true; }
    if (!rejected) throw std::runtime_error("Invalid OBJ index accepted");
    std::remove(mesh);
    std::cout << checks << " BVH/linear intersections match exactly; OBJ checks passed\n";
}
