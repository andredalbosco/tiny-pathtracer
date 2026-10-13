// Render float32 pixels before HDR quantization for the parity check.
#define main pathtracer_main
#ifdef OPTIM
#include "../pathtracer_optim.cpp"
#elif defined(SIMD)
#include "../pathtracer_simd.cpp"
#else
#include "../pathtracer.cpp"
#endif
#undef main

int main() {
    Scene scene = cornell_box();
    std::vector<float> pixels(128 * 128 * 3);
    render(scene, 64, 128, 128, pixels.data());
    std::cout.write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * sizeof(float));
    return !std::cout;
}
