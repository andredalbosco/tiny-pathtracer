# Tiny path tracer

Three Cornell box renderers live on `main`. The `experiments` branch holds the BVH, CUDA and volume experiments and benchmark history.

| Renderer | Build | Run |
| --- | --- | --- |
| Scalar C++ + OpenMP | `make` | `./build/pathtracer 128 320 build/scalar.hdr` |
| CPU OpenMP + SIMD triangle packets | `make simd` | `./build/pathtracer_simd 128 320 build/simd.hdr` |
| NVIDIA OptiX | `make optix` | `./build/pathtracer_optix build/optix.ppm` |

The CPU arguments are samples per pixel, image width and output path. OptiX renders at 640 × 640 and loads the PTX file next to its executable. Its build requires the CUDA toolkit and fetches the pinned OptiX headers.

The SIMD renderer tests one ray against eight triangles at a time, using compiler auto-vectorization. Both CPU renderers use OpenMP to distribute image rows across CPU threads with dynamic scheduling; each pixel keeps its own random streams and sample accumulation order. Set `OMP_NUM_THREADS` to control the thread count:

```sh
OMP_NUM_THREADS=8 ./build/pathtracer_simd 128 320 build/simd.hdr
```

To build both CPU renderers without OpenMP, use `make -B all simd OPENMP_FLAGS=`.

Release builds disable debug assertions with `-DNDEBUG`. Use `make BUILD=debug all simd` for checked CPU builds in `build/debug/`; `BUILD=debug` also applies to the OptiX targets.
