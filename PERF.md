# Hardware perf: what the CPU is doing

The simple C++ eight-ray refill loop remains the best one-core implementation in this pass. Hardware counters support its improvement through fewer retired instructions, fewer mispredicted branches, and better use of independent work. The remaining problem is chiefly inside the CPU and cache hierarchy; the observed DRAM traffic is small. No renderer source changed during this profiling pass.

These measurements were collected on 2026-10-13 on blacknoir's i9-14900K, GCC 15.2, perf 6.17.13. Root permission was used for the perf observer; the renderer ran as ordinary user `blacknoir`. The system's perf policy, governor, frequency settings, and watchdog were retained. Previous reports of denied hardware access describe the earlier unprivileged pass; this pass supersedes that limitation.

## Protocol and scope

The workload remains 320×320, 128 spp, at most 24 surface visits: 13,107,200 completed paths per render. All kernels use the same per-pixel/sample PCG stream identities and estimator. Fast builds permit FMA contraction; the kernels produce 88,965,776–88,965,840 live ray segments because some floating-point boundary decisions differ. Fixed/refilled ordinary ray kernels execute the same 88,965,817 live segments. Strict layout/scheduling equivalence checks are documented in [COMPUTATION.md](COMPUTATION.md); do not describe the fast kernels as bit-identical.

Basic counts use one warmup and three interleaved observations of 13 configurations. Explicit `cpu_core` and `cpu_atom` events are separated. Homogeneous stat launchers are pinned before program startup; renderer workers are individually pinned. Counters cover launcher activity, initialization, rendering, HDR output, and teardown. Events ran for 99–100% of their enabled intervals. Separate small passes measured load/divider events, topdown, and active/reference cycles. Those supplemental passes are single observations, not confidence intervals.

Scored throughput remains the unprofiled whole-executable timings in [COMPUTATION.md](COMPUTATION.md) and [SCALING.md](SCALING.md). The privileged observer/launcher and sampling add overhead, especially to short multicore runs; profiled wall times are diagnostics, not replacement scores. Median event counts are computed independently before ratios in the table below.

The debug sampling executable adds `-g` with otherwise identical production flags. Its extracted `.text` is byte-identical to production (SHA256 `db9fdbc0747070f72764ece395b9ac54fe609be7619f0afaf3e009a6344cb707`). Cycle sampling used 499 Hz and DWARF call stacks, with no lost samples reported. Unlike stat, record did not pin its launcher before startup; workers still pin before tracing. Small startup/launcher contributions remain in scope.

## Useful work beats instruction count or IPC alone

| One-core implementation | Cycles/path | Instructions/path | IPC | Branch mispredictions/path | Mispredicted branches |
| --- | ---: | ---: | ---: | ---: | ---: |
| Scalar, P | 5,981 | 10,983 | 1.84 | 61.68 | 6.99% |
| Flat triangle SoA, P | 2,804 | 6,164 | 2.20 | 18.72 | 2.65% |
| Fixed eight-ray groups, P | 3,344 | 9,284 | 2.78 | 15.19 | 4.28% |
| **Refilled eight-ray groups, P** | **1,758** | **4,480** | **2.55** | **7.90** | **4.14%** |
| Affine triangle kernel, P | 2,344 | 4,084 | 1.74 | 14.96 | 4.46% |
| Culled triangle kernel, P | 2,393 | 4,165 | 1.74 | 17.50 | 5.45% |
| Explicit AVX2 refill, P | 1,892 | 4,840 | 2.56 | 8.58 | 4.54% |
| Ordinary C++ refill, E | 3,796 | 4,480 | 1.18 | 8.59 | 4.51% |

Scalar → ordinary refill cuts total retired instructions from 143.96 to 58.72 billion, user cycles from 78.40 to 23.04 billion, and retired branch mispredictions from 808.47 to 103.53 million. This is about 3.4× fewer cycles per completed path. Several changes contribute—vectorization, triangle reuse, refill, and regular control flow—so these counts do not assign the improvement to a single cause.

