# Astra compiler audit: the no-STL performance gap

The frozen plain renderer generates a materially different intersection loop. Both programs use eight-wide AVX2 arithmetic; this is not evidence that STL storage is inherently faster. The strongest concrete difference is that GCC unrolled the plain triangle loop by two and generated substantially more stack traffic. Call/inlining boundaries differ as well. Small controls can test and potentially recover these costs while keeping raw arrays, malloc, OpenMP and the same estimator.

This was read-only inspection on 2026-10-14. Fresh SSH found clean HEAD `7659a2880884383613fd3dc47f5f0f576809bc5b`; no applicable remote AGENTS.md was present. I ran no compiler, renderer, profiler or heavy CPU job. Disassembly was read with objdump and saved locally as `frozen-quality.asm`, `frozen-plain.asm`, and focused `selected-*.asm` files.

Frozen binaries inspected:

- `build/pathtracer_quality`: SHA256 `2cbb26df11fc29cc8480c7dc04830f95ea9961291981715191a69efae2e268a2`, 760056 bytes.
- `build/pathtracer_plain`: SHA256 `eb91a325059140c8be118cd2c14585b9963a641ccd6bbe7d051458f1e563b8cb`, 129600 bytes.

The sizes are whole ELF file sizes, not hot instruction-cache footprints. The quality binary includes many unused batch/layout/runtime specializations, so total executable size is not a diagnosis of this hot loop.

## Trace the actual eight-ray path

For NEE, the original OpenMP worker calls `render_tile_refill<QualityScene<(Sampling)1>,8,PathsAoS>` at **0x68660**. Its ray intersection is **inlined into that tile function**, with a YMM division at 0x69090 and its triangle loop at **0x68fd8–0x691b6**. It calls a separate NEE scatter clone at 0xaf50.

For uniform, `render_image<RayScene,8,PathsAoS>::worker` starts at **0x229d0**. Its refill branch contains the inlined loop **0x2371f–0x238f6**, with the YMM division at 0x237cd. The branch at 0x22cda checks the refill option; the other section of this large function handles fixed-group rendering. Counting every division in the whole worker would therefore count different execution paths.

Plain NEE's OpenMP worker at 0x8870 calls `intersect_batch` at **0x25c0** from 0x9338. Plain uniform's worker at 0x17480 calls the same function from 0x17ebd. This out-of-line helper reserves 0x440 stack bytes, packs the eight AoS rays and writes the eight Hit records back through an output pointer.

Do not compare that helper with the original **four-ray** `.constprop.2` clone at 0xa0b0: that clone uses XMM division and a 0x140 frame. There is also an original **eight-ray** `.constprop.3` clone at **0xa550**, with YMM division, a 0x240 frame and a one-triangle loop at **0xa980–0xab6e**. It is a useful control even though the measured refill paths inline their intersections. Its existence shows that being out-of-line alone does not force the plain helper's larger/unrolled loop.

## Main-loop work: like-for-like static counts

| Frozen loop | Triangles per loop iteration | Assembly instructions | YMM stack reads | YMM stack writes | YMM divisions |
|---|---:|---:|---:|---:|---:|
| Original NEE B8, inlined | 1 | 83 | 12 | 5 | 1 |
| Original uniform B8 refill, inlined | 1 | 83 | 13 | 4 | 1 |
| Original B8 standalone clone | 1 | 84 | — | — | 1 |
| Plain B8 main loop | 2 | 179 | 49 | 22 | 2 |

The original standalone clone has 17 total stack-memory instructions, like the inlined loops. Exact address ranges and the other rows are recorded in `loop-counts.json`. Counts include the loop's branch and arithmetic instructions with memory operands. They are **static architectural instructions/references**, not measured retired counts, micro-ops, cache-line transfers or DRAM bytes.

