// Complete paths and all samples stay in one thread per pixel.
#include "cuda_common.cuh"

__global__ void render(const Scene* scene, int samples, int width, float* pixels) {
    int pixel = blockIdx.x * blockDim.x + threadIdx.x;
    if (pixel >= width * width) return;
    Float3 sum{};
    for (int s = 0; s < samples; ++s) {
        Path path;
        path.ray = camera_ray(scene->camera, pixel, s, width, width, path.random);
        while (scene->intersect(path.ray, path.hit) && shade(*scene, path)) {}
        sum += path.color;
    }
    write_pixel(pixels, pixel, sum * (1.0f / samples));
}
int main(int argc, char** argv) {
    Settings s = settings(argc, argv, "build/megakernel.hdr");
    Buffers b(cornell_box(), s.width);
    render<<<(s.width*s.width+255)/256, 256>>>(b.scene, s.samples, s.width, b.pixels);
    finish(s, b);
}
