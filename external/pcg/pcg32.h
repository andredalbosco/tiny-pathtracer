/*
Copyright (c) 2014-2017 Melissa O'Neill and PCG Project contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <cstdint>

// CUDA-compatible subset of PCG XSH-RR 64/32 from pcg-cpp.
// Preserves the two-argument constructor and operator() sequences.
struct pcg32 {
#ifdef __CUDACC__
    __host__ __device__
#endif
    pcg32(std::uint64_t seed, std::uint64_t stream)
        : state(0), increment((stream << 1u) | 1u) {
        (*this)();
        state += seed;
        (*this)();
    }

#ifdef __CUDACC__
    __host__ __device__
#endif
    std::uint32_t operator()() {
        std::uint64_t previous = state;
        state = previous * 6364136223846793005ULL + increment;
        std::uint32_t bits = std::uint32_t(((previous >> 18u) ^ previous) >> 27u);
        std::uint32_t rotation = std::uint32_t(previous >> 59u);
        return (bits >> rotation) | (bits << ((0u - rotation) & 31u));
    }

private:
    std::uint64_t state;
    std::uint64_t increment;
};
