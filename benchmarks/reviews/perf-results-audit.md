# Hardware-counter and sampling audit

The new measurements support the existing recommendation: ordinary C++ ray batching with persistent refill is the best measured combination of useful throughput and code simplicity. It reduces instructions and branch mispredictions per completed path substantially. The data do not support optimizing for the highest IPC, fewest arithmetic operations, fewest divisions, or a claimed DRAM bandwidth limit in isolation.

This audit uses `../perf/hardware-stat.json`, `../perf/pmu-specific.json`, `../perf/samples-summary.json`, `../perf/followup-core.json`, `../perf/controller-traffic.json`, and the corresponding raw CSV and self/inclusive sampling reports. It complements `perf-interpretation.md`, which records event definitions and primary references. Hardware access was obtained without changing machine policy or CPU settings.

## Measurement validity and boundaries

I checked all 348 event records across 39 basic-stat runs against their raw CSV values: they agree. All report 99–100% running; there are no unsupported or unscheduled entries in these results. The basic study has three repetitions per configuration. The PMU-specific cache/divider pass has one run per configuration, so small differences in those counts should not be ranked as repeatable effects.

Basic and PMU-specific stat commands select the P or E PMU explicitly and launch `taskset` before `runuser`, pinning the renderer's entire startup for homogeneous runs. The short `taskset` launcher itself precedes that affinity change. Hardware events use user-space scope; task-clock and scheduler events cover the monitored process tree more broadly. Counts include launch, initialization, rendering, and HDR output, rather than only the timed rendering region.

Sampling commands select the appropriate PMU but do not launch through `taskset`. Their renderer sets worker affinity after startup. Thus early work can execute on a different PMU and be absent from that profile. The pinned, multi-second rendering phase dominates the reports, but they are not complete startup profiles. Sample counts are 6,599 for scalar P1, 1,936 for refill P1, and 5,543 for refill E1, with zero reported lost samples. Percentages are approximate cycle-sample attribution, not exact elapsed-time partitions or latency measurements.

The saved production and debug `.text` sections are byte-identical: both SHA-256 `db9fdbc0747070f72764ece395b9ac54fe609be7619f0afaf3e009a6344cb707`. Debug information therefore did not alter the executable instructions being profiled.

All runs complete 13,107,200 paths. They use the same per-path random streams and estimator, but permitted floating-point differences slightly change paths: scalar traces 88,965,776 ray segments; flat 88,965,791; fixed/refill rays 88,965,817; affine 88,965,790; culled 88,965,839; explicit ray8 88,965,840. The maximum difference is 64 segments, about 0.000072% of the workload. Fixed versus refill rays is an especially clean comparison because its actual ray-segment count is identical.

Use the earlier unprofiled timing matrix for performance claims. These profiling runs confirm the broad ranking; their wall times include privileged observers and launch overhead. The 32-thread stat runs also record roughly 4,700–5,000 context switches, versus 62 for eight P workers, and show more timing variation. That is measured behavior during profiling, not evidence that the normal renderer requires those switches or that one threading runtime is intrinsically slower.

## What improves useful throughput

The table reports medians of the three basic-stat runs. Ratios are calculated from the corresponding median counts; `B` means billion, `M` million. Hardware columns include the process scope described above, while the time column is the renderer's internal diagnostic timer.

| One-core configuration | Render s | Instructions B | Cycles B | Branch mispredictions M | IPC |
|---|---:|---:|---:|---:|---:|
| Scalar, P | 13.176 | 143.959 | 78.400 | 808.471 | 1.836 |
| Flat triangle SoA, P | 6.184 | 80.792 | 36.755 | 245.432 | 2.198 |
| Fixed groups of eight rays, P | 7.364 | 121.691 | 43.832 | 199.036 | 2.776 |
| Ordinary C++ rays with refill, P | 3.879 | 58.722 | 23.037 | 103.531 | 2.549 |
| Affine intersection, P | 5.157 | 53.534 | 30.717 | 196.089 | 1.743 |
| Culled affine intersection, P | 5.284 | 54.586 | 31.370 | 229.382 | 1.740 |
| Explicit AVX2 rays with refill, P | 4.169 | 63.434 | 24.801 | 112.426 | 2.558 |
| Ordinary C++ rays with refill, E | 11.124 | 58.717 | 49.755 | 112.639 | 1.180 |