Normalized per triangle, the plain main loop has **89.5 instructions versus 83**, and **35.5 YMM stack references versus 17**. With 32-byte operands that is 1136 versus 544 bytes of architecturally addressed stack operands per triangle in these main bodies. This does not imply 2.09× cache misses or time. Some locations are source arrays and some are compiler temporaries; do not label every stack reference a separate register spill.

The unroll is explicit: plain advances `rcx` by **0x90 = 144 bytes**, adds two to the triangle index, and evaluates two candidates before the back-edge 0x2f1a→0x2a80. The originals advance **0x48 = 72 bytes** and one index. Plain broadcasts and preserves two sets of geometry/temporaries, increasing live vector state on an AVX2 machine with a limited vector register file. Numerous broadcasts immediately stored to stack and later reread substantiate the increased temporary traffic.

There are four static vdivps sites in the plain function, but this is **not four divisions per triangle**. Two are in the two-triangle main loop and the other sites handle remaining candidates. With 36 triangles the main loop covers 34 and the two remainder paths cover the last two. Both programs still evaluate one vector division per triangle for an eight-ray query. The division arithmetic was not replaced or vector width reduced.

Both versions also pack rays and reconstruct hits. Inlining has not magically eliminated all packing in the original. The plain standalone function has additional ABI/prologue and stack-boundary work, but the repeated main-loop difference is stronger evidence than comparing stack-frame sizes: the original tile's frame also holds path scheduling state, so its full frame is not a comparable intersection-only number.

## Measured counts are consistent with extra work, not an STL tax

From the two recorded P-core observations per mode in `../round5/perf/counts.json`, comparing medians:

| Plain relative to current Quality | Cycles | Instructions | Branch instructions | Branch misses |
|---|---:|---:|---:|---:|
| Uniform | +8.37% | +5.55% | −12.62% | −2.82% |
| NEE | +6.06% | +0.77% | −2.03% | +3.30% |

Uniform has exactly the same 88,965,817 rays and 11,127,580 batches in these runs. NEE has 83,034,221 versus 83,034,226 rays and 50,030,382 versus 50,030,387 shadow rays, with the same batch count; fast-build rounding caused a tiny work difference that must still be disclosed. Active/reference-cycle ratios remain close (uniform 1.8711 versus 1.8684; NEE 1.8656 versus 1.8709). This does not look like a large clock-frequency explanation.

The uniform increase in instructions and lower branch count is consistent with the unrolled, more memory-heavy code. It does not isolate causality: only controlled builds can do that. There are no matched retired-load, L1-miss or detailed backend events in this small plain comparison, so do not invent a measured load-port or cache-stall diagnosis. The 60 strict pixel/work checks passed in the recorded validation; fast images differ slightly (uniform RMSE 1.13e-9; NEE 2.95e-8). Exact strict and fast numeric claims remain distinct.

## Other concrete code-shape differences

**Inlining boundaries.** The original defines Scene methods in-class, making them implicitly inline. Plain defines several large helpers as static free functions without inline. GCC may inline either form at O3, but the heuristic inputs differ. Observed NEE placement differs: original tile calls scatter, whose body integrates visibility and cosine generation; plain integrates scatter into its worker but calls both `visible` and `cosine_direction` separately. This changes values live across calls and opportunities for propagation. It does not mean every helper should be forced inline; a giant caller can create a new spill/code-size problem.

**Fixed inner count versus later specialization.** Plain writes `r<8` in its helper; original accepts `count` and later receives/propagates eight at the call site. Original's eight-ray standalone clone still has the smaller one-triangle body. Pass ordering and unroll/vectorization decisions are therefore plausible contributors beyond inline marking. The exact compiler-pass cause has not been inspected in a fresh dump because this audit did not compile anything.

**Scene representation.** Plain embeds 64 triangle slots and an integer count. Original stores a pointer/end/capacity triple for its 36 allocated triangles. The hot instructions in both cases broadcast p/e1/e2 from the same 72-byte stride; plain's helper loads its count from scene+0x1200, while original derives its count from pointer distance. Static storage is not slower by definition. A raw pointer-plus-count view can reproduce the old addressing shape without reintroducing any container, if the simpler controls fail.

