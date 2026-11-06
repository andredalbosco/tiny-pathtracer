#define PATHTRACER_CLANG_NO_MAIN
#include "../experiments/pathtracer_clang.cpp"

int main(int argc, char** argv) {
    int samples = argc > 1 ? atoi(argv[1]) : 8;
    int width = argc > 2 ? atoi(argv[2]) : 32;
    int threads = argc > 3 ? atoi(argv[3]) : 1;
    if (samples < 1 || width < 1 || threads < 1) return 1;
    omp_set_dynamic(0); omp_set_num_threads(threads);
    Scene scene = cornell_box();
    int height = int(width / scene.camera.aspect);
    size_t count = size_t(width) * height * 3;
    float* pixels = (float*)calloc(count, sizeof(float));
    if (!pixels) return 1;
    bool rendered = render(scene, samples, width, height, pixels);
    bool written = rendered && fwrite(pixels, sizeof(float), count, stdout) == count;
    free(pixels);
    return written ? 0 : 1;
}
