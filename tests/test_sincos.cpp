#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>

// Prevent the separate reference calls from becoming a combined sincos call.
__attribute__((noinline)) float separate_sin(float x) { return std::sin(x); }
__attribute__((noinline)) float separate_cos(float x) { return std::cos(x); }
std::uint32_t bits(float x) { std::uint32_t b; std::memcpy(&b, &x, sizeof(b)); return b; }

int main() {
    std::uint64_t sine_changes = 0, cosine_changes = 0;
    for (std::uint32_t i = 0; i < (1u << 24); ++i) {
        float u = float(i) * 0x1p-24f;
        float phi = 2 * M_PI * u;
        float sine, cosine;
        __builtin_sincosf(phi, &sine, &cosine);
        sine_changes += bits(sine) != bits(separate_sin(phi));
        cosine_changes += bits(cosine) != bits(separate_cos(phi));
    }
    std::cout << "{\"angles\":16777216,\"sine_changes\":" << sine_changes
              << ",\"cosine_changes\":" << cosine_changes << "}\n";
    return sine_changes || cosine_changes;
}
