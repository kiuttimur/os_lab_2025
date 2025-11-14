#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

long long result = 1;   // общий результат 
int mod_global;         

struct FactArgs {
    int start;
    int end;
};

// Функция для вычисления части факториала
void *ThreadFactorial(void *args) {
    struct FactArgs *arg = (struct FactArgs *)args;

    long long local_res = 1;
    for (int i = arg->start; i <= arg->end; i++) {
        local_res = (local_res * i) % mod_global;
    }

    // --- Критическая секция ---
    pthread_mutex_lock(&mut);
    result = (result * local_res) % mod_global;
    pthread_mutex_unlock(&mut);
    // --------------------------

    return NULL;
}

int main(int argc, char **argv) {
    int k = -1;
    int pnum = -1;
    int mod = -1;

    // Аргументы командной строки
    while (1) {
        static struct option long_options[] = {
            {"k", required_argument, 0, 'k'},
            {"pnum", required_argument, 0, 'p'},
            {"mod", required_argument, 0, 'm'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "k:p:m:", long_options, &option_index);
        if (c == -1) break;

        switch (c) {
            case 'k':
                k = atoi(optarg);
                break;
            case 'p':
                pnum = atoi(optarg);
                break;
            case 'm':
                mod = atoi(optarg);
                break;
            default:
                printf("Usage: %s -k num --pnum num --mod num\n", argv[0]);
                return 1;
        }
    }

    if (k <= 0 || pnum <= 0 || mod <= 0) {
        printf("Arguments must be positive\n");
        return 1;
    }

    mod_global = mod;

    pthread_t threads[pnum];
    struct FactArgs args[pnum];

    int chunk = k / pnum;
    int remainder = k % pnum;

    int current_start = 1;

    // Разделяем диапазоны между потоками
    for (int i = 0; i < pnum; i++) {
        args[i].start = current_start;
        args[i].end = current_start + chunk - 1;

        if (i < remainder)
            args[i].end++;

        current_start = args[i].end + 1;

        pthread_create(&threads[i], NULL, ThreadFactorial, &args[i]);
    }

    // Ждём все потоки
    for (int i = 0; i < pnum; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Result: %lld\n", result);
    return 0;
}
