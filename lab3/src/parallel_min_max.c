

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "find_min_max.h"
#include "utils.h"

// ------------------------------------------------------------------------
// Вспомогательная функция для деления диапазона массива
// total — длина массива
// parts — количество процессов
// i — номер текущего процесса
// begin, end — возвращаемые индексы начала и конца диапазона
// ------------------------------------------------------------------------
static void split_range(unsigned int total, unsigned int parts, unsigned int i,
                        unsigned int *begin, unsigned int *end) {
  unsigned int base = total / parts;     // базовая длина куска
  unsigned int rem  = total % parts;     // остаток, который нужно "раскидать"
  *begin = i * base + (i < rem ? i : rem);     // равномерное распределение
  *end   = *begin + base + (i < rem ? 1 : 0);  // конец диапазона
}


int main(int argc, char **argv) {
  int seed = -1;        // значение seed для генерации
  int array_size = -1;  // размер массива
  int pnum = -1;        // количество процессов
  bool with_files = false; // способ обмена (по умолчанию pipe)

  // -----------------------------
  // Обработка аргументов командной строки
  // -----------------------------
  while (true) {
    static struct option options[] = {
        {"seed",        required_argument, 0, 0},
        {"array_size",  required_argument, 0, 0},
        {"pnum",        required_argument, 0, 0},
        {"by_files",    no_argument,       0, 'f'},
        {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);
    if (c == -1) break;

    switch (c) {
      case 0:
        // обработка опций --seed, --array_size, --pnum, --by_files
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            if (seed <= 0) {
              fprintf(stderr, "Error: --seed must be positive\n");
              return 1;
            }
            break;
          case 1:
            array_size = atoi(optarg);
            if (array_size <= 0) {
              fprintf(stderr, "Error: --array_size must be positive\n");
              return 1;
            }
            break;
          case 2:
            pnum = atoi(optarg);
            if (pnum <= 0) {
              fprintf(stderr, "Error: --pnum must be positive\n");
              return 1;
            }
            break;
          case 3:
            with_files = true;
            break;
          default:
            fprintf(stderr, "Unknown option index %d\n", option_index);
            return 1;
        }
        break;
      case 'f':
        with_files = true;
        break;
      default:
        fprintf(stderr, "Unknown argument\n");
        return 1;
    }
  }

  // Проверяем корректность аргументов
  if (seed == -1 || array_size == -1 || pnum == -1) {
    fprintf(stderr,
            "Usage: %s --seed NUM --array_size NUM --pnum NUM [--by_files]\n",
            argv[0]);
    return 1;
  }

  // Если процессов больше, чем элементов — уменьшаем до array_size
  if (pnum > array_size) pnum = array_size;

  // -----------------------------
  // Генерация массива
  // -----------------------------
  int *array = malloc(sizeof(int) * (size_t)array_size);
  if (!array) {
    perror("malloc");
    return 1;
  }
  GenerateArray(array, array_size, seed);

  // -----------------------------
  // Создаём пайпы, если выбран режим pipe
  // -----------------------------
  int (*pipes)[2] = NULL;
  if (!with_files) {
    pipes = calloc((size_t)pnum, sizeof(int[2]));
    if (!pipes) {
      perror("calloc pipes");
      free(array);
      return 1;
    }
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipes[i]) == -1) {
        perror("pipe");
        free(pipes);
        free(array);
        return 1;
      }
    }
  }

  // Засекаем время выполнения
  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  int active_child_processes = 0;

  // -----------------------------
  // Создаём pnum процессов
  // -----------------------------
  for (int i = 0; i < pnum; i++) {
    unsigned int begin, end;
    split_range((unsigned)array_size, (unsigned)pnum, (unsigned)i, &begin, &end);

    pid_t child_pid = fork();
    if (child_pid < 0) {
      perror("fork");
      free(array);
      if (pipes) free(pipes);
      return 1;
    }

    if (child_pid == 0) {
      // ========== Код дочернего процесса ==========
      struct MinMax mm = GetMinMax(array, begin, end);

      if (with_files) {
    // --- вариант с обменом через файлы ---

    // Формируем уникальное имя файла, чтобы каждый процесс писал в свой
    char fname[64];
    snprintf(fname, sizeof(fname), "mm_%d.tmp", i);

    // Открываем файл для записи (создаём или очищаем, если он уже есть)
    // 0644 — права доступа: владелец может читать и писать, остальные — только читать
    int fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { 
        // если не удалось открыть файл 
        perror("open (child)"); 
        _exit(1); // завершаем дочерний процесс с ошибкой
    }

    // Пишем структуру MinMax в файл.
    // write() возвращает количество реально записанных байт.
    ssize_t written = write(fd, &mm, sizeof(mm));

    // Проверяем, записали ли мы столько байт, сколько ожидалось
    if (written != (ssize_t)sizeof(mm)) {
        perror("write (child)");  // сообщение об ошибке
        close(fd);
        _exit(1);
    }

    // Закрываем файл, освобождая дескриптор
    close(fd);

} else {
    // --- вариант с обменом через pipe (канал в памяти) ---

    // Закрываем ненужный конец канала (чтение)
    close(pipes[i][0]);

    // Записываем структуру MinMax в pipe
    ssize_t written = write(pipes[i][1], &mm, sizeof(mm));

    // Проверяем, удалось ли записать все байты
    if (written != (ssize_t)sizeof(mm)) {
        perror("write (pipe child)");
        close(pipes[i][1]);
        _exit(1);
    }

    // Закрываем конец канала для записи — сигнализируем, что всё отправили
    close(pipes[i][1]);
}

      _exit(0); // завершаем ребёнка
    } else {
      // ========== Код родителя ==========
      active_child_processes++;
      if (!with_files) close(pipes[i][1]); // родителю запись не нужна
    }
  }

  // -----------------------------
  // Ожидаем завершения всех дочерних процессов
  // -----------------------------
  while (active_child_processes > 0) {
    waitpid(-1, NULL, 0);
    active_child_processes--;
  }

  // -----------------------------
  // Считываем результаты от всех процессов
  // -----------------------------
  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  for (int i = 0; i < pnum; i++) {
    struct MinMax got;

    if (with_files) {
    // --- вариант с обменом через файлы ---

    // Формируем имя временного файла, из которого будем читать
    char fname[64];
    snprintf(fname, sizeof(fname), "mm_%d.tmp", i);

    // Открываем файл только для чтения
    int fd = open(fname, O_RDONLY);
    if (fd == -1) {
        perror("open (parent)");
        continue; // переходим к следующему файлу
    }

    // Читаем данные из файла в структуру got
    ssize_t read_bytes = read(fd, &got, sizeof(got));

    // Проверяем, удалось ли прочитать все байты (иначе ошибка)
    if (read_bytes != (ssize_t)sizeof(got)) {
        perror("read (parent)");
        close(fd);
        continue;
    }

    // Закрываем файл
    close(fd);

    // Удаляем временный файл, чтобы не засорять каталог
    unlink(fname);

} else {
    // --- вариант с обменом через pipe ---

    // Читаем структуру MinMax из pipe
    ssize_t read_bytes = read(pipes[i][0], &got, sizeof(got));

    // Проверяем, успешно ли прочитали все байты
    if (read_bytes != (ssize_t)sizeof(got)) {
        perror("read (pipe parent)");
        close(pipes[i][0]);
        continue;
    }

    // Закрываем конец канала после чтения
    close(pipes[i][0]);
}


    // обновляем глобальные min и max
    if (got.min < min_max.min) min_max.min = got.min;
    if (got.max > min_max.max) min_max.max = got.max;
  }

  // -----------------------------
  // Фиксируем время завершения
  // -----------------------------
  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  // вычисляем разницу (в миллисекундах)
  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  // освобождаем ресурсы
  free(array);
  if (pipes) free(pipes);

  // -----------------------------
  // Выводим результат
  // -----------------------------
  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %f ms\n", elapsed_time);
  fflush(NULL);

  return 0;
}