Relative to scalar, ordinary C++ refill retires **59.2% fewer instructions**, uses **70.6% fewer cycles**, and records **87.2% fewer branch mispredictions**. Its diagnostic render time is 3.40× shorter. This is strong evidence that reorganizing useful work and avoiding branch-heavy scalar execution matters much more than minimizing the scene's already-small unique footprint.

Relative to fixed ray groups, refill retires 51.7% fewer instructions and uses 47.4% fewer cycles while completing the same paths and ray segments. The software ratio of live ray segments to processed slots rises from 42.26% to 99.94%. That ratio describes the batch scheduler, not measured hardware SIMD utilization. Refilling has a small bookkeeping cost, but avoiding processing mostly inactive groups outweighs it decisively.

IPC alone would select the wrong implementation: fixed groups have the highest IPC here yet take almost 1.90× as long as refill. The affine and culled kernels retire fewer instructions than refill but require more cycles. The explicit AVX2 refill kernel has almost the same IPC as ordinary C++ refill, but retires approximately 8.0% more instructions and takes more cycles. Those comparisons support retaining the compiler-vectorized C++ kernel as the default; neither source-level arithmetic counts nor intrinsics establish performance by themselves.

Branch miss *rate* also needs a denominator. Flat has a lower miss rate than refill, 2.65% versus 4.14%, but incurs 18.72 versus 7.90 mispredictions per completed path. Scalar incurs 61.68. The useful comparison is total work and mispredictions per path alongside time, not the smallest percentage of a changing branch count. These counters do not measure a fixed penalty per miss, so do not multiply them by an assumed penalty to attribute a percentage of runtime.

## The cache result is narrow but useful

The specific pass counts retired P-core load instructions and E-core load micro-operations. L1 miss ratios below use their matching denominators, not total cache accesses.

| Configuration | Retired loads B | Retired L1 misses | L1 misses per million counted loads | Retired L3 misses | FP-divider active / cycles |
|---|---:|---:|---:|---:|---:|
| Scalar P | 38.589 | 85,381 | 2.21 | 1,028 | 13.86% |
| Flat P | 22.592 | 77,892 | 3.45 | 1,684 | 7.70% |
| C++ refill P | 19.547 | 104,412 | 5.34 | 1,275 | 12.20% |
| Culled P | 16.415 | 85,571 | 5.21 | 1,773 | 9.07% |
| AVX2 refill P | 19.903 | 109,103 | 5.48 | 1,412 | 11.35% |
| C++ refill E | 25.929 | 593,042 | 22.87 | Not collected | 18.90% |

