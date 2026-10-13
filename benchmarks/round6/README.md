# Round-six evidence

`full-results.json` records actual-production wall measurements against frozen
STL and initial-plain binaries, including commands, image/work records and
hashes. Repeat 0 is warmup; repeats 1–5 are scored. `summary.json` includes full
observed ranges. `validation.json` records 62 exact strict pixel/work checks and
full fast comparisons. `perf/` contains two interleaved counter observations
per version/mode, with running fractions and raw CSV exports.

`plain-before.cpp` is the initial plain source from 7659a28. The variant scripts
rebuild diagnostic controls from that snapshot; `variants.json` contains the
original recorded commands and hashes. Normalized-disassembly identity checks
are distinct from byte-identical-text checks in the Astra audit. Build paths
can shift assertion strings and RIP-relative addresses, so a reproduction
need not have the original binary hash.

On blacknoir with GCC 15.2, prepare baselines before running the saved tools:

```sh
mkdir -p build/pass6
make quality plain build/pathtracer_plain_strict build/pathtracer_quality_strict
cp build/pathtracer_quality build/pass6/baseline-stl
g++ -iquote . -isystem . -O3 -std=c++17 -Wall -Wextra -Wpedantic \
    -march=native -ffp-contract=fast -fopenmp \
    benchmarks/round6/plain-before.cpp -o build/pass6/baseline-plain
python3 benchmarks/round6/build_variants.py
python3 benchmarks/round6/build_nojam.py
python3 benchmarks/round6/screen.py
python3 benchmarks/round6/screen_nojam.py
python3 benchmarks/round6/full_bench.py
python3 benchmarks/round6/validate_plain.py
sudo -v
python3 benchmarks/round6/perf_plain.py
```

Run timed commands sequentially without competing benchmarks. Perf requires
the user's normal authorized sudo access under this kernel's counter policy;
the renderer itself runs as blacknoir. All probe executables and raw images
are written to ignored `build/pass6/`. `PLAIN_FLAGS` is one target's GCC tuning
option, not a project-wide optimization policy.
