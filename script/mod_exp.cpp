#include <cstdio>
#include <cinttypes>

template <class T> T mod_exp(T base, T exp, T mod) {
    T result = 1;
    base = base % mod;
    while (exp > 0) {
        if (exp & 1) {
            result = (result * base) % mod;
        }
        exp = exp >> 1;
        base = (base * base) % mod;
    }
    return result;
}

int main() {
    uint32_t base = 0x9E6BF27A;
    uint32_t mod = 524287;
    __attribute__((annotate(""))) uint32_t exp = 0xDEADBEEF;
    uint32_t result = mod_exp<uint32_t>(base, exp, mod);
    printf("%u\n", result);
    return 0;
}