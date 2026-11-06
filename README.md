# Tiny path tracer

Small C++ experiments for learning path tracing, BVHs, CUDA, OptiX, and volumes.

Main renderers stay at the root. Other implementations and archives live in
`experiments/`; dependencies live in `external/`. Historical performance reports
are in [experiments/archive/docs](experiments/archive/docs/), with scripts and
results in `benchmarks/`.

## Run

```sh
git submodule update --init
make
./build/pathtracer 128 320 build/cornell.hdr

make bvh test-bvh
./build/pathtracer_bvh 32 768 build/ajax.hdr /path/to/ajax.obj
```

Arguments: samples per pixel, image width, output file, and optional OBJ for the
BVH example. Ajax uses Nori’s camera with an emissive area light; the mesh is not
bundled. The integrator samples uniform hemisphere bounces.
Eigen lives in `external/eigen`. The BVH splits input ranges in half, with one
triangle per leaf. The earlier median builder is in `experiments/archive/pathtracer_bvh_median.cpp`.

GPU examples: `make cuda`, `make optix`, and `make cloud-viewer`.
See the [detailed notes](experiments/archive/README-detailed.md) for dependencies and other experiments.

## Open-source references

- [Ray Tracing in One Weekend](https://github.com/RayTracing/raytracing.github.io) — start with a small renderer.
- [Nori](https://github.com/wjakob/nori) — educational renderer and scene setups.
- [PBRT](https://github.com/mmp/pbrt-v4) — rendering algorithms, BVHs, and SAH.
- [Mitsuba](https://github.com/mitsuba-renderer/mitsuba3) — research renderer and reference images.
- [MoonRay](https://github.com/OpenMoonRay/openmoonray) — production renderer.
- [Cycles](https://github.com/blender/cycles) — production CPU and GPU path tracing.
- [TinyBVH](https://github.com/jbikker/tinybvh) — compact BVH construction and traversal.
- [Embree](https://github.com/RenderKit/embree) — CPU ray-tracing kernels and BVH acceleration.

Cloud examples use the [WDAS Cloud Data Set](https://disneyanimation.com/resources/clouds/),
Copyright 2017 Disney Enterprises, Inc., under [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/).
