# Round eight records

See `../../ROUND8.md`. These are measurements on blacknoir's i9-14900K,
GCC 15.2 and Clang 20.1.8. Binary/source hashes and hardware details are in
`provenance.json`, `builds.json` and `delivery.json`.

Reproduce from the repository root, running heavy commands sequentially:

```sh
make plain clang build/pathtracer_plain_strict
python3 benchmarks/round8/build.py
python3 benchmarks/round8/validate.py
python3 benchmarks/round8/screen.py
python3 benchmarks/round8/full_bench.py
python3 benchmarks/round8/validate_more.py
python3 benchmarks/round8/summarize.py
python3 benchmarks/round8/audit.py
python3 benchmarks/round8/audit_gcc.py
sudo -v
python3 benchmarks/round8/perf_compare.py
python3 benchmarks/round8/sanitize.py
```

The scripts write scratch outputs under `build/pass8/`. Perf needs permission
for CPU PMU events; authentication belongs in the terminal, never these scripts.
Its child renderer runs as blacknoir on CPU 8. The sanitizer script preserves
the observed Clang/libomp baseline leak diagnostic and expects that current
runtime behavior; a future runtime that fixes it needs an updated assertion.
GCC's unsuppressed storage checks must pass normally.

The AoS wrapper control and split geometry are in the smaller screen. The full
comparison contains originals, fields and scalar SoA for both compilers, two
sampling modes and two CPU scopes. All warmups and scored outliers are retained.
Perf has two separate observations per Clang configuration/mode. Load/store
counters count retired instructions, not bytes transferred to DRAM.

`loop-audit.json` / `gcc-loop-audit.json` and decoded assembly snippets record
actual loop instructions rather than disassembler byte-continuation lines.
Full executable disassemblies, rendered pixels and binaries remain ignored in
`build/`; the archived text records contain hashes and image-error summaries.