The named retired-load events record very few misses. That supports heavy cache reuse for these loads and weakens a demand-load cache-capacity explanation for the large differences between kernels. It does **not** show that all data stays in L1, all loads are free, or DRAM traffic is negligible. The counters omit stores, write allocation, much prefetch traffic, and instruction-fetch traffic; they are not byte counters. The P event counts instructions and includes software-prefetch instructions in its all-load denominator; the E event counts micro-operations. Their absolute load totals and miss ratios are not directly interchangeable. The [Intel P definitions](https://raw.githubusercontent.com/intel/perfmon/main/ADL/events/alderlake_goldencove_core.json) and [E definitions](https://raw.githubusercontent.com/intel/perfmon/main/ADL/events/alderlake_gracemont_core.json) specify these differences.

Our approximately 192 KiB refill sample buffer per worker exceeds L1 capacity. Sequential access and advance fetching can allow demand loads to hit after traffic has already occurred; store traffic can occur without a counted retired-load miss. Also, even L1 hits consume load/store execution resources, and register spills can be costly while producing few misses. Therefore the safe conclusion is “very low observed retired demand-load miss counts,” not “zero memory bottleneck,” “the whole renderer is L1 resident,” or a computed FLOPs/DRAM-byte figure.

The FP-divider ratios are activity measurements, not stalled-cycle fractions. In particular, 12.2% on P and 18.9% on E do not predict the speedup from replacing exact divisions. They also use different PMU event implementations. For P flat and C++ refill, absolute activity counts are nearly the same, 2.832B and 2.807B, while total cycles fall substantially. The ratio rises because the rest of the workload becomes faster. It would be incorrect to call that an increased divider cost caused by refill. Our earlier branchless explicit-kernel experiment likewise improved time while evaluating more vector divisions.

The memory-controller follow-up adds direct traffic evidence. Summing read and write counters across both controllers gives the following system-wide measurements. Rates use the event runtime, rather than the larger observer subprocess wall time; all controller events report 100% running.

| Window | Controller traffic MiB | Counter duration s | MiB/s |
|---|---:|---:|---:|
| Idle before | 194.94 | 3.00283 | 64.92 |
| Ten 32-thread renders, 1 | 500.69 | 2.89819 | 172.76 |
| Ten 32-thread renders, 2 | 449.94 | 2.87374 | 156.57 |
| Ten 32-thread renders, 3 | 436.60 | 2.89587 | 150.77 |
| Idle after | 173.82 | 3.00279 | 57.89 |

Subtracting the observed idle-rate range gives an estimated excess of approximately 86–115 MiB/s during loaded windows. This is not application-exclusive traffic: background activity can change, and the loaded windows include launchers and file output. It is also not a cache-traffic measurement; L1/L2/L3 transfers can greatly exceed DRAM transfers. Nevertheless, the directly observed controller traffic is modest even with all 32 logical CPUs rendering. Combined with the retired-load results, it makes DRAM bandwidth a low-priority explanation for this scene. A measured sustainable-bandwidth benchmark was not collected, so do not report a percentage of DRAM capacity used.

## Sampling localizes the next work

Worker self attribution is 83.20% for scalar P, 61.78% for refill P, and 75.32% for refill E. The worker contains inlined intersection, packing, path replacement, and bookkeeping, so these numbers cannot all be labeled “intersection.”

DWARF inclusive attribution provides a narrower view: approximately 79.39% of scalar P samples fall under inlined `ScalarScene::intersect`; refill `RayScene::intersect_batch` accounts for 47.10% on P and 68.46% on E. These are useful approximate locations, with optimized-source attribution and sampling-skid limits. They do not identify a particular instruction as the cause of a stall.

For refill P, `scatter` is 35.69% inclusive and `sample_hemisphere` is 23.31% inclusive; the latter is nested inside the former. Refill E has 23.30% and 15.80%, respectively. Do not add those overlapping percentages. The separate self shares for the sine and cosine implementations total 10.57% on P and 7.65% on E, excluding wrappers. Sampling therefore shows that the sampler is now worth considering, especially on P cores, but it is not the majority of the render. As intersection gets faster, unchanged shading can naturally become a larger percentage.

Existing recordings can be annotated against the byte-identical debug executable to distinguish the ray loop, state packing, refill, and scalar scatter work without changing code. Source-line attribution should be checked against the disassembly rather than interpreted as a precise per-expression cost. A speculative counter also need not map one-to-one to the instruction sampled nearby.

## Topdown strengthens the control-flow explanation

The dedicated P-core pass uses the installed `TopdownL1` metric formulas, including their frontend correction. These are fractions of issue slots, not fractions of elapsed seconds.

| P-core slot category | Scalar | C++ refill |
|---|---:|---:|
| Bad speculation | 42.8% | 22.0% |
| Frontend bound | 9.8% | 4.1% |
| Backend bound | 16.3% | 27.3% |
| Retiring | 31.1% | 46.5% |

This corroborates the large reduction in branch mispredictions: the scalar renderer spends far more issue opportunities on unsuccessful speculation. Refill devotes a larger fraction to work that retires, while the remaining execution becomes relatively more backend constrained. The backend *fraction* increasing does not mean a performance regression or prove a new memory bottleneck. Total slots fall from approximately 470.99B to 138.45B; a larger percentage of a much smaller total can still mean fewer affected slots. “Retiring” also does not certify that an operation was algorithmically necessary. [perf topdown interpretation](https://perfwiki.github.io/main/top-down-analysis/)

For the E-core raw-event pass, normalizing each category by `5 × cycles` gives approximately 3.38% frontend, 10.37% bad speculation, 45.73% backend, and 40.47% retiring. The sum is 99.951%, a useful consistency check. Report these as approximate E-specific slot ratios, not the P metric-register method. They suggest that backend execution deserves closer examination on E cores, consistent with the larger intersection sample share. This level does not distinguish execution-port pressure, dependencies, divider activity, or load/store throughput. P and E topdown definitions also differ, so their percentages are not a ranking of which core wastes more seconds.

## Core scaling evidence

With refill, P1 and P8 retire essentially the same 58.72B instructions. Their cycle totals are 23.037B and 23.107B, and IPC is 2.549 and 2.541. Thus the eight-P result does not show a large increase in executed work or cycles per path. User cycles divided by task-clock are approximately 5.958 and 5.640 cycles/ns, a 5.3% reduction. This is consistent with the roughly 5% departure from linear P-core scaling in the unprofiled study.

That quotient is a proxy, not a precise frequency measurement: task-clock includes kernel and launcher activity whereas the cycle numerator excludes kernel execution, and the hardware counts are scaled. The focused follow-up supplies a cleaner comparison using active cycles divided by same-scope reference cycles:

| Configuration | P active/reference | Relative to isolated P | E active/reference | Relative to isolated E |
|---|---:|---:|---:|---:|
| One core of its type | 1.86615 | Baseline | 1.40852 | Baseline |
| Eight P / sixteen E, measured separately | 1.78354 | −4.43% | 1.39880 | −0.69% |
| 24 physical cores together | 1.61142 | −13.65% | 1.29953 | −7.74% |
| 32 logical CPUs together | 1.54262 | −17.34% | 1.23737 | −12.15% |

These same-PMU ratios support lower average active clock rates under larger simultaneous workloads. They explain a substantial reason the isolated-core, weighted linear model is optimistic: the cores do not keep their isolated operating rates when all are active. No reference-clock frequency needs to be assumed to compare these ratios. These are one focused observation per configuration, not a frequency distribution over the original benchmark runs. They do not establish whether the lower rate was caused by a particular turbo policy, package-power limit, thermal limit, or another operating constraint.

E1 and E16 similarly retain approximately 58.72B instructions and 49.76B/49.61B cycles. Their cycle/task-clock proxies are approximately 4.480/4.462 cycles/ns. This agrees with the much closer homogeneous E-core scaling. In mixed runs, P and E instruction counts sum to approximately 58.75B, close to the homogeneous result; there is no indication of a large hybrid scaling inflation in these saved counts.

P IPC is approximately 2.54 in the 24-physical-core run and 1.68 in the 32-logical-CPU runs; E IPC remains approximately 1.18. Summed sibling cycles count both logical processors while they share a physical P core, so these P ratios are logical-cycle-weighted IPC, not physical-core IPC. The decline does not negate the independently measured throughput benefit from SMT. The unprofiled std/OpenMP results remain effectively tied; the small profiled differences do not establish a new runtime winner.

## Minimal-complexity implications and follow-up

Keep the ordinary C++ ray kernel, eight persistent paths, dynamic tile queue, and worker-owned state as the default. Keep the flat kernel as the simpler baseline. Retain advanced kernels as experiments rather than adding their complexity to the winning path without a new measurable benefit.

The next low-complexity arithmetic experiment is sharing sine/cosine evaluation, if supported by the chosen compiler/library, since the profile shows both functions being called. It should preserve the hemisphere distribution and per-path stream; validate numerical changes and throughput rather than assuming identical output or a speedup. A vectorized sampler is a larger follow-up and is not justified merely by the worker's entire self share.

The completed topdown, reference-cycle, and memory-controller passes now address the main interpretation gaps. The next focused diagnostic, if another optimization pass is wanted, is annotation of the existing recording followed by a small backend-specific event set for the actual hot instructions. If state transport remains a target, inspect retired stores, loads, and generated spills before redesigning its layout; if dependency or port pressure appears responsible, measure that specific hypothesis. The earlier measured AoS refill already beat the more elaborate SoA state arrangement. No additional counter collection is necessary to choose the current implementation.
