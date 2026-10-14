# Clang renderer in the style of pathtracer.cpp

`pathtracer_clang.cpp` now implements the original uniform renderer with plain
`Scene`, `pathtrace`, `render` and `main` functions. It has 344 lines, down from
the 564-line version at `e90b8d6`; the original `pathtracer.cpp` has 264 lines.

The Cornell scene, front-face Möller–Trumbore geometry, uniform hemisphere
sampling, rho × 2 cos throughput, half-pixel camera jitter and 24 surface visits
are retained. It uses deterministic per-pixel/sample PCG seed 42, as in the
optimization experiments; the original file uses a different global Mersenne
Twister stream, so the two original/PCG images are not expected to match bytes.

The useful CPU optimizations remain explicit and small:

- `struct Triangles` stores positions and edges as scalar C arrays, with surface
  values separate. There are no STL containers or layout templates.
- `Scene::intersect` tests eight rays against each triangle with ordinary C++.
  Clang generates the SIMD arithmetic; no intrinsics are used.
- `pathtrace` keeps eight paths active while samples for one pixel remain. When
  a path finishes, its slot starts another sample. Its shading loop follows
  the same hit/miss/emission/diffuse flow as the original.
- `render` is a pair of pixel loops under OpenMP. Each worker reuses one sample
  buffer and sums samples in index order.

The sampling modes, MIS/NEE scratch state, template scene types, custom tile
queue, CPU-list parser, runtime/kernel/layout options, alternate RNG switches,
work-counter JSON and raw-output option were removed from this file. NEE,
cosine and MIS remain in the plain and quality implementations. Previous
consolidation measurements refer to the source at `e90b8d6`, not this revision.

```sh
make build/pathtracer_clang
./build/pathtracer_clang 128 320 build/clang.hdr 32
make test-clang
```

The four positional arguments are samples, width, output filename and threads.
Threads are optional and default to OpenMP's configured count. Output defaults
to `build/clang.hdr`. `OMP_NUM_THREADS`, `OMP_PLACES`, `OMP_PROC_BIND` and external
`taskset` can control execution without an affinity parser in the renderer.
`make render-clang` accepts `RENDER_ARGS`; Clang targets still use O3/native,
fast FP contraction and OpenMP. Strict targets disable FP contraction.

## Checks and measured tradeoff

All 17 strict pixel comparisons pass against the independent uniform PCG
reference: widths 1/9/17/33/64 with sample tails and one/four/32 threads, plus
320×320 at 128 spp on one and 32 threads. Raw output for comparisons lives in
`tests/raw_clang.cpp`, keeping instrumentation outside the renderer.
The full fast images also match the previous version byte-for-byte on both
CPU scopes, and remain identical across all observations and thread counts.
GCC AddressSanitizer/UndefinedBehaviorSanitizer smoke renders passed on one and
32 threads; an HDR output smoke render passed as well.

|320×320, 128 spp, uniform|Previous version|Simple version|Time change|
|---|---:|---:|---:|
|One P core, CPU 8|3.454122 s|3.521534 s|+1.95%|
|All 32 logical CPUs|0.258643 s|0.264285 s|+2.18%|

These are fresh paired Clang 20.1.8/libomp wall-time observations on blacknoir's
i9-14900K, with one warmup and three scored observations per configuration.
Both write raw float pixels. CPU scopes are identical, with `taskset` and
explicit OpenMP places/binding. All observations are retained. The small
measured slowdown is the tradeoff for the simpler pixel execution; no confidence
interval or new perf-counter attribution is claimed.

The previous implementation refilled eight paths across a whole 16×8 tile.
The new one finishes each pixel's samples before moving on, so its last paths
can leave more ray slots unused. At 128 spp, the per-worker sample buffer also
falls from 196,608 bytes to 1,536 bytes. These are algorithm/storage differences,
not measured DRAM traffic or proof of the slowdown's precise cause.

Timing records, source/binary hashes, strict checks and full fast image
comparisons are in `benchmarks/clang-simple/`. The old expanded renderer and
its old command line remain recoverable through commit `e90b8d6`.

To reproduce the raw timing comparison:

```sh
make build/raw_clang
python3 benchmarks/clang-simple/bench.py
```

The script recovers the previous source from Git and builds a reference under
`build/clang-simple/` if the frozen reference binary is absent. New raw images
and scratch records remain in `build/`.
