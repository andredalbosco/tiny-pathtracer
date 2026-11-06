// Render float32 pixels before HDR quantization for the parity check.
#define main pathtracer_main
#if defined(BVH)
#include "../pathtracer_bvh.cpp"
#elif defined(BVH_LINEAR)
#include "../experiments/archive/pathtracer_bvh_median.cpp"
#elif defined(OPTIM)
#include "../experiments/pathtracer_optim.cpp"
#elif defined(SIMD)
#include "../experiments/pathtracer_simd.cpp"
#else
#include "../pathtracer.cpp"
#endif
#undef main

int main() {
    Scene scene = cornell_box();
#if defined(BVH_LINEAR)
    // Collapse the archived median builder into a brute-force reference leaf.
    scene.nodes.resize(1);
    scene.nodes[0].left = scene.nodes[0].right = -1;
    scene.nodes[0].first = 0;
    scene.nodes[0].count = int(scene.triangles.size());
    std::iota(scene.indices.begin(), scene.indices.end(), 0);
#endif
    std::vector<float> pixels(128 * 128 * 3);
    render(scene, 64, 128, 128, pixels.data());
    std::cout.write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * sizeof(float));
    return !std::cout;
}
