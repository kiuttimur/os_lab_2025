#include "factorial.h"

uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod) {
    uint64_t result = 0;
    a = a % mod;
    while (b > 0) {
        if (b % 2 == 1)
            result = (result + a) % mod;
        a = (a * 2) % mod;
        b /= 2;
    }
    return result % mod;
}

uint64_t ComputeFactorial(uint64_t start, uint64_t end, uint64_t mod) {
    uint64_t result = 1;
    if (start > end) {
        return 1;
    }
    for (uint64_t i = start; i <= end; i++) {
        result = MultModulo(result, i, mod);
    }
    return result;
}