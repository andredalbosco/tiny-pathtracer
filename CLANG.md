# Dedicated Clang renderer

`pathtracer_clang.cpp` is a standalone STL-free implementation with the same
Cornell scene, PCG streams, camera, eight-ray intersection, ordered sample sums,
24 surface visits and OpenMP scheduler as `pathtracer_plain.cpp`.

It consolidates the two useful layouts from round eight in one executable.
Uniform sampling uses scalar geometry columns (`float x[64], y[64], z[64]` for
each of p/e1/e2). Cosine, NEE and MIS use three `Float3[64]` field arrays.
Surface values remain together in a separate array. Compile-time scene types
select storage when entering a sampling mode, with no storage-selection branch
inside the intersection loop. Cosine/MIS are supported and checked, but their
layout performance was not measured separately.

There are no STL containers, SIMD intrinsics or GCC-specific loop flags. The
Clang build uses O3, native architecture, fast FP contraction and OpenMP. Scene
preparation remains outside the reported rendering interval.

```sh
make build/pathtracer_clang
./build/pathtracer_clang 128 320 build/clang-nee.hdr --threads 32
./build/pathtracer_clang 128 320 build/clang-uniform.hdr --sampling uniform --threads 32
make test-clang
```

`make clang` includes the new executable alongside the compiler-comparison
builds. `make render-clang` accepts `RENDER_ARGS`. NEE is the default mode;
the default output is `build/clang.hdr`. `CLANGXX` defaults to `clang++-20`.
`build/pathtracer_clang_strict` disables FP contraction for exact comparisons.

## Verification

62 strict pixel/work comparisons passed against the independent Clang plain
reference: four sampling modes, five small/tail workloads with three seeds,
one/four/32 threads, plus full 320×320×128 uniform and NEE renders. The tested
work counters are completed paths, rays, ray slots, batches and shadow rays.

The four small fast comparisons are byte-identical to the selected round-eight
layout builds. Full fast images are also byte-identical between the new and
selected old executable, across every scored/warmup observation and both CPU
scopes. An HDR smoke render also succeeded.

## Consolidation timings

|Sampling / CPUs|Selected round-eight build|New `pathtracer_clang`|Time change|
|---|---:|---:|---:|
|Uniform / P1|3.485689 s|3.455868 s|-0.86%|
|Uniform / all32|0.257968 s|0.259196 s|+0.48%|
|NEE / P1|7.987883 s|8.016830 s|+0.36%|
|NEE / all32|0.549263 s|0.551972 s|+0.49%|

Fresh paired measurements on blacknoir's i9-14900K with Clang 20.1.8/libomp:
320×320, 128 spp, seed 42, CPU 8 for P1 and CPUs 0..31 for all32. Each case has
one warmup and three scored interleaved observations. The table uses median
wall time including raw output; hashing is outside the timed interval. No
scored observation was discarded. These small differences establish similar
observed throughput, not a new optimization win or a confidence interval.

Reproduce the timing comparison, after building both round-eight layout targets:

```sh
make build/pathtracer_clang build/pathtracer_triangles_clang_fields build/pathtracer_triangles_clang_soa
python3 benchmarks/clang-delivery/bench.py
```

Checks, all 32 timing records, observed ranges, source/binary hashes and the
benchmark script are archived in `benchmarks/clang-delivery/`. The sampling
computation is unchanged; performance remains specific to this CPU/compiler.
