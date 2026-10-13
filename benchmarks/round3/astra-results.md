# Astra audit: third optimization round

The two small source changes improve the measured renderer, and PGO adds another repeatable gain. The final fast build completes **50.09 million paths/s** on all 32 logical CPUs with std::thread, up **9.18%** from the frozen baseline in this round. One P core improves **15.82%**. OpenMP reaches 50.01 million paths/s and remains practically tied with std::thread.

The retained source changes are worker-owned reuse of the ordered tile-result vector and a combined sine/cosine call on the tested GNU libc platform. The ray kernel, scalar arithmetic expressions, RNG definition, estimator, termination, triangle tie order, and pixel-sum order remain in source. The experimental column interface was removed after failing to establish a throughput improvement. Optional PGO changes compiler decisions, rather than adding renderer complexity or changing arithmetic flags.

## Timing and scaling

`final-results.json` contains five warmed, interleaved observations of each of 21 configurations: three builds across seven CPU/runtime configurations. Each scored render completes 13,107,200 paths and records 88,965,817 live ray segments. The table uses whole-executable wall-time medians. “Simple” means both retained source changes; “PGO” means those source changes plus profile-guided compilation. Gains are throughput ratios, not percentages of time removed.

| Configuration | Baseline s | Simple s | PGO s | Simple gain | PGO gain |
|---|---:|---:|---:|---:|---:|
| One P | 3.87620 | 3.61895 | 3.34687 | 7.11% | 15.82% |
| One E | 11.11476 | 10.89107 | 10.27261 | 2.05% | 8.20% |
| Eight P | 0.51241 | 0.47759 | 0.44365 | 7.29% | 15.50% |
| Sixteen E | 0.69914 | 0.68566 | 0.64997 | 1.97% | 7.56% |
| 24 physical cores | 0.32793 | 0.31385 | 0.29597 | 4.48% | 10.80% |
| 32 logical CPUs, std | 0.28569 | 0.27541 | 0.26167 | 3.73% | 9.18% |
| 32 logical CPUs, OpenMP | 0.28574 | 0.27415 | 0.26211 | 4.23% | 9.02% |

PGO alone, relative to the new simple build, adds 8.13% throughput on one P core, 6.02% on one E core, and 5.25% across all logical CPUs with std. The total 15.82% P gain corresponds to 13.66% less time; the total 9.18% all-CPU gain corresponds to 8.41% less time. Do not add sequential percentage gains arithmetically.

Within each tested configuration, the five observed baseline/simple/PGO wall-time ranges do not overlap. This supports the ranking under this protocol; it is not a statistical confidence interval or a promise for another scene. The PGO std/OpenMP ranges overlap, and their medians differ by only about 0.17%, so the data do not establish a threading-runtime winner.

Using each build's matching isolated core baseline, PGO scales 7.544× across eight P cores, or 94.30% of the homogeneous linear ideal. It scales 15.805× across sixteen E cores, or 98.78%. These are essentially the same strong homogeneous scaling pattern as before. The smaller all-CPU percentage improvement does not imply a broken tile queue: P and E gains differ, SMT shares execution resources, and the prior reference-cycle measurements showed lower active clock rates under package-wide load. This round's actual counters must be used before attributing its remaining gap quantitatively.

The calibrated E/P rate ratio also changes: it is 0.3487 in this round's baseline and 0.3258 with PGO. Therefore reuse of the old weighted-core ideal would be inappropriate. A simple 24-core additive ideal from the PGO isolated wall-time rates predicts 0.25330 s versus the observed 0.29597 s, about 85.59% of that model. It is an optimistic model that assumes isolated rates persist; it is not measured hardware utilization or a physically guaranteed limit. No new isolated SMT-pair calibration was collected here, so this report does not invent a new weighted 32-thread ideal.

The initial column and buffer-only screening had three observations each. It did not isolate a clear column win, and the refactored ray adapter itself was slower in that screen. Restoring the original ray implementation and keeping the rejected experiment as patches/data is the appropriate simplicity decision. The final source comparison measures the buffer and trig changes together; it does not assign the entire simple-build gain to either one.

## Numerical scope

`final-validation.json` contains **56 comparisons total: 28 strict and 28 fast**, covering seven size/batch/refill cases, two candidate builds, and std/OpenMP. All 28 strict comparisons are bit-identical to the matching frozen strict baseline. Both runtimes also produce identical outputs for each candidate/configuration. These are tested cases, not an assertion that unrelated implementations or arithmetic configurations are universally interchangeable.

The exhaustive local trig check compared all 2^24 possible sampler-angle inputs with the original double-expression-to-float construction. Separate sine and cosine output bits matched the combined call for the tested compiler/libm domain. That validates the library substitution locally, but does not imply bit-identical whole-program output after recompilation with FMA enabled.

At 320×320/128 spp, compared with the frozen fast baseline:

