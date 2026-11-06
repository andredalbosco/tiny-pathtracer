# Astra plan: small changes after the hardware profile

Read-only analysis of the fresh source snapshot from clean HEAD `47ced90`, 2026-10-13. Root owns implementation and measurements. This plan changes neither the estimator nor its per-path random streams. The baseline is the current ordinary C++ `RayScene`, AoS paths, batch eight, persistent refill, native FMA configuration; retain the matching strict build as a separate numerical baseline.

The best initial source experiment is a direct ray-column interface. Worker result-buffer reuse is simpler and worth implementing separately, but its likely ceiling is smaller. Then try profile-guided compilation of the best unchanged source. A combined sine/cosine call is a separate math-library experiment whose exactness must be demonstrated, rather than presumed.

## Baseline and success criterion

Freeze baseline sources and binaries before editing, including both `-ffp-contract=fast` and `off`. Use identical compiler, ordinary flags, output format, scene, PCG seeding, maximum visits, runtime, affinity, and dimensions. The source snapshot's `benchmarks/benchmark_compute.py` records current headers but does not identify the historical source behind every separately named executable. Add per-binary hashes and its source/flag provenance to experiment metadata; a baseline binary built before header edits must not be described by the current headers' hashes.

Primary score remains complete executable wall time, with render-only time retained. The representative score is 320×320 at 128 spp, one P core on CPU 8 and all 32 logical CPUs; calibrate one E core and the homogeneous P/E groups for finalists. Screen small changes with a compact warmed interleaved matrix first. Use the existing five-observation protocol for finalists and preserve raw runs. Do not combine observer/perf launch time with the unprofiled score or run competing CPU jobs.

Compare the frozen baseline against each independent change before combining winners. A useful matrix is baseline, columns only, buffer only, columns+buffer, then best source with/without PGO. Keep sincos independently switchable during evaluation. An interaction can change the ranking, so measure the combination instead of adding standalone percentages.

## 1. Direct columns at the ray-intersection boundary

Current flow, visible in `experiments/render_driver.h:245–255` and `experiments/simple_kernels.h:60–68`, is:

```text
Path storage → temporary Ray[B] → six local float columns → triangle/ray loop → Hit[B]
```

Keep the existing AoS `Path` bookkeeping. Add a compact column input type containing origin x/y/z and direction x/y/z, and a `RayScene::intersect_columns` entry point. Fill those columns directly from active paths in the driver. Keep the legacy ray-array entry point as an adapter for tests/other callers and the existing advanced backends. A compile-time detection path can select columns only for backends that expose them; this avoids making every backend and scalar path adopt a new layout.

Move the existing intersection loop body unchanged into the column entry point. Retain triangle order, the determinant clamp, bitwise validity conjunction, the exact expression trees for cross/dot/u/v/t, and the strict `t < best_t` update. Keep nearest-hit t/u/v/id state private to the call. Preserve count semantics for 1/2/4/8 and finite zero-direction padding for inactive slots. All outputs for the supplied count must still be assigned before the driver reads them.

The smallest first version packs columns once per batch. Do not immediately maintain two persistent copies of the ray in both AoS state and separate columns: that adds synchronization obligations and changes too much at once. Do not simultaneously change output hits to columns, unroll two ray groups, remove inactive handling, or change division timing. Those are independent experiments.

If aliasing blocks vectorization, first inspect GCC's report. A disjoint output argument contract is valid when the driver supplies independent input/output objects; qualify the actual function interface if needed. Avoid overlapping scratch or applying a local pointer qualifier as an unverified hint. The earlier flat-scene investigation already showed why a locally qualified pointer can fail to communicate the intended interface contract.

**Why this is promising:** cycle attribution places intersection near 47% on P and 68% on E, and annotation shows stack-based YMM reloads. There is a concrete adapter boundary to simplify. However, reloads may reflect live-register pressure inside the arithmetic, and the compiler may already remove some source copies. The interface change is a hypothesis, not a guaranteed reduction in machine traffic.

**Validation and diagnosis:** compare fixed-ray hit IDs and t/u/v bits with the frozen matched backend, including empty scenes, triangle counts 1/7/8/9/17/36, ties, boundaries, inactive slots, and counts 1/2/4/8. Compare raw rendered pixels under strict and fast builds separately. Inspect packed arithmetic and stack traffic in the actual eight-ray specialization. If runtime regresses, reject it even if source copies disappear.

## 2. Reuse the ordered result vector per worker

`render_tile_refill` currently constructs `std::vector<Float3> results(jobs)` for every tile at line 220. Allocate one vector inside the worker, after binding, sized once to maximum tile jobs (`16 × 8 × samples`), and pass it to the refill function. Preserve allocation within the existing timed render scope. Allocate it only for refill mode, not every non-refill worker.

Keep the vector's logical size at that maximum and use only `[0, jobs)` for the current tile. Do not clear, assign, or resize it between tiles. Every valid job is issued exactly once and writes `results[job[r]]` when it finishes, including miss, emission, and depth-limit termination; accumulation begins after every issued job is finished. Therefore old entries cannot affect the new tile. In a debug correctness check, record one write per valid result and no unwritten reads; do not carry that instrumentation into the timed binary.

`reserve()` alone is insufficient: writing elements beyond vector size would be invalid. An uninitialized raw allocation is unnecessary. `Float3` has a zeroing default constructor, so merely switching to a default-initialized array would not necessarily remove initialization. A normally sized vector reused thereafter is compact and safe.

