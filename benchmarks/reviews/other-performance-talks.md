# Performance talks applied to tiny-pathtracer

Reviewed 2026-10-13. The most useful direction is to retain the simple C++ eight-path refill loop and investigate its remaining costs: packing and spills, dependency overlap, sampler work, and compiler decisions. These talks explain why the current winner is plausible; they do not establish its hardware bottleneck or promise a further speedup.

The task is to execute the existing estimator faster: 36 Cornell triangles, front-face intersection, uniform hemisphere sampling, the existing 24-visit limit, identical per-path PCG streams, and ordered sample accumulation. Better sampling, different termination, and approximate math are outside this review.

## Evidence and source quality

I read relevant portions of the actual timed captions from the CppCon recordings, including their code demonstrations and questions. This is a focused review, not a claim to have watched every minute. Captions were retrieved with caption-only `yt-dlp` requests; no videos were downloaded. Primary conference recordings are linked below. Automatic captions can misrecognize technical names, so the analysis paraphrases concepts rather than quoting them.

| Talk | Content actually read | Evidence status |
| --- | --- | --- |
| Fedor Pikus, **Branchless Programming in C++**, CppCon 2021 | Pipeline/dependency explanation; measurement; alternating conditions; short-circuit expressions; extra work from branch elimination; predictable versus unpredictable examples | Original English automatic captions. [Recording](https://www.youtube.com/watch?v=g-WPhYREFjk). The conference calls its slide deck *Branchless Computing: Why Conditions Are Bad for Your Code, and What Can You Do About It*. |
| Chandler Carruth, **Tuning C++: Benchmarks, and CPUs, and Compilers! Oh My!**, CppCon 2015 | Benchmark setup; code deletion/inlining example; escape barriers; profiler callgraphs and assembly; modulo example and sampling attribution | Manual English captions. [Recording](https://www.youtube.com/watch?v=nXaxk27zwlk). This caption track has omissions and an explicitly marked misplaced-subtitles block. Its themes are usable, but this report supplies no precise timestamps for it. |
| Chandler Carruth, **High Performance Code 201: Hybrid Data Structures**, CppCon 2016 | Small buffers and interfaces; compact values; pointer-array versus linked traversal; deterministic ordering; small linear scans; key/value separation question | Original English automatic captions. [Recording](https://www.youtube.com/watch?v=vElZc6zSIXM). |
| Jefferson Amstutz, **Compute More in Less Time Using C++ SIMD Wrapper Libraries**, CppCon 2018 | SAXPY and trigonometric demonstration; masks; AoS/SoA/AoSoA; independent-ray vectorization; partial-lane question | Manual English captions, cross-checked against automatic captions in the relevant sections. [Recording](https://www.youtube.com/watch?v=8khWb-Bhhvs). |

Timestamp links below were checked against content in the timed caption files. They are caption anchors, with normal caption timing precision. No slide-only assertion is presented as a verified spoken statement, and no claims here depend on a talk abstract alone.

## What the project has already measured

These are project results from [COMPUTATION.md](../../COMPUTATION.md) and [scaling-analysis.md](../../SCALING.md), not measurements made by this review:

- Ordinary C++ vectorization over eight replenished paths beats the tested explicit ray kernels on one P core. Explicit intrinsics are not a demonstrated prerequisite for the fastest implementation.
- Fixed groups have 42.26% software live-slot occupancy; refill reaches about 99.94%. This counts useful ray slots, not hardware vector utilization or execution-unit occupancy.
- Whole-path component SoA was about 2.1% slower than AoS through the tested get/set and intersection adapter. That experiment does not isolate a direct column interface without packing.
- The final 320×320/128-spp sweep reports 7.552× whole-process speedup for eight P cores, 15.86× for sixteen E cores, and approximately 45.91 million completed paths/s across all 32 logical CPUs. OpenMP and `std::thread` are effectively tied under the matched protocol.
- Geometry's useful intersection coefficients occupy 1,296 bytes. The ordered result buffer is 192 KiB per full 16×8 tile at 128 spp. The scene fits within L1 by capacity; the completion buffer exceeds L1 and fits within the stated L2 capacities. These sizes do not establish hit rates.
- Strict builds matched the tested reference exactly. Permitted FMA contraction and affine arithmetic can change a few geometric decisions. Preserve each numerical configuration separately when comparing organization changes.

The machine is an i9-14900K with 8 P cores, 16 E cores, 32 logical CPUs, AVX2/FMA, and GCC 15.2. The project documents hardware topology; none of these older talks measures this hybrid processor. New hardware-counter findings being collected separately should supersede any bottleneck hypothesis here.

## Four useful connections

### Pikus: branches, dependency chains, and extra work

At [08:57](https://www.youtube.com/watch?v=g-WPhYREFjk&t=537), the explanation distinguishes operations that depend on previous results from independent streams that can overlap. At [40:00](https://www.youtube.com/watch?v=g-WPhYREFjk&t=2400), a compound short-circuit condition can expose unpredictable intermediate decisions even when its final result is predictable. At [49:49](https://www.youtube.com/watch?v=g-WPhYREFjk&t=2989), replacing branches evaluates more expressions. The [55:04](https://www.youtube.com/watch?v=g-WPhYREFjk&t=3304) demonstration changes the condition to be predictable and reverses the advantage.

This is directly relevant to triangle rejection. An early-exit scalar candidate can perform fewer operations while making execution irregular; a regular eight-ray loop can perform rejected arithmetic while offering independent work. Neither candidate count nor branch count selects the winner. Preserve the measured simple loop until profiling identifies a specific costly decision. Test selective rejection against its full arithmetic and mask costs rather than applying branch removal everywhere.

The useful latency experiment is to offer another independent ray packet while retaining each ray's operation order. It is not to reorder one path's dependent bounces. This is a hypothesis about available overlap, not a measured claim that the divider is saturated.

### Carruth, Tuning: benchmark the generated program

The vector/reserve example appears dramatically faster because different inlining decisions allow the optimizer to delete work. The escape-barrier demonstration preserves the benchmarked operations while permitting optimization inside them. Profiling then proceeds from functions and callgraphs to generated instructions. The modulo example also shows that a sample attributed after a costly instruction need not identify the actual cause; simply increasing sampling frequency does not solve that imprecision. [Primary recording](https://www.youtube.com/watch?v=nXaxk27zwlk).

The existing whole executable, which writes an image, is a useful anchor. An isolated intersection or sampler benchmark still needs observable outputs and representative inputs. A checksum or output escape belongs at the benchmark boundary; placing barriers in every candidate or lane can obstruct the optimization being studied. Inspect compiler reports and assembly before attributing a source-layout improvement to SIMD, instruction overlap, or fewer loads.

For new profiles, inspect neighboring instructions around a hot sample. A store or branch immediately following division is not automatically the bottleneck. Keep any profiling-specific build distinct from the build used to rank throughput.

### Carruth, Hybrid: choose representation around operations

At [13:53](https://www.youtube.com/watch?v=vElZc6zSIXM&t=833), compact values matter to making small containers effective. The [17:17](https://www.youtube.com/watch?v=vElZc6zSIXM&t=1037) traversal example distinguishes a flat array's visible future addresses from a linked chain whose next address depends on the current load. At [35:24](https://www.youtube.com/watch?v=vElZc6zSIXM&t=2124), small linear scans can be preferable to a more elaborate lookup structure. In the [42:07](https://www.youtube.com/watch?v=vElZc6zSIXM&t=2527) key/value separation question, the answer depends on which fields a lookup actually examines.

That last point is especially useful for the AoS/SoA result. Intersection consumes common origin/direction columns; scalar scatter consumes most of one path's state. A globally uniform layout can make one stage more convenient while imposing conversion on another. Test the hot interface, not a layout label.

For 36 triangles, a tiny regular scan can remain competitive with more elaborate culling despite evaluating more candidates. This does not prove that flat traversal beats a BVH for other scenes. Keep a geometry representation that exposes independent candidates and a path representation that keeps refill and scatter simple.

### Amstutz: independent rays and math-library limits

The [40:16](https://www.youtube.com/watch?v=8khWb-Bhhvs&t=2416) section describes AoS, SoA, and small SoA blocks; [47:03](https://www.youtube.com/watch?v=8khWb-Bhhvs&t=2823) recommends SIMD over independent items, followed by an origin/direction ray example. That maps directly to eight separate paths rather than trying to use an entire SIMD register for one `Float3` dot product.

At [38:10](https://www.youtube.com/watch?v=8khWb-Bhhvs&t=2290), the trigonometric demonstration explicitly uses different algorithms in the vector math implementation. Its speedup supports investigating math calls; it does not establish numerical equivalence to this renderer's sampler. At [56:07](https://www.youtube.com/watch?v=8khWb-Bhhvs&t=3367), ignoring lanes does not make the vector arithmetic finish sooner merely because fewer outputs are written.

This gives a precise interpretation of refill: keeping lanes occupied can remove wasted packet work, while a mask alone cannot promise the same gain. If sampling becomes a significant profile share, first batch its independent algebra while retaining the existing scalar math results and exact PCG draw sequence. Replacing sine/cosine algorithms is a separate numerical change.

## Experiments worth doing, in order

All entries below are proposals, not measured improvements. Retain identical pixel/sample identities, PCG states and draw counts, camera jitter, triangle ordering and closest-hit tie behavior, termination, and final sample-sum order. Compare exact outputs against the matched reference within each floating-point build configuration, including partial tiles and packet tails. Reject or separately classify a change that alters path decisions.

| Priority | Small experiment | What would support the hypothesis | What would argue against it |
| --- | --- | --- | --- |
| 1 | Attribute the winner's remaining time to intersection, scatter/sampling/RNG, packing, result-buffer handling, and setup. Normalize counters by completed paths and live ray segments. | A stable large cost in one stage identifies the next change and its maximum plausible benefit. | Inconsistent attribution, instruction-sample skid, or a cost dominated by another stage. |
| 2 | Give the existing C++ intersection loop a direct origin/direction column interface, retaining AoS for scalar path bookkeeping. Compare with the current adapter using the same loop body. | Fewer copies, stack traffic, or spills in assembly, plus a repeatable throughput gain. | The adapter was already optimized away, or extra live state increases spills. |
| 3 | Reuse the ordered tile-result allocation once per worker, with the same indexed results and sum order. Separately compare a few nearby tile sizes. | Reduced setup/initialization cost or improved locality with unchanged results. | Allocation is negligible, or smaller tiles increase queue/tail overhead. |
| 4 | Try GCC profile-guided optimization on the unchanged source and FP options. Train the actual Cornell workload; evaluate the primary render plus smaller and tail cases. | Better generated code and repeatable gain from a build-only change. | Profile mismatch, code growth, or a gain limited to a tiny training case. |
| 5 | If the profile indicates dependency stalls, compare eight active paths with two eight-lane packets available for interleaving. Keep plain C++ first. | Shorter exposed stalls or more completed work per core without excess spills. | Register pressure, instruction footprint, or packing grows faster than overlap helps. |
| 6 | If sampler cost is material, batch only its independent weight/vector algebra first; leave original scalar trigonometry and PCG draws intact. | A vectorized stage reduces total render time while matching the reference. | The stage is too small, math calls dominate, or numerical outputs differ. |
| 7 | If a specific candidate rejection branch is costly, compare its current implementation with one local alternative: regular masks or a cheap whole-packet empty rejection. | The saved work exceeds added mask, control, and dataflow costs. | Rejection is already predictable or the added staging delays useful arithmetic. |

For proposal 2, valid non-aliasing information is an interface contract, not a hint to add everywhere. Use `__restrict__` only where actual ownership makes the outputs disjoint, and pass immutable loop bounds directly. The existing flat-kernel result already demonstrates that source shape mattered to GCC's vectorizer. [GCC's restricted-pointer documentation](https://gcc.gnu.org/onlinedocs/gcc/Restricted-Pointers.html) supports the contract; attributing that specific GCC result to either Carruth talk would be an overreach.

For proposal 4, PGO is our additional experiment, not a technique demonstrated to improve this renderer by the four recordings. GCC documents profile feedback for branch probabilities, inlining, loop transformations, and vectorization, and diagnostics for missing or mismatched profiles. Preserve those diagnostics instead of suppressing them. [GCC optimization options](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#index-fprofile-use).

Arithmetic scheduling needs a narrow definition: move independent computations so their latency can overlap, while retaining each expression's operations and contraction policy. Reassociating dot products, replacing division with reciprocal multiplication, changing triangle transforms, or switching transcendental algorithms can change rounding and hit decisions. GCC documents that reassociation can change results and that reciprocal substitution loses precision. The existing FMA/strict comparison remains a separate numerical experiment. [GCC arithmetic options](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#index-fassociative-math).

## Read hardware evidence at the appropriate scope

Geometry's 36 useful bytes per triangle are coefficient bytes, not 36 DRAM bytes per query. Reuse across eight rays gives an ideal 4.5 useful coefficient bytes per ray/triangle pair, but broadcasts, stack spills, cache-line traffic, and copies are additional costs. A warm L1-resident scene can still stress load ports or registers. A 192 KiB completion buffer can create traffic at another cache level. Report L1/L2/LLC/DRAM observations separately; a single arithmetic-intensity number must name its memory boundary.

For hybrid scaling, retain separate P, E, P-with-SMT, mixed-physical, and all-logical results. Normalize each group's counters by actual work, and interpret events using that core type's PMU. Record frequency/power evidence for a loaded package before attributing the mixed-group shortfall to a shared queue or memory contention. An isolated P-core rate, an E-core rate, and an SMT-pair rate are useful calibration points; their sum is a model, not a package-wide guaranteed limit. The already near-linear P/E sweeps and OpenMP/std tie make a scheduling rewrite a lower-priority hypothesis than a profile-guided change to the current computation.

The next concrete pass should therefore be: use the current hardware profile to select one stage; inspect the corresponding generated loop; test the direct column interface or worker-buffer reuse if those costs are visible; then try PGO on unchanged source. Retain the eight-path refill design unless an equally simple controlled comparison establishes a better choice.
