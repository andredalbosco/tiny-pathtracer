// A warp-wide state machine chooses the stage with the most waiting lanes.
// Path state remains local; all lanes participate in every scheduler ballot.
#include "cuda_common.cuh"

enum Stage { CAMERA, INTERSECT, SHADE, FINISH, DONE };
__global__ void render(const Scene* scene, int samples, int width, float* pixels) {
    int pixel = blockIdx.x * blockDim.x + threadIdx.x;
    bool valid = pixel < width * width;
    int stage = valid ? CAMERA : DONE, sample = 0;
    Path path;
    Float3 sum{};
    const unsigned mask = __activemask();
    while (__ballot_sync(mask, stage != DONE)) {
        int selected = CAMERA, largest = -1;
        for (int candidate = CAMERA; candidate < DONE; ++candidate) {
            int count = __popc(__ballot_sync(mask, stage == candidate));
            if (count > largest) { largest = count; selected = candidate; }
        }
        if (stage != selected) continue;
        switch (selected) {
        case CAMERA:
            path = Path{};
            path.ray = camera_ray(scene->camera, pixel, sample, width, width, path.random);
            stage = INTERSECT;
            break;
        case INTERSECT:
            stage = scene->intersect(path.ray, path.hit) ? SHADE : FINISH;
            break;
        case SHADE:
            stage = shade(*scene, path) ? INTERSECT : FINISH;
            break;
        case FINISH:
            sum += path.color;
            stage = ++sample < samples ? CAMERA : DONE;
            break;
        }
    }
    if (valid) write_pixel(pixels, pixel, sum * (1.0f / samples));
}
int main(int argc, char** argv) {
    Settings s = settings(argc, argv, "build/scheduled.hdr");
    Buffers b(cornell_box(), s.width);
    render<<<(s.width*s.width+255)/256, 256>>>(b.scene, s.samples, s.width, b.pixels);
    finish(s, b);
}