**Construction and operation placement.** Plain Float3's default constructor is trivial; the old one zero-initializes. This intentionally avoids one-time result-buffer initialization, while inactive path and ray arrays are explicitly initialized. That can also alter optimization/code shape even if images agree. Uniform origin/ray/weight statements are placed differently around the sampler call; matching their original placement is a small control for value lifetimes, not a mathematical estimator change. Do not attribute a constructor-control timing result solely to the cost of zeroing; inspect the changed loop.

**Flags.** Makefile rules use the same O3, C++17, native architecture, fast contraction and OpenMP settings. Quality explicitly adds `-pthread`; plain uses `-fopenmp`, which already implies `-pthread` in GCC. That textual difference does not establish a missing thread/optimization flag. Both binaries contain AVX2/FMA and stack-protector checks. Full compiler-command provenance remains with the parent's build records; disassembly does not prove every build option.

## Small controlled experiments to close the gap

Use the frozen plain source as the common starting point and keep all containers absent. Preserve the estimator, camera expressions, PCG order, origin offsets, finite-depth logic and ordered sums.

1. **Inline only the batch helper.** Add ordinary `inline` to the static function, inspect whether the call actually disappears and whether the outer loop becomes single-triangle. If it changes nothing, an explicit always_inline build is a diagnostic next control, not permission to force every helper. Record stack-reference counts again.
2. **Restore the count parameter only.** Give plain intersect_batch the original int count and use it in the inner packing/intersection/output loops; pass eight. This tests the fixed-bound/later-specialization difference, with otherwise identical C arrays and arithmetic.
3. **Suppress only triangle-loop unrolling.** Use a local GCC unroll control or isolated function option, leaving the eight-ray inner loop vectorized. Check the generated code: pragma intent is not proof the pass complied. This is the most direct test of the demonstrated paired-triangle temporary traffic. Do not disable vectorization or optimization globally.
4. **Match the original helper boundary.** If batch changes leave a NEE gap, inline only visible/cosine into scatter while keeping scatter as a separate callable function, or use a small raw-array Scene method wrapper. These are alternative code-shape controls, not simultaneous changes. Inspect their call sites and live spills.
5. **Constructor and statement-order controls.** Compare original-style zeroing constructors separately from trivial constructors, and match uniform scatter statement placement separately. Keep per-worker results' initialization status explicit. The purpose is to identify an optimizer sensitivity, not to assume more initialization is intrinsically faster.
6. **Raw geometry view only if needed.** Expose `(const Triangle*, int)` to the hot helpers, retaining the same array owner. This removes the embedded-array versus vector-storage shape difference without allocating a vector. Preserve legal alias relationships; do not add restrict unless its nonaliasing contract is satisfied at every caller.

First score each isolated variant in the parent's short CPU8 screen, then retain only promising variants for the existing warmed interleaved full-render matrix. Require the existing strict pixel/work checks before ranking and separately quantify fast FP differences. A combined winner is useful, but keep isolated evidence so the explanation identifies what actually recovered speed. Use the same sampler and no new quality/geometry optimization while resolving this gap.