Fixed groups have higher IPC than refill but do roughly twice the instruction work per completed path. Their live-slot occupancy is 42.26%, versus 99.94% with refill. Occupancy here counts software ray slots, not physical execution-unit utilization. The affine and culled implementations retire fewer instructions than refill but take more cycles. The goal is completed paths per second, not the highest IPC or fewest instructions.

## Where the winner spends cycles

DWARF inclusive source attribution identifies the following approximate shares of sampled user cycles. Parent/child shares overlap and must not be added.

| Source region | One P refill | One E refill |
| --- | ---: | ---: |
| Inlined `RayScene::intersect_batch` | 47.10% | 68.46% |
| `scatter`, including its callees | 35.69% | 23.30% |
| `sample_hemisphere`, including its callees | 23.31% | 15.80% |
| libm cosine, self | 7.05% | 5.08% |
| libm sine, self | 3.52% | 2.57% |

The enclosing worker's self shares are 61.78% P and 75.32% E; these include intersection, packing, refill, and bookkeeping. They are not intersection-only shares. Scalar's inlined intersection accounts for about 79.39% of its P samples. Sampling is approximate and instructions can receive samples after a latency-causing neighbor. The source-attributed shares select promising experiments; they are not exact stage timers.

Annotated assembly confirms packed FMA/division in the C++ ray loop and several stack-based YMM reloads. This supports inspecting register pressure and the packing boundary. It does not prove each sampled reload stalls, or that replacing AoS with SoA everywhere will help.

## Speculation, division, and loads

P-core installed `TopdownL1` metrics report:

| Issue-slot category | Scalar P | Refill P |
| --- | ---: | ---: |
| Retiring | 31.1% | 46.5% |
| Bad speculation | 42.8% | 22.0% |
| Frontend bound | 9.8% | 4.1% |
| Backend bound | 16.3% | 27.3% |

