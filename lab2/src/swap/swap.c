#include "swap.h"

void Swap(char *left, char *right)
{
    char temp = *left;   // сохраняем значение по адресу left
    *left = *right;      // записываем в left то, что было в right
    *right = temp;       // в right кладём сохранённое
}
