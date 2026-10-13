# Path tracer benchmark

Measured on 2026-10-13 on blacknoir: Intel Core i9-14900K, Linux x86-64,
GCC 15.2.0. Each renderer ran on logical CPU 8 (a P-core), single-threaded.
The workload was the same seeded 36-triangle Cornell box at 320×320, 128 spp,
with a maximum of 24 surface visits.

One full warmup per executable was excluded. Five measured renders per
executable ran in rotating order. Times include startup, scene packing,
rendering, and HDR writing. Hashing is outside the timed region.

## With `-march=native`

All three builds use `-O3 -march=native -ffp-contract=off -std=c++17`.
The explicit SIMD build also specifies `-mavx2`. Floating-point contraction
is disabled consistently so hit decisions and seeded paths remain comparable.

| Source | Median | Range | Speedup over scalar |
| --- | ---: | ---: | ---: |
| `pathtracer.cpp` | 13.946 s | 13.924–13.983 s | 1.00× |
| `pathtracer_optim.cpp` | 6.454 s | 6.451–6.506 s | 2.16× |
| `pathtracer_simd.cpp` | 6.608 s | 6.600–6.613 s | 2.11× |

## Plain `-O3` controls

A preceding comparison used the same workload and timing protocol without
`-march=native`. The AVX2 variants below add `-mavx2 -ffp-contract=off`.

| Source and flags | Median | Range |
| --- | ---: | ---: |
| `pathtracer.cpp` | 13.506 s | 13.341–13.543 s |
| `pathtracer.cpp` with AVX2 flags | 13.479 s | 13.290–13.529 s |
| `pathtracer_optim.cpp` | 8.273 s | 8.175–8.337 s |
| `pathtracer_optim.cpp` with AVX2 flags | 6.542 s | 6.462–6.612 s |
| `pathtracer_simd.cpp` | 6.670 s | 6.606–6.749 s |

## What enables automatic vectorization

The original triangle loop combines early rejection branches with a
loop-carried nearest-hit update. GCC reports unsupported control flow and
does not vectorize that loop.

`pathtracer_optim.cpp` stores positions and edges in separate arrays, computes
independent hit candidates in an eight-triangle loop, and reduces the nearest
hit afterward in triangle order. Bitwise validity comparisons avoid
short-circuit branches. Clamping rejected determinants before division
removes another branch while preserving every accepted determinant.
This source uses ordinary C++ float operations, with no intrinsics, vector
types, or SIMD pragmas.

GCC reports that the candidate loop at `pathtracer_optim.cpp:145` vectorizes
with 16-byte vectors under plain `-O3` and 32-byte vectors under
`-march=native` or `-mavx2`. The compiler produces the vector instructions.

## Validation

- Each optimized variant matched 200,006 reference intersection cases exactly
  in triangle ID, distance, and barycentrics, including empty scenes, tails,
  back faces, edges, parallel rays, and equal-distance hits.
- Every float32 RGB pixel matched exactly at 128×128, 64 spp before HDR
  quantization, for native scalar, native optimized, plain `-O3` optimized,
  and native explicit SIMD builds.
- All measured 320×320, 128-spp HDR outputs had identical SHA-256 hashes.
- AddressSanitizer and UndefinedBehaviorSanitizer intersection checks passed
  for both packed implementations.

These are end-to-end measurements on this workstation. The reported ranges
are the observed five runs, not confidence intervals.

## Perf availability

`perf stat` was attempted with user-space cycles, instructions, branches, and
branch-miss counters. The workstation has `perf_event_paranoid=4`, which blocks
them. `sudo` requires interactive authentication. No hardware counters or sampled
profiles were collected or used in the conclusions above.

## Reproduce

```sh
make test
make benchmark BENCH_ARGS="--spp 128 --width 320 --runs 5 --cpu 8"
# Include plain -O3 controls in a new comparison:
make benchmark BENCH_ARGS="--spp 128 --width 320 --runs 5 --cpu 8 --generic"
# Verify compiler vectorization:
g++ -isystem . -O3 -march=native -ffp-contract=off -std=c++17 \
    -fopt-info-vec-optimized pathtracer_optim.cpp -o build/optim-report
```

Raw trial timings and source hashes are saved in
[benchmarks/2026-10-13.json](benchmarks/2026-10-13.json).
