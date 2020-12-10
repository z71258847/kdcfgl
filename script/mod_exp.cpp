#include <cstdio>
#include <cinttypes>

template <class T> T mod_exp(size_t len, T base, __attribute__((annotate(""))) T exp, T mod) {
    T result = 1;
    base = base % mod;
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
    uint32_t base = 0x9E6BF27A;
    uint32_t mod = 524287;
    uint32_t exp = 0xDEADBEEF;
    size_t len = 32;
    uint32_t result = mod_exp<uint32_t>(len, base, exp, mod);
    printf("%u\n", result);
    return 0;
}