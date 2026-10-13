# Intel i9-14900K PMU interpretation for tiny-pathtracer

This is an event-definition and measurement-scope audit, prepared on 2026-10-13 before the newly authorized hardware-counter results arrive. It makes no measured IPC, cache-miss, divider-utilization, or DRAM-bandwidth claim. Earlier reports described the permissions available at their measurement time; privileged collection is now available through the parent agent without changing the system policy.

## Identify the two PMUs before counting

Live metadata reports family 6, model 183 (`0xB7`), stepping 1, perf 6.17.13. `cpu_core` has type 4 and CPUs 0–15; `cpu_atom` has type 10 and CPUs 16–31. The former comprises eight P cores with two logical CPUs each; the latter comprises sixteen E cores without SMT. Both expose the kernel family label `alderlake_hybrid`. This is compatible with the processor: Intel explicitly maps `GenuineIntel-6-B7` to the ADL Golden Cove and Gracemont event definitions. Use those definitions, not Arrow Lake/Skymont or an older generic Intel event table. [Intel model map](https://raw.githubusercontent.com/intel/perfmon/main/mapfile.csv)

The captured installed event list is `perf-list-details.txt`. Official Intel JSON snapshots in `pmu-reference/` have version 1.41 and publication date 2026-08-21. The installed perf aliases and formulas are the immediate record of what this experiment actually requests; the official files explain their meaning and restrictions.

## Recommended small measurement passes

Use explicit `cpu_core/<event>/u` or `cpu_atom/<event>/u` selectors for application-only counts, retaining the same privilege scope in every numerator and denominator. The names below are case-insensitive perf event names. Keep separate passes small and retain event runtime, percentage running, command, affinity, binary hash, output checksum, and workload counts.

| Purpose | P-core event | E-core event | Interpretation |
|---|---|---|---|
| Retired instructions | `instructions` | `instructions` | Architectural instructions, not FLOPs or ray work |
| Active cycles | `cycles` | `cycles` | Unhalted cycles; frequency dependent |
| Reference cycles | `ref-cycles` | `ref-cycles` | Unhalted reference-clock count; useful alongside active cycles |
| Retired branches | `br_inst_retired.all_branches` | Same name | Include predictable branches as well as difficult ones |
| Mispredicted retired branches | `br_misp_retired.all_branches` | Same name | Divide by retired branches; not a lost-cycle measurement |
| Retired loads | `mem_inst_retired.all_loads` | `mem_uops_retired.all_loads` | P counts load instructions; E counts load micro-operations |
| Retired L1 misses | `mem_load_retired.l1_miss` | `mem_load_uops_retired.l1_miss` | Match each to its own load denominator |
| Retired L2 misses | `mem_load_retired.l2_miss` | `mem_load_uops_retired.l2_miss` | Load-side hierarchy evidence, not traffic bytes |
| Retired L3 misses | `mem_load_retired.l3_miss` | `mem_load_uops_retired_misc.l3_miss` | Event names and encodings differ |
| FP-divider activity | `arith.fpdiv_active` | Same name, different encoding | Activity, not stalled cycles |
| Any-divider activity | `arith.div_active` | Same name, different encoding | Broader than just intersection vector divisions |

The P load events above are restricted to programmable counters 0–3. The listed E load events permit counters 0–5. A large group can therefore fail even when the processor has other counters available. PEBS support on an event is not a requirement to add precise sampling modifiers for ordinary `perf stat` counting. These distinctions come from Intel's [P-core definitions](https://raw.githubusercontent.com/intel/perfmon/main/ADL/events/alderlake_goldencove_core.json) and [E-core definitions](https://raw.githubusercontent.com/intel/perfmon/main/ADL/events/alderlake_gracemont_core.json).

Suggested order is: (1) instructions, cycles, reference cycles, branches and mispredictions; (2) loads and L1/L2 misses; (3) divider events with cycles; (4) topdown separately; (5) memory-controller traffic separately. Group directly compared events on the same PMU so they observe the same intervals. Do not put P and E events into one hardware group. Reduce the event set if it cannot schedule; no watchdog or policy change is needed for this audit.

## Hybrid scaling, affinity, and IPC

Generic unqualified events automatically create P and E events. A thread that changes core type can spend only a small fraction of its lifetime on one PMU. Perf can scale that small count across the enabled interval, producing a misleading extrapolation. Pin homogeneous runs before executable startup; setting affinity only inside renderer workers does not constrain preceding initialization. Preserve P and E output separately, avoid `--hybrid-merge`, and inspect all running percentages. This behavior and the prohibition on cross-PMU groups are documented in the [perf-stat manual's Intel hybrid section](https://man7.org/linux/man-pages/man1/perf-stat.1.html).

Perf normally estimates a multiplexed count as `raw_count × time_enabled / time_running`. This assumes observed intervals represent the unobserved intervals. The running percentage can also reflect hybrid PMU eligibility, so a low percentage alone does not prove too many counters were requested. Inspect raw counts with `--no-scale` when diagnosing this and retain the enabled/running times. [perf maintainer tutorial](https://perfwiki.github.io/main/tutorial/)

For one homogeneous core, same-scope `instructions / cycles` is a useful IPC measure. For a mixed-core run, report P IPC and E IPC independently before considering a combined ratio. `sum(instructions) / sum(cycles)` is a cycle-weighted logical-CPU ratio, not the arithmetic mean of IPCs and not a physical-core IPC. Both active SMT siblings accumulate cycles while sharing one physical P core; summing their cycles double-counts elapsed physical-core cycles. Higher logical-CPU aggregate IPC is therefore not required for SMT to improve completed paths/s. Retired instructions per completed path and cycles per completed path should accompany IPC, because removing instructions can lower IPC while improving useful throughput.

For mixed runs, check whether collection is process-attached with inherited worker events or system-wide on selected CPUs. They answer different questions. System-wide counts avoid off-PMU thread extrapolation when separated by CPU type, but include unrelated work on those CPUs. Small homogeneous runs with idle siblings provide the cleanest initial kernel diagnosis.

## Divider activity is not a stall measurement

The installed P FP event encodes `event=0xb0,umask=0x01,cmask=1`; its broader divide event is `0xb0/0x09/cmask=1`. E FP activity is `0xcd/0x02` without that counter mask; E general divider activity is `0xcd/0x03/cmask=1`. Copying a raw encoding from one PMU to the other would count the wrong thing. Prefer the named aliases.

Intel describes the P general event as active divide/square-root cycles, including integer and floating-point work. The E FP event observes the divider loop stage. These are speculative execution activity events, not retired division instructions, eight-lane operation counts, or cycles when the whole CPU makes no progress. Independent arithmetic can overlap them. Interpret activity divided by matching unhalted cycles as an activity ratio with the event's scope; do not call it the fraction of time removable by eliminating divisions. SMT resource sharing further complicates attributing shared-divider utilization to a single sibling.

This distinction already matters in our implementation: the branchless ray kernel performs more vector division instructions than the staged version, yet its matched render runs became faster. Removing dependent rejection branches can improve throughput despite increasing arithmetic. Whole-render divider counts also include square roots, normalization, sampling, and library work; attributing every divider cycle to triangle intersection requires instruction-level profiling or an isolated fixed-ray test. Intel's [P event definitions](https://raw.githubusercontent.com/intel/perfmon/main/ADL/events/alderlake_goldencove_core.json) and [E event definitions](https://raw.githubusercontent.com/intel/perfmon/main/ADL/events/alderlake_gracemont_core.json) provide the encoding and scope distinction.

## L1 misses, LLC requests, and DRAM bytes

The P L1 event counts a retired load instruction if at least one constituent load micro-operation missed L1. Its all-load denominator also includes software prefetch instructions. The E events count load micro-operations. Label their ratios accordingly, and do not merge them into a supposedly identical cross-core load-miss probability. A missed load may merge into an outstanding line request; one retired miss is not one transferred cache line. None of these retired-load events measures bytes.

The generic `cache-misses` alias is `event=0x2e,umask=0x41`, an LLC-request event. Its scope can include instruction fetch, write ownership, speculation, and prefetch traffic. `cache-references` is also an LLC event, not all L1 references. Multiplying `cache-misses` by 64 does not establish DRAM bandwidth or useful bytes moved. The [P definitions](https://raw.githubusercontent.com/intel/perfmon/main/ADL/events/alderlake_goldencove_core.json) and [E definitions](https://raw.githubusercontent.com/intel/perfmon/main/ADL/events/alderlake_gracemont_core.json) specify different request populations.

The live machine offers a better traffic measurement: `uncore_imc_free_running_0` and `_1`, each with `data_read`, `data_write`, and `data_total`. All three have sysfs scale `6.103515625e-5` and unit `MiB`: one raw count corresponds to 64 bytes. Their representative CPU mask is CPU 0; that CPU is a control location, not a claim that all memory traffic originates there. Sum matching controller events, respect perf's displayed units, and avoid adding `data_total` to read plus write. The kernel exposes these counters and conversion through its [Intel client uncore driver](https://raw.githubusercontent.com/torvalds/linux/v6.17/arch/x86/events/intel/uncore_snb.c).

Uncore counts cover the whole controller, including other processes. Record an idle baseline and the actual measurement duration. Observed controller MiB/s is useful evidence against a bandwidth bottleneck only when compared with a credible available-bandwidth measurement and temporal workload scope. A small geometry footprint is evidence of reuse, not proof of zero DRAM access. Our refill sample buffer is approximately 192 KiB per worker, larger than L1, even though the geometry and active ray state are small.

## Topdown constraints

On this installation `TopdownL1` and `TmaL1` show P-core formulas using the `topdown-*` aliases and `slots`. The frontend formula includes a dropped-uop correction; do not reconstruct the suite by treating every raw alias as an independent cycle percentage. Use the installed metric formula and retain its actual events. Topdown categories partition issue opportunities rather than seconds of runtime. A high retiring fraction may include redundant work; a high backend fraction does not by itself distinguish dependency latency, execution throughput, cache latency, or DRAM bandwidth. [perf topdown guidance](https://perfwiki.github.io/main/top-down-analysis/)

E cores expose `topdown-fe-bound` (`0x71/0`), `topdown-bad-spec` (`0x73/0`), `topdown-be-bound` (`0x74/0`), and `topdown-retiring` (`0xc2/0`). Their definitions count issue slots or retired uops. Installed E subcategory formulas normalize by five times `CPU_CLK_UNHALTED.CORE`; these are not the P metric-register mechanism. An E raw-event diagnostic can use the E-specific definitions and check the category totals, but should not be presented as the installed P-only `TopdownL1` metric applied unchanged. [Intel Gracemont definitions](https://raw.githubusercontent.com/intel/perfmon/main/ADL/events/alderlake_gracemont_core.json)

Start with one P core with its sibling idle and one E core, in separate passes. Then compare eight P cores, sixteen E cores, and SMT/mixed configurations if the measurements remain schedulable. Do not infer that 32 charged logical CPUs mean 32 independent physical execution engines. Preserve wall time and completed paths/s as the performance result; use counters to explain the result and to choose the next experiment.

## When results arrive

Audit the exact event names, scope, raw or scaled status, runtime percentages, repeated-run variability, and identical numerical/workload configuration before interpreting ratios. In particular, check:

1. Whether fewer instructions or branch misses per completed path explain the kernel winner, even when its IPC or divider activity moves in the opposite direction.
2. Whether increasing core count changes active/reference-cycle ratios; this can support a frequency explanation for the gap from the isolated-core weighted scaling estimate. It does not alone establish a package-power or thermal cause.
3. Whether retired L1 misses, LLC requests, and controller bytes jointly support the proposed cache explanation. Low miss counts do not exclude L1 load/store throughput or register-spill overhead.
4. Whether SMT improves total completed paths/s despite sharing physical resources; use per-type counters rather than a mixed-PMU IPC headline.

No hardware-counter result has yet been incorporated into this report.
