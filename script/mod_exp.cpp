#include <cstdio>
#include <cinttypes>
#include <x86intrin.h>
#include <random>

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

template<typename T> T test(__attribute__((annotate(""))) T key) {
    const size_t len = sizeof(T) << 3;
    T ans = 0;
	for (size_t i=0; i < len; i++){
		T c=(key>>i)&1;
		if (c){
			ans++;
		}
	}
    return ans;
}

template<typename T> T test2(__attribute__((annotate(""))) T key) {
    const size_t len = sizeof(T) << 3;
    T ans0=0, ans1=0;
	for (size_t i=0; i < len; i++){
		int c=(key>>i)&1;
		if (c){
			if (i%2==0){
				ans0++;
			}
			else{
				ans1++;
			}
		}
	}
	// printf("%d %d\n", ans0, ans1);
    return 0;
}

template<typename T> T test3(__attribute__((annotate(""))) T key) {
    const size_t len = sizeof(T) << 3;
    T ans0=0, ans1=0, ans2=0;
	
	for (size_t i=0; i<len; i++){
		int c=(key>>i)&1;
		if (c){
			if (i%2==0){
				ans0++;
			}
			else{
				ans1++;
			}
		}
		else{
			ans2++;
		}
	}
	// printf("%d %d %d\n", ans0, ans1, ans2);
    return 0;
}

template<typename T> T test4(__attribute__((annotate(""))) T key) {
    const size_t len = sizeof(T) << 3;
    T ans0=0, ans1=0, ans2=0, ans3=0;
	
	for (size_t i=0; i<len; i++){
		int c=(key>>i)&1;
		if (c){
			if (i%2==0){
				ans0++;
			}
			else{
				ans1++;
			}
		}
		else{
			if (i%2==1){
				ans2++;
			}
			else{
				ans3++;
			}
		}
	}
    return 0;
}

int main() {
    std::mt19937_64 eng(0);
    std::uniform_int_distribution<uint32_t> distr;
    uint32_t base = 0xDEADBEEF;
    uint32_t mod = (1 << 16) - 123;
    uint32_t exp = 0x0;
    
    // uint64_t base = 0xDEADBEEF;
    // uint64_t mod = (1 << 16) - 123;
    // uint64_t exp = 0x0;

    uint64_t tstart = __rdtsc();
    uint64_t tend = __rdtsc();
    
    float average = 0.0;

    fprintf(stderr, "all zeros\n");

    uint32_t result = mod_exp<uint32_t>(base, exp, mod);
    // uint64_t result = mod_exp<uint64_t>(base, exp, mod);
    for (int i = 0; i < 30; i++){
        exp = 0x0;
        tstart = __rdtsc();
        result = mod_exp<uint32_t>(base, exp, mod);
        // result = mod_exp<uint64_t>(base, exp, mod);
        // result = test<uint32_t>(exp);
        // result = test2<uint32_t>(exp);
        // result = test3<uint32_t>(exp);
        // result = test4<uint32_t>(exp);
        tend = __rdtsc();
        if (i >= 20){
            average += (tend - tstart);
            fprintf(stderr, "%lu,", tend - tstart);
            fprintf(stdout, "%u\n", result);
        }
    }
    fprintf(stderr, "Average: %f\n", average / 10.0);

    average = 0.0;
    fprintf(stderr, "all ones\n");
    for (int i = 0; i < 30; i++){
        exp = 0xFFFFFFFF;
        tstart = __rdtsc();
        result = mod_exp<uint32_t>(base, exp, mod);
        // result = mod_exp<uint64_t>(base, exp, mod);
        // result = test<uint32_t>(exp);
        // result = test2<uint32_t>(exp);
        // result = test3<uint32_t>(exp);
        // result = test4<uint32_t>(exp);
        tend = __rdtsc();
        if (i >= 20){
            average += (tend - tstart);
            fprintf(stderr, "%lu,", tend - tstart);
            fprintf(stdout, "%u\n", result);
        }
    }
    fprintf(stderr, "Average: %f\n", average / 10.0);

    average = 0.0;
    fprintf(stderr, "half half\n");
    for (int i = 0; i < 30; i++){
        exp = 0x0000FFFF;
        tstart = __rdtsc();
        result = mod_exp<uint32_t>(base, exp, mod);
        // result = mod_exp<uint64_t>(base, exp, mod);
        // result = test<uint32_t>(exp);
        // result = test2<uint32_t>(exp);
        // result = test3<uint32_t>(exp);
        // result = test4<uint32_t>(exp);
        tend = __rdtsc();
        if (i >= 20){
            average += (tend - tstart);
            fprintf(stderr, "%lu,", tend - tstart);
            fprintf(stdout, "%u\n", result);
        }
    }
    fprintf(stderr, "Average: %f\n", average / 10.0);

    average = 0.0;
    fprintf(stderr, "random\n");
    for (int i = 0; i < 30; i++){
        exp = distr(eng);
        tstart = __rdtsc();
        result = mod_exp<uint32_t>(base, exp, mod);
        // result = mod_exp<uint64_t>(base, exp, mod);
        // result = test<uint32_t>(exp);
        // result = test2<uint32_t>(exp);
        // result = test3<uint32_t>(exp);
        // result = test4<uint32_t>(exp);
        tend = __rdtsc();
        if (i >= 20){
            average += (tend - tstart);
            fprintf(stderr, "%lu,", tend - tstart);
            fprintf(stdout, "%u\n", result);
        }
    }
    fprintf(stderr, "Average: %f\n", average / 10.0);

    return 0;
}