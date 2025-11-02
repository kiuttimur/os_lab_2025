#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/time.h>

#include "utils.h"     // из ЛР3: GenerateArray
#include "sum_lib.h"   

struct SumArgs {
  const int *array;
  size_t begin;
  size_t end;
  int64_t partial;
};

/* Обёртка для потока */
static void *ThreadSum(void *args) {
  struct SumArgs *a = (struct SumArgs *)args;
  a->partial = sum_range(a->array, a->begin, a->end);
  return NULL;
}

/* Вспомогательная функция для деления массива */
static inline void split_range(size_t n, size_t parts, size_t i,
                               size_t *begin, size_t *end) {
  size_t base = n / parts, rem = n % parts;
  *begin = i * base + (i < rem ? i : rem);
  *end   = *begin + base + (i < rem ? 1 : 0);
}

static void usage(const char *prog) {
  fprintf(stderr, "Usage: %s --threads_num N --array_size N --seed N\n", prog);
}

int main(int argc, char **argv) {
  int threads_num = -1, array_size = -1, seed = -1;

  while (true) {
    static struct option opts[] = {
        {"threads_num", required_argument, 0, 0},
        {"array_size", required_argument, 0, 0},
        {"seed", required_argument, 0, 0},
        {0,0,0,0}};
    int idx = 0;
    int c = getopt_long(argc, argv, "", opts, &idx);
    if (c == -1) break;
    if (c == 0) {
      switch (idx) {
        case 0: threads_num = atoi(optarg); break;
        case 1: array_size = atoi(optarg); break;
        case 2: seed = atoi(optarg); break;
      }
    } else {
      usage(argv[0]); return 1;
    }
  }

  if (threads_num <= 0 || array_size <= 0 || seed <= 0) {
    usage(argv[0]); return 1;
  }

  int *array = malloc(sizeof(int) * (size_t)array_size);
  if (!array) { perror("malloc"); return 1; }
  GenerateArray(array, array_size, seed);

  pthread_t *threads = malloc(sizeof(pthread_t) * (size_t)threads_num);
  struct SumArgs *args = malloc(sizeof(struct SumArgs) * (size_t)threads_num);

  if (!threads || !args) {
    perror("malloc");
    free(array);
    free(threads);
    free(args);
    return 1;
  }

  struct timeval start, end;
  gettimeofday(&start, NULL);

  for (int i = 0; i < threads_num; ++i) {
    size_t b, e;
    split_range((size_t)array_size, (size_t)threads_num, (size_t)i, &b, &e);
    args[i].array = array;
    args[i].begin = b;
    args[i].end = e;
    pthread_create(&threads[i], NULL, ThreadSum, &args[i]);
  }

  int64_t total = 0;
  for (int i = 0; i < threads_num; ++i) {
    pthread_join(threads[i], NULL);
    total += args[i].partial;
  }

  gettimeofday(&end, NULL);
  double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                   (end.tv_usec - start.tv_usec) / 1000.0;

  printf("Total sum: %lld\n", (long long)total);
  printf("Elapsed (sum only): %.3f ms\n", elapsed);

  free(array);
  free(threads);
  free(args);
  return 0;
}
