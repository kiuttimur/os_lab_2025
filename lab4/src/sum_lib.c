#include "sum_lib.h"

int64_t sum_range(const int *arr, size_t begin, size_t end) {
    int64_t total = 0;
    for (size_t i = begin; i < end; ++i)
        total += arr[i];
    return total;
}
