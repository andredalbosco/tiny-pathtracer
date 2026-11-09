Official PCG C++ headers, copied unchanged from
[imneme/pcg-cpp](https://github.com/imneme/pcg-cpp) at commit
`428802d1a5634f96bcd0705fab379ff0113bcf13`.

The headers are distributed under the MIT option of their upstream dual license.
See `LICENSE-MIT.txt`.

`pcg32.h` is our small CUDA-compatible subset of PCG XSH-RR 64/32. It exposes
only the two-argument seed/stream constructor and `operator()`, preserving the
official implementation's integer sequences. Its complete MIT license is
included directly in the file. The unchanged upstream headers are retained as
the reference for sequence validation.
