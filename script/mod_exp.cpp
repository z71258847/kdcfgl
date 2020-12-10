#include <cstdio>
#include <cinttypes>
#include <x86intrin.h>

template<typename T> T mod_exp(T base, __attribute__((annotate(""))) T exp, const T mod) {
    const size_t len = sizeof(T) << 3;
    T result = 1;
    base %= mod;
    for (size_t i = 0; i < len; i++) {
        if (exp & 1) {
            result = (result * base) % mod;
        }
        exp >>= 1;
        base = (base * base) % mod;;
    }
    return result;
}

int main() {
    uint32_t base = 0xDEADBEEF;
    uint32_t mod = (1 << 16) - 123;
    uint32_t exp = 0x0;
    uint64_t tstart = __rdtsc();
    uint32_t result = mod_exp<uint32_t>(base, exp, mod);
    uint64_t tend = __rdtsc();
    fprintf(stderr, "all zeros, time: %lu\n", tend - tstart);
    fprintf(stdout, "%u\n", result);
    exp = 0xFFFFFFFF;
    tstart = __rdtsc();
    result = mod_exp<uint32_t>(base, exp, mod);
    tend = __rdtsc();
    fprintf(stderr, "all ones, time: %lu\n", tend - tstart);
    fprintf(stdout, "%u\n", result);
    return 0;
}