| Fast candidate | Changed float components | RMSE | Maximum absolute difference | Raw SHA-256 |
|---|---:|---:|---:|---|
| Simple | 12,821 | 1.16746e-9 | 5.96046e-8 | `8774f305dbdb0aab2a4a8e577b94355c0496ae5691cd3306bc1373473617a307` |
| PGO | 23,121 | 2.04741e-9 | 1.19209e-7 | `f0b692008987d026ce7cee77804cf2ac0658a47850d3e82df3e113c14ee9a3f8` |

All fast candidates retain the same total ray count, 88,965,817. That total alone does not prove every path made identical hit decisions. The tiny differences are consistent with changed permitted contraction/code generation; no operation-level trace here isolates every cause, so do not describe them as a proven error solely in path weights or in a particular FMA. Exact trig component equality rules out differences in those tested sine/cosine values themselves.

The strict full image retains SHA-256 `7be83a5ed68a5160c31cfedc7ae650250b7d8e9906fa7926988c16de62bb4cab` and 88,965,787 ray segments. The supplemental strict timing matrix has three observations per configuration. Strict PGO improves whole-process throughput by 12.57% on one P core (4.23043 → 3.75789 s) and 7.00% across 32 logical CPUs (0.29257 → 0.27343 s), while preserving that tested exact result. One strict P baseline observation is 5.451 s versus the other two near 4.23 s; the median remains informative, but this small supplemental set should not be presented with unwarranted precision or confidence.

## Build and profile provenance

The frozen baseline is commit `47ced90`. The variant manifest associates each candidate with its own source hashes and binary hash, avoiding the mistake of assigning the final header hashes to the old executable. I checked every fast timing row's binary hash against the manifest or its PGO metadata; they match.

Fast and strict PGO were trained separately with matching `-ffp-contract=fast` or `off` flags. Both metadata files record the generation/use commands, atomic instrumentation updates, training workloads, profile hashes, source hashes, and final binary hash. Missing-profile and coverage-mismatch diagnostics are errors; the recorded successful builds have no diagnostics. No fast-math option was added. Training concentrates on this Cornell refill workload; the measured gain is not established for untrained kernels or unrelated scenes. Instrumented training runs are not performance scores or numerical reference binaries.

`profile-identity.json` confirms byte-identical production/debug `.text` for both the frozen baseline and final fast PGO binary. Their text hashes are respectively `db9fdbc0747070f72764ece395b9ac54fe609be7619f0afaf3e009a6344cb707` and `9e0a58d03dfa1af647cf9fe0a3decff3065dce6fe4b78527c2961eeec752ebbf`. Thus the sampled comparisons describe the scored instruction streams, with the usual sampling and scope limits.

## Perf interpretation and next experiment

The completed `perf/stat.json` contains three basic-counter observations for each of the six build/core combinations. Explicit `cpu_core` or `cpu_atom` user-mode events avoid conflating hybrid PMUs. The launcher is pinned before `runuser`; the renderer also pins its worker. Counters report about 99% running on P and 100% on E, so multiplexing is small. Process-scope counts include a little launcher/startup/output work; normalization is by the same 13,107,200 completed paths. These are separate diagnostic runs, not replacements for the unprofiled timing matrix.

| Core/build | Cycles/path | Instructions/path | IPC | Branch misses/path |
|---|---:|---:|---:|---:|
| P baseline | 1,759.51 | 4,480.12 | 2.546 | 7.920 |
| P simple | 1,642.97 | 4,362.46 | 2.655 | 5.676 |
| P PGO | 1,518.41 | 4,160.61 | 2.740 | 6.144 |
| E baseline | 3,786.09 | 4,479.75 | 1.183 | 8.594 |
| E simple | 3,714.62 | 4,362.08 | 1.174 | 5.978 |
| E PGO | 3,513.43 | 4,160.23 | 1.184 | 6.280 |

Compared with the frozen baseline, final PGO retires about 7.13% fewer instructions on both core types. P cycles fall 13.70% while IPC rises; E cycles fall 7.20% while IPC remains approximately unchanged. Active/reference-cycle ratios remain approximately 1.869 on P and 1.408656 on E across the three builds. This supports an improvement from executing the compiled workload more efficiently, rather than a higher measured active clock ratio. It does not provide a new frequency measurement under all-core load.

The source changes substantially reduce branch count and misses. PGO subsequently reduces total instructions and branches again, but **increases P branch misses 8.24% relative to the new simple build**, while reducing cycles. Its higher 5.19% miss rate versus 3.80% also has a smaller branch-count denominator. Therefore neither “PGO wins solely by predicting branches better” nor “more branch misses imply slower rendering” fits these measurements. The data are consistent with inlining, specialization, scheduling and other code-generation changes; they do not isolate the contribution of each compiler transformation.