Primary compiler references: [GCC inline semantics and optimization](https://gcc.gnu.org/onlinedocs/gcc/Inline.html), including implicit inline for definitions inside a class; [GCC OpenMP options](https://gcc.gnu.org/onlinedocs/gcc/OpenMP-and-OpenACC-Options.html), documenting that fopenmp implies pthread. The decisive evidence here is the frozen machine code and recorded matching-work counters, not a generic language-performance claim.

## Isolated-variant assembly audit

The parent built seven source variants under `build/pass6`; I only read their sources and disassembled their binaries. `variants-metadata.json` preserves the parent's build metadata. `variant-*.cpp`, `variant-*.asm` and `variant-loop-audit.json` preserve the reviewed artifacts locally. No timing conclusion is made in this assembly section.

| Variant | Batch placement | Main loop address, NEE / uniform | Triangles per iteration | Instructions per iteration | YMM stack refs per iteration |
|---|---|---|---:|---:|---:|
| control | Out-of-line helper 0x25c0 | 0x2a80–0x2f1a, shared | 2 | 179 | 71 |
| inline_batch | Inlined into each worker | 0x6fc0–0x7439 / 0x1a5e0–0x1aa59 | 2 | 180 | 62 |
| inline_hot | Inlined into each worker | 0xa1d0–0xa649 / 0x1ae20–0x1b299 | 2 | 180 | 62 |
| old_ctors | Out-of-line helper 0x25c0 | 0x2a80–0x2f1a, shared | 2 | 179 | 71 |
| unroll1 | Out-of-line helper 0x25c0 | 0x2a80–0x2f19, shared | 2 | 178 | 71 |
| count_param | Out-of-line constprop helper 0x4f40 | 0x5400–0x589a, shared | 2 | 179 | 71 |
| inline_unroll1 | Inlined into each worker | 0x6fc0–0x7439 / 0x1a5e0–0x1aa59 | 2 | 180 | 62 |

All these main loops still have two YMM divisions per pair. The named `unroll1` variant does **not** establish that paired-triangle processing was disabled: the pragma left index increments of two and pointer increments of 144 bytes. Likewise, adding the original count parameter did not change the paired body in this experiment. The hypotheses that ordinary inline marking or a count parameter alone would restore the original one-triangle body are not supported by these generated variants.

Ordinary inline marking successfully removed the batch call and reduced stack references from 71 to 62 per pair, but retained the paired traversal. `inline_hot` additionally removed the separate visible/cosine calls in NEE; its worker now calls sincos directly. It still has the same 180-instruction/62-stack-reference intersection body, so a distinct NEE gain from that variant would concern the surrounding call/dataflow changes rather than elimination of the paired loop.

The old-constructors variant restores zero-initializing Float2/Float3 constructors and changes `Path p{}` back to `Path p`; it leaves the batch body unchanged. It also restores initialization work elsewhere, including the worker result buffer. If it changes performance, that result cannot be assigned to an improved intersection body because the inspected body did not improve.

Two reproducibility checks matter:

- `inline_batch` and `inline_unroll1` have byte-identical `.text`, SHA256 `eeecbf2103c5a0e751a918eb0f42d65bdab0a4410616d6df40ff185199246a85`, length 109746. A timing difference between them is not a different generated hot instruction stream.
- Frozen plain and rebuilt control have the same instruction sequence and instruction addresses after normalizing RIP-relative data addresses, but **different `.text` bytes**. The regenerated source/include location changes assertion strings and data layout. Their text hashes are `09920f14790dd55ceddff94781affd6c1f259d1b94042ea6c2d49d7169e723d0` and `3fcd7d5c432a7969a5331b61face151fb42c19736312e8d180a1a9038305d6fe`, respectively, both 95982 bytes. Keep control in the timing rotation rather than assuming source relocation has literally no binary effect.

A **true** one-triangle control is still missing at this point. The next targeted diagnostic is disabling GCC's `loop-unroll-and-jam` transformation, first with a separate build flag or narrow function optimization control, while checking the resulting loop and keeping vectorization enabled. GCC documents this O3 pass as unrolling an outer loop and fusing the resulting inner loops, matching the observed form. The exact responsible pass remains an inference until that switch or a compiler dump confirms it. [GCC optimization options](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)

## Confirmed no-jam controls and production identity

The parent subsequently built the targeted controls, completing the missing test. I read their sources, flags, binaries and `nojam-screen.json` without compiling or running workloads. Exact identities and loop records are in `nojam-variants-metadata.json` and `nojam-loop-audit.json`.

| Variant | Actual batch placement | Triangle loop(s) | Instructions / triangle | YMM stack reads + writes / triangle | YMM divides / triangle |
|---|---|---|---:|---:|---:|
| nojam: unchanged source, TU no-jam flag | Helper 0x25c0, frame 0x240 | 0x29d0–0x2bbe | 84 | 12 + 5 = 17 | 1 |
| batch_nojam: helper-only optimization attribute | Helper 0x25c0, frame 0x240 | 0x29d0–0x2bbe | 84 | 12 + 5 = 17 | 1 |
| inline_nojam: inline-hot source plus TU no-jam flag | Inlined into each worker | NEE 0x99b0–0x9b8f; uniform 0x196e0–0x198be | 84 | 12 + 5 = 17 | 1 |

Every row advances one triangle index and 0x48 bytes. These are now genuine one-triangle loops; SIMD remains eight-wide. Compared with the original plain pair, normalized static stack references fall from 35.5 to 17 per triangle, and instructions from 89.5 to 84. The standalone helper's 0x440 frame shrinks to 0x240. Plain no-jam therefore recovers essentially the same loop shape/count as the original eight-ray clone; the earlier `unroll 1` pragma did not control this transformation.

The global and scoped controls are particularly strong: **nojam and batch_nojam have byte-identical `.text`**, SHA256 `0101cc0d66fa6f705f36b0b8207a338851029f5c1f3fad29b62a65eca8b08c16`, length 93998. For this frozen source, the translation-unit flag's executable-code effect is reproduced by the attribute on the batch helper. Their different small-screen times cannot be credited to a different generated instruction stream. Their full ELF hashes differ, as expected for differently named builds.

The `inline_nojam` source is exactly the earlier `inline_hot` source: five ordinary inline declarations, with no source-level estimator change. Its command adds `-fno-loop-unroll-and-jam`. NEE integrates visibility and cosine work into the worker, leaving sincos library calls; the uniform sampler's separate call remains. Thus the combined result includes both the confirmed loop transformation and changed helper boundaries. It is not evidence that one inline keyword alone explains the whole gain.

### Screen interpretation

I recomputed these **process-wall medians** from repeats 1–3 in `nojam-screen.json`; repeat zero is the warmup. The workload is CPU8, 160×160, 128 spp, OpenMP, eight-ray refill. These are a small screen, not the final full-resolution score.

| Variant | Uniform wall seconds | NEE wall seconds |
|---|---:|---:|
| Frozen Quality baseline | 0.907483 | 2.088071 |
| Frozen plain | 0.983778 | 2.218176 |
| nojam | 0.924062 | 2.160603 |
| batch_nojam | 0.919856 | 2.158195 |
| inline_nojam | 0.902937 | 2.064438 |

The combined variant recovers the observed small-screen gap: about 0.50% less wall time than Quality for uniform and 1.13% less for NEE. Uniform ranges overlap, and only three scored observations exist. It is appropriate to say **“the plain implementation now matches the baseline in this screen”**, not to claim a general superiority. Against frozen plain, its observed throughput is about 8.95% higher for uniform and 7.45% for NEE. Full warmed interleaved renders and numeric checks remain the promotion evidence.

### Narrowest practical scope

The helper-only `optimize("no-loop-unroll-and-jam")` attribute is a valid diagnostic and produces the same code as global no-jam for the non-inlined version. It does not by itself reproduce the faster combined variant's helper integration. A further combination with inline declarations could be tested, but GCC may retain a call when caller/callee optimization options differ; source annotation alone would not prove the intended result.

For production, the parent's **plain-target-only, overridable `PLAIN_FLAGS ?= -fno-loop-unroll-and-jam`** plus the five ordinary inline declarations is simpler than additional wrappers or pragma regions. This is one build-target option, not a project-wide or CPU setting. It applies equally to plain fast and strict targets and can be cleared for a compiler that lacks the option. Keep its short explanation tied to the measured two-triangle temporary-pressure regression; do not generalize it as “unrolling is bad.” Re-audit after compiler changes or larger scenes.

### Production versus staged winner

The production source has SHA256 `dbf02d4d889d76189459df6c29eb5c53a861672771d457da91865581def58dfc`, exactly matching staged inline_nojam. Production binary SHA256 is `a8617b570b8292842907b3059bdbd6a67cd18d76d54fa5f963a9bc00d489e819`.

Production and staged winner have **identical normalized disassembly, including instruction addresses**, after replacing RIP-relative data offsets and comments. They have the same 104076-byte `.text` size, but not byte-identical sections:

- Staged inline_nojam text: `8b46db9bbda3f21b99e2dbeda557fea9aab4778229193e3609c01da34ce2244b`.
- Production text: `adfdedcee89e3e726aeda839582ac444e1c6e747be9a07411700f8a5a623a9d4`.

As in the source-location control, assertion strings/data addresses shift with header/source paths. Therefore the core code shape is carried into production, while final scoring should still use the actual production binary. `production-plain.asm`, `production-plain.cpp` and `production-identity-audit.json` preserve this check. No production timing or full-validation claim is made by this identity comparison.


## Final full-render, numerical and PMU audit

The completed evidence closes the original gap. This final section supersedes the earlier pending-validation and small-screen qualifications. I independently read `full-results.json`, `validation.json`, the twelve rows of `perf/counts.json`, `perf/summary.json` and the final `ROUND6.md` draft. I recomputed the unprofiled wall medians/ranges and the counter medians from the underlying rows. No renderer code, binary, build option or system setting was changed during this audit.

### Production timings

Each cell below is the median of five scored whole-process observations, following one warmup per configuration. The workload is 320×320, 128 spp, 13,107,200 completed paths, PCG seed 42, OpenMP, eight-ray AoS refill. P1 is logical CPU 8, a P-core; all32 uses CPUs 0–31. The scored binaries are frozen Quality, frozen initial plain and the actual fixed production plain target. These are unprofiled timings; do not substitute perf-launch wall time.

| Mode / CPU scope | Quality reference (s) | Initial plain (s) | Fixed plain (s) | Fixed time relative to Quality |
|---|---:|---:|---:|---:|
| Uniform / P1 | 3.602154 | 3.909628 | 3.611638 | +0.26% |
| Uniform / all32 | 0.273142 | 0.280206 | 0.274919 | +0.65% |
| NEE / P1 | 8.278475 | 8.827504 | 8.174291 | −1.26% |
| NEE / all32 | 0.561486 | 0.578039 | 0.550217 | −2.01% |

Uniform Quality/fixed ranges overlap: P1 3.583845–3.624133 versus 3.595878–3.642800 s; all32 0.271836–0.283685 versus 0.272067–0.275531 s. NEE fixed ranges are below Quality in this run: P1 8.150042–8.240327 versus 8.276984–8.394653 s; all32 0.545237–0.551089 versus 0.558868–0.568733 s. Five shared-workstation observations are not confidence intervals or a universal ranking. The justified conclusion is recovered uniform throughput and a small NEE improvement in this configuration, with no STL requirement for either result.

Every scored configuration has one stable image hash and stable work counts across repetitions. Fixed and initial plain have identical work: uniform 88,965,817 ray segments and 11,127,580 batch calls; NEE 83,034,226 ray segments, 50,030,387 shadow rays and 10,385,975 batch calls. Fast Quality NEE has five fewer ray segments and five fewer shadow rays, an existing floating-point difference rather than a new optimization in this recovery pass. Uniform work agrees exactly across all three.

### Numerical validation

All 62 strict checks report both pixel identity and work-count identity. They cover all four samplers, small image/tile/sample tails, 1/4/32 workers, and full-resolution uniform and NEE. The fixed fast uniform and NEE images are byte-identical to initial plain. They are not asserted to be byte-identical to fast Quality: the unchanged differences are uniform RMSE 1.1313e−9 / maximum 5.9605e−8 and NEE RMSE 2.9518e−8 / maximum 1.6198e−5. Strict output is exactly equal to Quality in the tested cases.

Thus “same computation” is supported by unchanged source arithmetic/estimator/PCG/accumulation plus strict pixel/work parity and fast parity against the initial plain executable. It should not be rewritten as universal bit parity between all fast builds or proof over every input.

### Fresh P-core counters

The final diagnostic has twelve rows: two observations for each of three versions and two samplers. All have 13,107,200 completed paths. They use explicit `cpu_core` user-space events on CPU 8, with recorded running fractions of 99–100%; these are the reported/scaled perf counts, not an assumption of perfect simultaneous measurement. The launch chain includes taskset and runuser before the renderer. Startup/launch and raw-output work are included, so this is a whole-command user-event accounting rather than a scoped inner-loop measurement. The rendering dominates these multi-second runs. No E-core or mixed-PMU IPC is being combined here.

| Mode/version | Cycles/path | Instructions/path | IPC | Retired loads/path | Retired stores/path | Active/reference cycles |
|---|---:|---:|---:|---:|---:|---:|
| Uniform / Quality | 1631.2 | 4387.6 | 2.690 | 1461.4 | 447.6 | 1.8641 |
| Uniform / initial plain | 1780.3 | 4632.0 | 2.602 | 1651.9 | 657.5 | 1.8655 |
| Uniform / fixed plain | 1631.9 | 4384.5 | 2.687 | 1423.6 | 441.6 | 1.8708 |
| NEE / Quality | 3771.4 | 10741.5 | 2.848 | 3566.2 | 650.8 | 1.8703 |
| NEE / initial plain | 4017.3 | 10824.8 | 2.695 | 3448.0 | 773.6 | 1.8753 |
| NEE / fixed plain | 3707.7 | 10349.9 | 2.791 | 3277.3 | 442.5 | 1.8685 |

Relative to initial plain, fixed uniform reduces cycles/path by 8.33%, instructions by 5.34%, retired loads by 13.82% and retired stores by 32.84%. Fixed NEE reduces them by 7.71%, 4.39%, 4.95% and 42.81%, respectively. Old/fixed active-to-reference ratios differ by less than 0.4%, supporting lower execution cost rather than a material frequency advantage. The ratio is an activity-weighted frequency indicator, not an absolute GHz reading or proof that instantaneous clocks and package conditions were identical.

Uniform fixed cycles/path agree with Quality within 0.044%; instructions/path within 0.071%. NEE fixed uses 1.69% fewer cycles and 3.65% fewer instructions than Quality. Its IPC is lower than Quality despite its lower elapsed time and cycle count; IPC by itself is not the optimization objective.

The counter reduction agrees with the demonstrated smaller temporary live set and fewer static stack operands. It does not separately apportion the gain between no-jam and helper integration, and it does not mean every removed store was a register spill. Retired load/store events count instructions, not vector bytes, L1 transactions, cache misses or DRAM bandwidth. No cache-stall attribution follows from these counters.

Nor is this a branch-prediction win: fixed uniform has about 15.42% more branch misses/path than initial plain, and fixed NEE about 0.90% more. Reduced total execution cost outweighs that counter change. A single branch statistic cannot explain this code-generation result.

### Final assessment

I found no unresolved material correctness or provenance issue in the final data or `ROUND6.md` interpretation. The measured explanation is sufficiently concrete: the original no-STL program exposed a different GCC optimization decision; a targeted pass control restored one-triangle traversal, while ordinary inline declarations recovered useful hot-helper boundaries. The paired source/flag controls, actual eight-ray assembly, production identity check, strict validation, full unprofiled timing and fresh counters all support that conclusion.

Keep the implemented plain-target-only, overridable flag and five inline declarations. There is no evidence here for reintroducing containers, additional manual SIMD, allocator rewrites, changed sampling or a broad compiler-disable policy. The result supports the user's expectation that the same renderer can keep its speed without STL; it does not establish that STL itself is inherently faster or slower. Revisit this specific compiler control when the compiler, scene or batch width changes.
