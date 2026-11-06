# General analysis and a renderer without STL containers

**Follow-up:** [Round six](ROUND6.md) isolates the compiler-code-generation
regression and recovers the original speed without reintroducing STL. The
measurements below describe the earlier plain implementation.

Astra at xhigh completed a general analysis of the renderer, sampling, CPU data flow, memory, and parallel scaling. STL removal is one comparison within that analysis. The next priorities are **camera stratification**, **dedicated finite shadow tests**, and **trig-free cosine sampling**. The aggressive tier is **shadow batching**, **scrambled low-discrepancy sampling**, and **shallow shadow culling**. These are proposals with falsifiable gates, not measured future speedups.

Read the [full general pass](../../../benchmarks/round5/astra-general-pass5.md) for the computation diagram, current CPU topology, numerical requirements, benchmarks, and all six proposals. The [source review](../../../benchmarks/round5/astra-plain-plan.md) audits the standalone renderer. Their inputs came from the completed [round-four evidence](ROUND4.md); references to local quality-final inputs in the reports are mirrored in `benchmarks/round4/quality/`.

|Priority|Simple computation change|Aggressive proposals|
|---|---|---|
|1|Stratify the existing camera jitter domain|Batch finite shadow rays with small worker queues|
|2|Specialize bounded shadow predicates and static emitter exclusions|Use verified scrambled Sobol dimensions|
|3|Test a trig-free cosine hemisphere warp|Cull shadow rays with conservative coarse geometry bounds|

The remaining NEE error is concentrated in a brightness-defined subset of106 pixels, while its P-core profile puts about50% of sampled cycles in shadow visibility. Neither observation points first to STL or RNG replacement. A primary-emitter coverage mask should test the bright-region explanation before assigning all of it to camera edges. Keep the existing half-pixel camera integral and 24-visit target when comparing new samplers. The earlier93.1%/94.8% homogeneous P8/E16 scaling result used **MIS**, and should retain that label.

## Plain implementation

`experiments/pathtracer_plain.cpp` uses fixed C arrays for scene/basis/light/path storage, malloc/free worker result buffers, C I/O and clocks, compiler atomics, and OpenMP. It has no STL containers, algorithms, threads, streams or virtual dispatch. `<new>` is used only for placement construction to begin C++17 object lifetimes. The result element's trivial default construction leaves it uninitialized; every job writes its slot before the ordered sum reads it. The executable's dynamic dependencies are libc, libm and libgomp, with no libstdc++ runtime dependency.

The shader, PCG identities, geometry, camera arithmetic, finite shadow visibility, maximum24 visits, eight-ray refill and sample sum order are preserved. All four sampling modes are available. This deliberately implements the measured8-slot AoS/OpenMP path rather than retaining the framework's other batch/layout/runtime combinations. Its fixed scene capacity is64 triangles; it creates the same36-triangle Cornell box.

```sh
make plain
./build/pathtracer_plain 128 320 build/plain.hdr --threads 32
./build/pathtracer_plain 128 320 build/plain-uniform.hdr --sampling uniform --threads 32
./build/pathtracer_plain 128 320 build/plain-mis.hdr --sampling mis --threads 32
```

## Same-computation measurements

320²/128 spp, PCG42, ray8 refill, OpenMP on **both** versions, CPU8 for P1 and explicit CPUs0..31 for all32. Ordinary GCC15.2-O3/native builds with FP contraction enabled, no PGO. Each pair has one warmup and five scored interleaved observations. Time includes startup, preparation, rendering, raw image writing and teardown.

|Sampling / CPUs|Existing quality wall s|Plain wall s|Plain time change|
|---|---:|---:|---:|
|uniform / P1|3.603177|3.906797|+8.43%|
|uniform / all32|0.273334|0.283270|+3.64%|
|nee / P1|8.327080|8.869935|+6.52%|
|nee / all32|0.561440|0.576148|+2.62%|

The plain version is slower on oneP in both workloads, with nonoverlapping observed timing ranges. The all-core NEE ranges overlap, including an existing-renderer outlier, so its small median difference is not a firm runtime separation. This honors the requested implementation style but does not establish a general STL penalty. Keep the existing executable as a measured reference and use the plain file for subsequent experiments if avoiding STL is preferred.

Removing containers also changes specialization, initialization, alias information, function inlining and code layout. GCC's release vector subscript is a pointer dereference and array subscript accesses the underlying C array; these do not imply one library call per element. Existing result buffers were already reused once per worker. The plain source skips their initial zero fill, but that saving did not overcome the changed generated code. Do not attribute the entire difference to an allocator or a container without an isolated assembly/variant test.

## Fresh perf and correctness

Two interleaved root-perf observations per oneP version run the renderer as the ordinary user, pinned to CPU8, with explicit cpu_core user events. This is diagnostic evidence; throughput above was measured without perf. All hardware running fractions are99–100%.

|Mode/version|Cycles/path|Instructions/path|IPC|Active/reference cycles|
|---|---:|---:|---:|---:|
|uniform/stl|1650.5|4389.5|2.659|1.8711|
|uniform/plain|1788.6|4633.1|2.590|1.8684|
|nee/stl|3789.8|10742.8|2.835|1.8656|
|nee/plain|4019.5|10826.0|2.693|1.8709|

Plain executes more instructions and more cycles per completed path, with lower IPC and nearly unchanged active/reference ratios. This supports changed execution cost rather than a large isolated-core frequency difference. It does not by itself identify the responsible compiler transformations or prove a memory bottleneck.

Sixty strict comparisons cover uniform/cosine/NEE/MIS, widths1/9/17/32/64, sample/tile tails and1/4/32 OpenMP workers. Pixels **and work counts** are identical to the frozen round-four strict quality reference. A full320²/128-spp NEE comparison is also bit-identical. ASan/UBSan runs passed NEE/MIS with partial tiles and four workers. Current production text is byte-identical to the measured plain text; the only cleanup suppresses third-party STB initializer warnings.

Full fast320² uniform images differ by RMSE1.13e-9 (max5.96e-8); NEE differs by RMSE2.95e-8 (max1.62e-5), consistent with changed contracted code. Strict equality is the exact result; fast builds are not claimed bit-identical. Full raw arrays and sanitizer/profile binaries are in ignored `build/pass5/`; records, hashes, analysis and perf exports are in `benchmarks/round5/`.

STL-free implementation is ready, and all future proposal rankings remain about computation and image accuracy per time. Camera sampling is the next inexpensive quality experiment; finite shadows are the next major compute stage to optimize.
