// Explicit ray and shading queues. Path state lives in device memory.
// Samples are separate passes so each pixel accumulates in reference order.
#include "cuda_common.cuh"

struct Queue { int* items; int* count; };
__device__ void push(Queue q, int pixel) { q.items[atomicAdd(q.count, 1)] = pixel; }
__global__ void generate(const Scene* scene, int sample, int width, Path* paths,
                         Queue rays, float* pixels) {
    int pixel = blockIdx.x * blockDim.x + threadIdx.x;
    if (pixel >= width*width) return;
    Path path;
    path.ray = camera_ray(scene->camera, pixel, sample, width, width, path.random);
    paths[pixel] = path;
    rays.items[pixel] = pixel;
    if (pixel == 0) *rays.count = width*width;
    if (sample == 0) write_pixel(pixels, pixel, {});
}
__global__ void intersect(const Scene* scene, Path* paths, Queue rays, Queue hits) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= *rays.count) return;
    int pixel = rays.items[i];
    if (scene->intersect(paths[pixel].ray, paths[pixel].hit)) push(hits, pixel);
}
__global__ void shade_hits(const Scene* scene, Path* paths, Queue hits, Queue next) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= *hits.count) return;
    int pixel = hits.items[i];
    if (shade(*scene, paths[pixel])) push(next, pixel);
}
__global__ void accumulate(const Path* paths, int width, float* pixels) {
    int pixel = blockIdx.x * blockDim.x + threadIdx.x;
    if (pixel >= width*width) return;
    size_t i = size_t(pixel)*3;
    Float3 c = paths[pixel].color;
    pixels[i] += c.x; pixels[i+1] += c.y; pixels[i+2] += c.z;
}
__global__ void average(int width, int samples, float* pixels) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < width*width*3) pixels[i] *= 1.0f / samples;
}
int main(int argc, char** argv) {
    Settings s = settings(argc, argv, "build/wavefront.hdr");
    Buffers b(cornell_box(), s.width);
    int n = s.width*s.width, blocks = (n+255)/256;
    Path* paths;
    cuda_check(cudaMalloc(&paths, size_t(n)*sizeof(Path)));
    Queue rays[2], hits;
    for (Queue* q : {&rays[0], &rays[1], &hits}) {
        cuda_check(cudaMalloc(&q->items, size_t(n)*sizeof(int)));
        cuda_check(cudaMalloc(&q->count, sizeof(int)));
    }
    for (int sample = 0; sample < s.samples; ++sample) {
        generate<<<blocks,256>>>(b.scene, sample, s.width, paths, rays[0], b.pixels);
        for (int depth = 0; depth < max_depth; ++depth) {
            Queue current = rays[depth%2], next = rays[(depth+1)%2];
            cuda_check(cudaMemset(hits.count, 0, sizeof(int)));
            cuda_check(cudaMemset(next.count, 0, sizeof(int)));
            intersect<<<blocks,256>>>(b.scene, paths, current, hits);
            shade_hits<<<blocks,256>>>(b.scene, paths, hits, next);
        }
        accumulate<<<blocks,256>>>(paths, s.width, b.pixels);
    }
    average<<<(n*3+255)/256,256>>>(s.width, s.samples, b.pixels);
    finish(s, b);
    for (Queue* q : {&rays[0], &rays[1], &hits}) {
        cuda_check(cudaFree(q->items)); cuda_check(cudaFree(q->count));
    }
    cuda_check(cudaFree(paths));
}
