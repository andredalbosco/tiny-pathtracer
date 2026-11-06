#define main pathtracer_main
#ifdef OPTIM
#include "../experiments/pathtracer_optim.cpp"
#else
#include "../experiments/pathtracer_simd.cpp"
#endif
#undef main

int checks = 0;

void check(const Scene& scene, Ray ray) {
    Hit scalar, simd;
    for (int i = 0; i < int(scene.triangles.size()); ++i) {
        if (scene.triangles[i].intersect(ray, scalar)) scalar.triangle = i;
    }
    scene.intersect(ray, simd);
    if (scalar.triangle != simd.triangle
        || std::memcmp(&scalar.t, &simd.t, sizeof(float))
        || std::memcmp(&scalar.u, &simd.u, sizeof(float))
        || std::memcmp(&scalar.v, &simd.v, sizeof(float))) {
        std::cerr << "Intersection mismatch at check " << checks << '\n';
        std::exit(1);
    }
    ++checks;
}

int main() {
    Scene cornell = cornell_box();
    std::mt19937_64 engine{123};
    std::uniform_real_distribution<float> position{-100, 700}, direction{-1, 1};
    for (int count : {0, 1, 7, 8, 9, 15, 16, 17, 35, 36}) {
        Scene scene;
        scene.triangles.assign(cornell.triangles.begin(), cornell.triangles.begin() + count);
        scene.pack_triangles();
        for (int i = 0; i < 20000; ++i) {
            Float3 origin{position(engine), position(engine), position(engine)};
            Float3 d{direction(engine), direction(engine), direction(engine)};
            check(scene, {origin, normalize(d)});
        }
    }

    // Duplicate coplanar triangles exercise ties within and across packets.
    Scene scene;
    scene.triangles.assign(17, Triangle{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 1}});
    scene.pack_triangles();
    for (Ray ray : {
        Ray{{0.25f, 0.25f, 1}, {0, 0, -1}},
        Ray{{0.25f, 0.25f, -1}, {0, 0, 1}},
        Ray{{0, 0, 1}, {0, 0, -1}},
        Ray{{0.5f, 0.5f, 1}, {0, 0, -1}},
        Ray{{0.25f, 0.25f, 1e-4f}, {0, 0, -1}},
        Ray{{0.25f, 0.25f, 1}, {1, 0, 0}}}) check(scene, ray);
    Hit hit;
    if (!scene.intersect({{0.25f, 0.25f, 1}, {0, 0, -1}}, hit) || hit.triangle != 0)
        return 1;

    std::cout << checks << " reference/packet intersections match exactly\n";
}