At the current full workload there are 800 full tiles. Each tile result vector is 192 KiB. Source-level construction initializes roughly 150 MiB of result storage across a render, whereas initial construction of 32 reusable worker buffers covers 6 MiB. These are logical initialization volumes, not measured DRAM traffic, and the normal result writes/ordered reads remain. The actual profile has not established allocation/zeroing as a major cost, so expect a modest ceiling and check whether GCC or the allocator already makes much of it cheap.

Keep tile dimensions and queue policy fixed for this comparison. Tail tiles require only the current `jobs` prefix, with unchanged local sample indexing. Cover widths 1, 7, 17, and a larger non-multiple of 16/8, with spp 1/7/8/9/64/128 as a small targeted selection rather than a full Cartesian product. Confirm raw pixels, ray counts, and sample-sum order. Tile-size experiments can follow after this change is isolated.

## 3. Profile-guided optimization with the same computation

Train the current best source and compile it with GCC's `-fprofile-use`, preserving the existing architecture and FP flags. Begin with one P worker on the real full refill workload; its branch distribution exercises the hot tracing computation without high contention on instrumentation counters. A few short runs of other supported modes can keep those paths represented. Score only the final profile-use binary, never the instrumented training executable.

Keep the source, options, object/output naming, and profile location consistent between generation and use. GCC profile filenames incorporate build/output paths; avoid silently losing feedback when naming the training and use binaries differently. Preserve missing/mismatched-profile diagnostics. For multithreaded training, use thread-safe counter updates; GCC selects `prefer-atomic` with `-pthread`, and explicit `atomic` is available. These are documented requirements, not renderer performance recommendations. [GCC instrumentation options](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html)

PGO may change inlining, branch layout, and loop/vectorization decisions. It does not require adding fast-math or changing the PCG. Keep those flags unchanged and validate outputs. A fast build permits contraction, so unchanged source expressions and flags alone do not prove bit-identical emitted arithmetic after recompilation. If PGO changes fast output, inspect the reason and do not silently classify it as an exact optimization. A separately trained/matched strict build provides the strongest operation-order check. [GCC optimization options](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#index-fprofile-use)

Judge PGO by whole-render time and the generated hot function, including code size/spills. Retain it only if its build/training complexity buys a repeatable gain across the relevant P/E configurations and tail cases. Do not claim a throughput estimate before measurement.

## 4. Combined sine/cosine, with an explicit equivalence gate

The sampler calls cosine and sine separately on the same float `phi`; the saved profiles show distinct libm calls. Their self shares total about 10.6% of P samples and 7.7% of E samples, excluding wrappers. This is material but bounded; even eliminating those self costs entirely would not double total rendering throughput.

Test one combined `sincosf` evaluation for the same `phi`, retaining the unchanged expressions `{r * cos_value, r * sin_value, z}`. GNU libc provides this extension for simultaneous evaluation; it is not portable standard C++ and its documentation does not promise the two outputs are bit-identical to separate calls on every implementation. Keep a fallback and test the actual linked libm. [GNU libc trigonometric functions](https://sourceware.org/glibc/manual/latest/html_node/Trig-Functions.html)

The numerical domain permits a strong focused test: `Random::uniform()` has exactly 2^24 possible float values. Exhaustively construct all 16,777,216 corresponding `phi` values with the **existing expression** `float phi = 2 * M_PI * u`, and compare the sine/cosine output bits against separate baseline calls. Protect the baseline side from compiler fusion, for example with separately compiled non-inlined wrappers and disabled builtin substitution for those wrapper calls; inspect that harness's call targets. Otherwise both sides might accidentally call the same combined function and make the check meaningless.

If the two trig outputs match over that domain, and the surrounding expression trees and draw sequence remain unchanged, the sampler's result matches locally for every possible second PCG uniform input under that tested math environment. Still validate the complete fast and strict renders. This is a local equivalence result for the tested compiler/library/environment, not a portable contract. If outputs differ, retain the original implementation for this exact-computation round and report the experiment separately; do not substitute a small image RMSE for the requested parity.

Do not replace the double `M_PI` expression with a float `2π` constant, change PCG extraction, combine/reassociate `1 - z*z`, approximate reciprocal/sqrt, use a different hemisphere estimator, or change the sign test. There is no per-bounce local basis here: the sampler constructs a sphere direction and flips it against the normal. Camera basis setup happens once.

## Exactness and measurement gates

For each source experiment, preserve the ordered triangle reduction, path identity, per-path PCG state/increment and draw counts, camera jitter expressions, scatter order, depth-limit behavior, miss/emission results, and sample summation in increasing sample index. Scheduling can change completion order only because result indices restore that summation order.

Use bitwise raw-float comparison against the frozen corresponding strict/fast binary, not HDR files alone. Check matching rays, completed paths, and live slots in addition to pixels. Keep batch/tail, worker-count, and std/OpenMP coverage focused on changed ownership/interfaces. For the candidate that wins screening, run the established full primary workload and core configurations. An unchanged raw image is necessary for this round but not evidence of faster computation by itself.

After root has results, Astra should audit a small fresh perf comparison of baseline versus selected winner: same explicit P/E PMUs, instructions/cycles/branches per completed path, active/reference cycles to control frequency, and exact event scope/runtime percentages. Use existing event definitions. Additional cache or topdown passes should answer a specific remaining question, not become a broad counter collection by default. No new DRAM or OpenMP redesign is motivated by the current measurements.

One optional later source observation: refill computes job/sample and pixel/tile quotient/remainder values for each new path. If annotation shows those integer divisions are material, explicit incrementing sample/x/y cursors can preserve IDs while avoiding division. Keep that out of the first matrix; it is a separate, unmeasured proposal.
