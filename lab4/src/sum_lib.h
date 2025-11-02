#ifndef SUM_LIB_H
#define SUM_LIB_H

#include <stddef.h>
#include <stdint.h>

/*
 * Функция считает сумму элементов массива на полуинтервале [begin, end)
 * arr — указатель на массив
 * begin, end — границы диапазона
 * Возвращает сумму элементов.
 */
int64_t sum_range(const int *arr, size_t begin, size_t end);

#endif // SUM_LIB_H
