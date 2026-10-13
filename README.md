# Tiny path tracer

For the STL-free implementation, use `pathtracer_plain.cpp`. It uses C arrays,
C allocation/I/O and OpenMP, with the same sampling computation.
[The compiler audit and speed recovery](ROUND6.md) explain how a small inlining
and GCC loop-pass change restores the original speed. The
[general Astra analysis](ROUND5.md) covers sampling quality and six proposals.

```sh
make plain
./build/pathtracer_plain 128 320 build/plain.hdr --threads 32
```

The plain targets use GCC's `-fno-loop-unroll-and-jam` to avoid extra vector
temporaries in the eight-ray loop. `PLAIN_FLAGS` is overridable; use
`make plain PLAIN_FLAGS=` with a compiler that lacks that option. Other targets
retain their existing flags. The loop still uses compiler-generated SIMD.


The latest sampling experiment adds cosine continuation, direct light sampling
(NEE), and optional MIS, while retaining the Cornell scene and 24 surface visits.
NEE gives about 10× better full-image error per second in the measured case.
See [sampling, arithmetic and RNG results](ROUND4.md) for tables, images, data
flow, perf and core scaling. PCG and the original ray-parallel intersection remain
defaults; the other intersection formulas and RNGs are explicit experiments.

```sh
make quality
./build/pathtracer_quality 128 320 build/nee.hdr --batch 8 --refill --threads 32
./build/pathtracer_quality 128 320 build/cosine.hdr --sampling cosine --batch 8 --refill --threads 32
make quality-rng explore
make test-quality test-explore test-rng
```

Quality sampling modes are `uniform`, `cosine`, `nee` (default), and `mis`.
`--seed N` controls the master seed; each pixel/sample keeps its own stream.
Std/OpenMP, AoS/SoA, fixed/refill execution and explicit CPU affinity remain
available. Optional RNG executables are `build/pathtracer_quality_xoroshiro_pp`
and `build/pathtracer_quality_xoroshiro_ss`; they change the random sequence.
`make quality-pgo` trains a GCC profile for NEE; that target is not scored here.

A small Cornell box path tracer with 36 triangles, uniform hemisphere
sampling, a maximum of 24 surface visits, and unclamped linear HDR output.

```sh
make
./build/pathtracer 128 320 build/cornell.hdr

make simd
./build/pathtracer_simd 128 320 build/cornell-simd.hdr

make optim
./build/pathtracer_optim 128 320 build/cornell-optim.hdr
```

Arguments are samples per pixel, image width, and output filename. Defaults are
128 spp, 320 pixels, and `cornell_box.hdr`. `make render`, `make render-optim`,
and `make render-simd` accept the same arguments through `RENDER_ARGS`. Executables and test/benchmark
outputs live in `build/`.

`pathtracer.cpp` is the scalar reference. Default builds use
`-O3 -march=native -ffp-contract=off` for the current CPU. The standalone
`pathtracer_simd.cpp` keeps the same camera, materials, integrator, arithmetic
order, and seeded random stream. It packs triangle positions and edges into
aligned arrays and intersects eight triangles per AVX2 packet. The final packet
is padded with degenerate triangles; equal-distance hits keep triangle order.
Repack the scene with `pack_triangles()` after changing its triangle list.

`pathtracer_optim.cpp` uses the same packed layout with ordinary C++ float
operations. It computes independent hit candidates before reducing the nearest
hit in triangle order. It evaluates validity comparisons with bitwise AND and
clamps rejected determinants before division, allowing GCC's `-O3` to vectorize
the candidate loop. There are no intrinsics, vector types, or SIMD pragmas.
The code is portable C++; the compiler chooses its instruction width.
`make build/pathtracer_o3 build/pathtracer_optim_o3` also builds plain `-O3`
controls without `-march=native`.
This scene also needs `pack_triangles()` after geometry edits.

The SIMD build requires an x86 CPU with AVX2 and adds `-mavx2`.
The original three builds disable floating-point contraction to keep arithmetic
consistent and avoid fused operations changing hit decisions. No approximate reciprocal,
fast math, parallel paths, or alternative sampler is used. The camera sampling
expression's missing `Float` was repaired as `Float2{float(x), float(y)}` in both
files, retaining the existing `random_float2() * 0.5f` jitter.

## Validation and timing

```sh
make test
make benchmark
make benchmark BENCH_ARGS="--spp 128 --width 320 --runs 5 --cpu 8"
```

Tests compare 200,006 reference intersections with each of the SIMD, optimized,
and plain `-O3` optimized builds, including empty scenes,
packet tails, back faces, edges, parallel rays, and equal-distance triangles.
They also compare every float32 pixel of a seeded 128×128, 64-spp render before
HDR encoding.

The Linux benchmark uses `taskset` to pin each renderer to one CPU, performs
one full warmup per executable, rotates execution order between trials, and
checks every output's HDR hash. It compares the scalar, optimized, and explicit
SIMD builds with `-march=native`. Add `--generic` to `BENCH_ARGS` to also compare
the plain `-O3` controls. Timings include process startup, scene setup, rendering, and HDR writing; hashes are
checked outside the timed region. Results are saved under `build/`.

See [BENCHMARK.md](BENCHMARK.md) for the measured results. Existing PBRT and
Mitsuba comparison assets and their provenance are in
[references/cornell_box](references/cornell_box/README.md).

## Computation experiments

`make simple advanced` builds the new layout, threading and refill experiments.
All new backends use the same reproducible per-pixel/sample PCG streams; these
are deliberately separate from the historical global MT sequence above.

```sh
./build/pathtracer_simple 128 320 build/flat.hdr --kernel flat
./build/pathtracer_simple 128 320 build/refill.hdr --kernel rays --batch 8 --refill --threads 32
./build/pathtracer_advanced 128 320 build/culled.hdr --kernel culled --threads 32
make test-compute
```

The simple intersection loops use ordinary C++; the advanced kernels use AVX2
and FMA. Performance builds permit floating-point contraction; a separate strict
build verifies exact layout/scheduling equivalence. `--layout aos|soa`,
`--batch 1|2|4|8`, `--cpus`, and `--raw` support comparisons. See
[COMPUTATION.md](COMPUTATION.md) for all six proposals, numerical differences,
whole-process paths/second, core scaling, and dataflow diagrams.

Select `--runtime omp` for the OpenMP team or `--runtime std` for explicit
threads. Both use the same tile queue and produce identical raw images.
See [SCALING.md](SCALING.md) for the 1/2/4/8-P, 1/2/4/8/16-E, mixed-core and
SMT comparisons, worker diagnostics and available perf timing events.
`OMP_FLAGS` defaults to `-fopenmp`; set it empty for a std-only build.

Hardware counters, cycle samples, memory-controller traffic, and frequency evidence
are in [PERF.md](PERF.md). [TALKS.md](TALKS.md) connects twelve additional
transcript-backed talks to three simple and three more aggressive next experiments.

The next optimization round reuses one result buffer per worker and combines
sine/cosine evaluation on GNU libc, retaining the original intersection loop.
`make pgo` builds `build/pathtracer_simple_pgo` with GCC profile feedback:

```sh
make pgo
./build/pathtracer_simple_pgo 128 320 build/pgo.hdr --kernel rays --batch 8 --refill --threads 32
```

See [ROUND3.md](ROUND3.md) for the new comparisons, strict numerical parity,
fresh perf interpretation, and the rejected column-interface experiment.
`make test-sincos` checks all possible sampler angles on GNU libc.