These are issue-slot fractions, not seconds or directly removable runtime. The backend fraction grows while total cycles fall strongly; this is not a regression. A separate E-core raw diagnostic normalized by five slots/cycle gives roughly 40.5% retiring, 10.4% bad speculation, 3.4% frontend bound, and 45.7% backend bound; the sum is 99.95%. E formulas and event mechanisms differ from P. Backend bound alone does not distinguish arithmetic throughput, dependency latency, load-port pressure, or cache latency. [Intel P/E event definitions](https://github.com/intel/perfmon/tree/main/ADL/events), [perf topdown guidance](https://perfwiki.github.io/main/top-down-analysis/).

Whole-render FP-divider activity is about 12.2% of P user cycles for refill, versus 18.9% on E; scalar P is 13.9%. Activity includes division/square-root work outside intersection and can overlap other instructions. It is not a stalled-time fraction or the speedup available from reciprocal approximation.

P refill records 19.55 billion retired loads and about 104 thousand retired L1-missing loads (0.00053%), plus 1,275 retired L3-missing loads. E refill records 25.93 billion load micro-operations and about 593 thousand L1 misses (0.00229%). P counts load instructions; E counts micro-operations, so their ratios have different denominators. These are demand-load events, not total transferred bytes: prefetching, stores, speculation, and cache-line requests have different populations. Low demand misses support effective reuse but do not rule out L1 traffic or register-spill costs. Exact encodings and restrictions are audited in [the PMU interpretation](benchmarks/reviews/perf-interpretation.md).

## DRAM is a different boundary from L1 traffic

System-wide memory-controller free-running counters were collected across both controllers. Three loaded windows each contained ten full 32-thread refill renders; idle windows bracketed them. Perf already converts counts to MiB (64 bytes/raw count). Rates use the mean event measurement interval, rather than Python launcher wall time.

| Window | Read + write traffic | Duration | Rate |
| --- | ---: | ---: | ---: |
| Idle before | 194.94 MiB | 3.003 s | 64.92 MiB/s |
| Ten renders, first | 500.69 MiB | 2.898 s | 172.76 MiB/s |
| Ten renders, second | 449.94 MiB | 2.874 s | 156.57 MiB/s |
| Ten renders, third | 436.60 MiB | 2.896 s | 150.77 MiB/s |
| Idle after | 173.82 MiB | 3.003 s | 57.89 MiB/s |

Gross traffic is roughly 3.5–4.0 controller bytes per completed path in the loaded windows; this includes unrelated processes, initialization, and output. Baseline subtraction suggests roughly 86–115 MiB/s of additional traffic, but does not make the counters application-exclusive. No sustainable-bandwidth benchmark was collected, so these are not a measured percentage of available bandwidth. They provide no evidence of bulk DRAM saturation and make CPU execution/cache traffic a stronger initial target.

Geometry's 1,296 useful coefficient bytes can be reused many times inside caches. Reading a coefficient or spill from L1 is still an executed load, but need not move another byte from DRAM. The ordered tile-result buffer is about 192 KiB per full tile at this spp: larger than L1, inside L2 capacity. Computation per byte must name its boundary; FLOPs/DRAM-byte and instructions/L1-load are different quantities. This pass measures completed paths and controller bytes, not FLOPs.

## Frequency explains part of scaling

| Configuration | P active/reference cycles | E active/reference cycles |
| --- | ---: | ---: |
| One P / one E, separate runs | 1.866 | 1.409 |
| Eight P / sixteen E, separate runs | 1.784 | 1.399 |
| 24 physical cores | 1.611 | 1.300 |
| 32 logical CPUs | 1.543 | 1.237 |

P1 → P8 retired instructions and cycles per render remain nearly constant; active/reference ratio falls about 4.4%. This is consistent with much of the 5.6% gap from ideal P scaling. E1 → E16 ratio falls only about 0.7%, consistent with its near-linear result. Under mixed package load both core families show a larger decline. That means an ideal formed by adding isolated-core rates assumes frequencies the loaded package does not retain. These are one-pass ratios; they do not establish a thermal or power-limit cause, exact GHz, or a complete quantitative decomposition of the mixed/SMT gap. P SMT siblings also share execution resources and accumulate separate logical-CPU cycles.

The unprofiled result remains 7.552× for eight P cores, 15.855× for sixteen E cores, and 45.91 million completed paths/s for all 32 logical CPUs. OpenMP and std::thread remain tied within observed variation. Hardware scheduling, arithmetic, and layout deserve attention before replacing the threading framework.

## Next controlled experiments

1. Give intersection direct origin/direction columns while keeping simple AoS path bookkeeping. Measure fewer copies/reloads and whole-render throughput; the earlier global-SoA adapter result does not answer this narrower question.
2. Reuse one ordered result buffer per worker and evaluate its initialization/tile size separately. Preserve indexed completion and sample-sum order.
3. Try profile-guided optimization of the unchanged computation. Then, if useful, batch independent sampler algebra while retaining the current PCG and scalar sine/cosine results.

More aggressive options—two independent eight-ray blocks, a vectorized sampler, and factorial early/late-division × rejection-flow experiments—are described with numerical limits in [TALKS.md](TALKS.md). None is a claimed speedup yet.

## Reproduction and retained evidence

From the repository root, on this CPU topology:

```sh
make simple advanced
c++ -isystem . -O3 -g -std=c++17 -Wall -Wextra -Wpedantic \
  -march=native -ffp-contract=fast -pthread -fopenmp \
  pathtracer_simple.cpp -o build/pathtracer_simple_profile
sudo -v
python3 benchmarks/profile_perf.py
python3 benchmarks/profile_followup.py
```

The scripts leave raw CSV and profiler recordings under ignored `build/perf/`. They never change machine settings. `profile_followup.py` reads the configuration metadata produced by the first script. Annotations can be generated with `perf annotate -i build/perf/refill-P1.data --stdio --source`; machine metadata and the byte comparison above were captured separately for this run. Original `.data` recordings and full annotation text are retained locally/under `build/`, rather than committed. Committed evidence is [hardware-stat.json](benchmarks/perf/hardware-stat.json), [pmu-specific.json](benchmarks/perf/pmu-specific.json), [followup-core.json](benchmarks/perf/followup-core.json), [controller-traffic.json](benchmarks/perf/controller-traffic.json), [machine.json](benchmarks/perf/machine.json), sampling reports, and the source/binary hashes. [Astra's audit](benchmarks/reviews/perf-results-audit.md) records interpretation limits.
