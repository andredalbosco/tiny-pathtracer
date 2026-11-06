# Round-seven evidence

The parent commit is 4273290. Renderer source code is unchanged in this pass;
the production change adds separate Clang build targets. Full timings use
actual production binaries, with one warmup and five scored observations per
configuration. `summary.json` preserves observed ranges, including outliers.
`validation.json` contains 44 strict cross-compiler and 80 scheduling checks.
`fast-validation.json` separately records differences with FP contraction.
`perf/` contains 16 observations and raw/scaled hardware counts with running
fractions. No counter wall time is used as the primary throughput score.

`trigger-audit.json` and the compiler-pass excerpts isolate the library size
guard, loop-header copying and factor-two jam. `clang-loop-audit.json` counts
actual instructions, excluding objdump's byte-continuation lines.
`runtime-identity.json` verifies the GCC/libgomp-versus-libomp normalized code
identity with the same decoding rule. Complete raw dumps and binaries are in
ignored `build/pass7/`.

To rebuild diagnostic controls from the renderer sources at 4273290 and the
current separate Clang targets:

```sh
make plain quality clang build/pathtracer_quality_strict build/pathtracer_plain_clang_strict build/pathtracer_quality_clang_strict
mkdir -p build/pass7
python3 benchmarks/round7/build_compare.py
g++ -isystem . -O3 -std=c++17 -march=native -ffp-contract=fast -pthread -fopenmp \
    -fdump-tree-ch-details -fdump-tree-unrolljam-details \
    experiments/pathtracer_quality.cpp -o build/pass7/gcc-stl-ch
python3 benchmarks/round7/build_trigger.py
python3 benchmarks/round7/build_more.py
python3 benchmarks/round7/screen.py
python3 benchmarks/round7/probe_screen.py
python3 benchmarks/round7/validate.py
```

The all32 runtime control is compiled with GNU OpenMP and linked against
LLVM libomp's GNU ABI compatibility entry points. Run compilation before wall
scoring, and keep benchmarks sequential:

```sh
g++ -isystem . -O3 -std=c++17 -march=native -ffp-contract=fast \
    -fno-loop-unroll-and-jam -fopenmp -c experiments/pathtracer_plain.cpp \
    -o build/pass7/gcc-plain-libomp.o
g++ build/pass7/gcc-plain-libomp.o -L/usr/lib/llvm-20/lib \
    -Wl,-rpath,/usr/lib/llvm-20/lib -lomp -o build/pass7/gcc-plain-libomp
python3 benchmarks/round7/full_bench.py
python3 benchmarks/round7/fast_compare.py
sudo -v
python3 benchmarks/round7/perf_compare.py
```

Perf follows the unprofiled benchmark and requires the user's authorized sudo
access. The renderer itself runs as blacknoir. Probe source variations are
generated under build/pass7 from the unchanged source; they are not promoted
to production. The checked-count diagnostic assumes the renderer's internally
constructed scene has a nonnegative count, as documented in ROUND7.md.