The dedicated P load-counter pass in `details.json` records 19.547 billion retired load instructions in the baseline and 18.041 billion with PGO, a 7.70% reduction. This is useful evidence of less executed load work for the same completed paths. It is **not a byte count**: instruction widths, split accesses, stores, prefetches and cache-line transfers are not recovered from this event. Retired load instructions with an L1 miss are only about 214 thousand and 196 thousand, respectively, roughly 0.0011% of the corresponding retired-load counts. That narrow observation supports little demand-load miss pressure in these runs; it does not measure all L1 traffic, prove every byte resides in L1, or establish DRAM bandwidth. E's load-uop counters use a different unit from P's retired-load-instruction counters and must not be directly compared as equivalent loads or bytes. No new controller-bandwidth measurement was made in this round.

P floating-point-divider-active counts are 2.807 billion baseline and 2.853 billion PGO, **1.63% more absolute activity** despite fewer total cycles. Relative to their matching detailed-pass cycle counts, that is 12.17% and 14.32%. PGO E reports 20.84%. These events indicate activity while the FP divide/square-root unit is occupied; they are not a removable fraction of stalled execution and cannot predict an equal speedup from replacing division. In particular, an increased activity fraction here mostly coexists with a shorter overall execution, rather than showing a newly demonstrated slowdown in division.

P Topdown L1 changes from baseline to PGO as follows: retiring 46.5% → 50.2%, bad speculation 22.0% → 18.5%, frontend bound 4.1% → 2.2%, backend bound 27.3% → 29.2%. These are slot fractions from separate diagnostic runs. The total slot count drops from 138.27 billion to 119.45 billion; the larger backend fraction does not imply more absolute backend work or a DRAM bottleneck. The reported raw backend slot count actually drops. This comparison is against the frozen baseline; no Topdown pass for the new non-PGO simple build was collected, so it cannot explain the incremental PGO gain alone.

### Where time remains

The 499 Hz cycle samples contain 2,058 baseline-P, 1,680 PGO-P and 5,134 PGO-E samples, with no lost samples reported. DWARF inline attribution allows a more useful breakdown than the outer worker symbol alone:

| Inclusive sampled region | Baseline P | PGO P | PGO E |
|---|---:|---:|---:|
| `RayScene::intersect_batch` | 47.19% | 52.68% | 74.54% |
| `scatter` | 36.02% | 35.75% | 20.21% |
| `sample_hemisphere`, nested inside scatter | 22.17% | 23.85% | 13.75% |

Do not add inclusive rows: the sampler is part of scatter and other helpers are nested again. The intersection region itself includes its input packing, arithmetic and nearest-hit bookkeeping, rather than only triangle math. PGO's worker self share rises to 87.97% on P and 92.79% on E because scatter and sampler have been inlined into it. Those percentages are **not** whole-intersection shares. The remaining separate combined-trig implementation has self shares 11.29% on P, plus 0.66% at its PLT entry, and 7.18% on E.

The baseline sampled render lasted 4.1125 s, slower than its primary and basic-stat observations; final PGO sampled times were about 3.3526 s P and 10.2711 s E. Consequently these finite-sample shares describe approximate stage composition, not exact saved seconds. Multiplying a share from one profiled run by an unrelated unprofiled median would overstate precision. Likewise the new combined-trig share cannot be compared with the old separate sin/cos share to conclude trig became slower: attribution and the execution-time denominator changed.

The practical interpretation is that PGO removes instruction and load overhead, and lets P cores execute the remaining instruction stream at higher IPC. The ordinary C++ intersection remains the largest measured region, particularly on E cores. Scattering and sampling remain material on P cores, but PGO has already inlined their main path. There is no present evidence for another broad layout rewrite, forced-inline policy, cache-padding change or approximate reciprocal substitution.

### One next small experiment

The next low-risk source experiment would be an **incrementing sample/pixel cursor inside `render_tile_refill`**, replacing the per-path division/remainder mapping from `job` to sample, x and y. Keep `job[r]` unchanged for the result index; advance a sample counter, then x and y at the same row boundaries, and seed PCG with exactly the same global pixel/sample identity. Retain the current camera floating-point expressions and ordered sums. This removes repeated integer address/identity computation without changing the ray kernel, estimator or sampler, and adds only a few integer variables and predictable boundary checks.

This is a conditional, modest opportunity, not a promised large gain. First inspect the final PGO instruction stream: if specialization has already eliminated those divisions, the experiment has little rationale. The complete refill lambda accounts for about 4.79% of P and 2.70% of E samples, and includes indispensable camera work, so only a subset is available to remove. Extra cursor branches could offset the saved arithmetic. Test it against this frozen final PGO binary, train its profile separately with the identical protocol, require strict full-image equality, report any fast-build differences, and score the same five-run P1/E1/32-thread comparisons. If the gain is not reproducible, keep the current smaller source. This round already delivers the substantial, validated gain without that follow-up.
