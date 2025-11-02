#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>        // <-- сигналы: signal(), kill(), SIGALRM, SIGKILL
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

/* ---------------- ЛОГИКА ТАЙМАУТА (ЛР-4) ---------------- */

// Массив PID-ов дочерних процессов, чтобы уметь их "прибить" по таймауту.
static pid_t *g_child_pids = NULL;
static int    g_pnum       = 0;

// Флаг: сработал ли SIGALRM (используем тип sig_atomic_t — безопасен в обработчике).
static volatile sig_atomic_t g_timeout_fired = 0;

// Обработчик сигнала SIGALRM: помечаем флаг и посылаем всем дочерним SIGKILL.
static void on_alarm(int signum) {
  (void)signum;                // не используем напрямую
  g_timeout_fired = 1;
  fprintf(stderr, "\n[timeout] Time is up → killing all children...\n");
  for (int i = 0; i < g_pnum; i++) {
    if (g_child_pids && g_child_pids[i] > 0) {
      // SIGKILL гарантированно завершает процесс
      kill(g_child_pids[i], SIGKILL);
    }
  }
}

int main(int argc, char **argv) {
  int seed = -1;        // значение seed для генерации
  int array_size = -1;  // размер массива
  int pnum = -1;        // количество процессов
  bool with_files = false; // способ обмена (по умолчанию pipe)
  int timeout_sec = -1; // <-- опциональный таймаут (секунды). -1 = без таймаута.

  // -----------------------------
  // Обработка аргументов командной строки
  // -----------------------------
  while (true) {
    static struct option options[] = {
        {"seed",        required_argument, 0, 0},
        {"array_size",  required_argument, 0, 0},
        {"pnum",        required_argument, 0, 0},
        {"by_files",    no_argument,       0, 'f'},
        {"timeout",     required_argument, 0, 0},   // <-- добавили timeout
        {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);
    if (c == -1) break;

    switch (c) {
      case 0:
        // обработка опций --seed, --array_size, --pnum, --by_files, --timeout
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
          case 4:
            timeout_sec = atoi(optarg);
            if (timeout_sec <= 0) {
              fprintf(stderr, "Error: --timeout must be positive seconds\n");
              return 1;
            }
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
            "Usage: %s --seed NUM --array_size NUM --pnum NUM [--by_files] [--timeout NUM]\n",
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
  // Подготовка под таймаут: храним PID'ы детей
  // -----------------------------
  g_pnum = pnum;
  g_child_pids = calloc((size_t)pnum, sizeof(pid_t));
  if (!g_child_pids) {
    perror("calloc child_pids");
    free(array);
    return 1;
  }

  // Если задан таймаут — вешаем обработчик и запускаем будильник
  if (timeout_sec > 0) {
    if (signal(SIGALRM, on_alarm) == SIG_ERR) {
      perror("signal(SIGALRM)");
      free(g_child_pids);
      free(array);
      return 1;
    }
    alarm((unsigned)timeout_sec); // через timeout_sec сек. придёт SIGALRM
  }

  // -----------------------------
  // Создаём пайпы, если выбран режим pipe
  // -----------------------------
  int (*pipes)[2] = NULL;
  if (!with_files) {
    pipes = calloc((size_t)pnum, sizeof(int[2]));
    if (!pipes) {
      perror("calloc pipes");
      free(g_child_pids);
      free(array);
      return 1;
    }
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipes[i]) == -1) {
        perror("pipe");
        free(pipes);
        free(g_child_pids);
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
      // в случае ошибки попробуем завершить уже созданных
      for (int k = 0; k < i; k++) if (g_child_pids[k] > 0) kill(g_child_pids[k], SIGKILL);
      if (pipes) free(pipes);
      free(g_child_pids);
      free(array);
      return 1;
    }

    if (child_pid == 0) {
      // ========== Код дочернего процесса ==========
      struct MinMax mm = GetMinMax(array, begin, end);

      if (with_files) {
        // --- запись результата в файл ---
        char fname[64];
        snprintf(fname, sizeof(fname), "mm_%d.tmp", i);
        int fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) { perror("open (child)"); _exit(1); }

        ssize_t written = write(fd, &mm, sizeof(mm));
        if (written != (ssize_t)sizeof(mm)) {
          perror("write (child)");
          close(fd);
          _exit(1);
        }
        close(fd);
      } else {
        // --- запись результата в pipe ---
        close(pipes[i][0]); // закрываем чтение
        ssize_t written = write(pipes[i][1], &mm, sizeof(mm));
        if (written != (ssize_t)sizeof(mm)) {
          perror("write (pipe child)");
          close(pipes[i][1]);
          _exit(1);
        }
        close(pipes[i][1]);
      }
      _exit(0); // завершаем ребёнка
    } else {
      // ========== Код родителя ==========
      g_child_pids[i] = child_pid;   // запомним PID для kill при таймауте
      active_child_processes++;
      if (!with_files) close(pipes[i][1]); // родителю запись не нужна
    }
  }

  // -----------------------------
  // Неблокирующее ожидание завершения всех детей (ЛР-4)
  // -----------------------------
  int reaped = 0;
  while (reaped < pnum) {
    int status = 0;
    pid_t got = waitpid(-1, &status, WNOHANG); // не блокируемся
    if (got > 0) {
      reaped++;
    } else if (got == 0) {
      // Никто пока не завершился — слегка подождём, чтобы не крутить ЦП
      // (если истечёт alarm, on_alarm() пошлёт SIGKILL оставшимся детям)
      usleep(50 * 1000); // 50ms
    } else {
      // Ошибка или детей уже нет
      if (errno == ECHILD) break;
      perror("waitpid");
      break;
    }
  }

  // Если все дети завершились раньше срока — отменим будильник.
  if (timeout_sec > 0) alarm(0);

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
      char fname[64];
      snprintf(fname, sizeof(fname), "mm_%d.tmp", i);
      int fd = open(fname, O_RDONLY);
      if (fd == -1) {
        // Если ребёнок был убит по таймауту, файла может не быть — это ок.
        perror("open (parent)");
        continue;
      }

      ssize_t read_bytes = read(fd, &got, sizeof(got));
      if (read_bytes != (ssize_t)sizeof(got)) {
        perror("read (parent)");
        close(fd);
        unlink(fname);
        continue;
      }
      close(fd);
      unlink(fname); // подчистить за собой
    } else {
      // --- вариант с обменом через pipe ---
      ssize_t read_bytes = read(pipes[i][0], &got, sizeof(got));
      if (read_bytes == 0) {
        // EOF: писателя нет (ребёнок мог быть убит по таймауту) — пропускаем
        close(pipes[i][0]);
        continue;
      }
      if (read_bytes != (ssize_t)sizeof(got)) {
        perror("read (pipe parent)");
        close(pipes[i][0]);
        continue;
      }
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
  free(g_child_pids);
  free(array);
  if (pipes) free(pipes);

  // -----------------------------
  // Выводим результат
  // -----------------------------
  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %f ms\n", elapsed_time);
  if (g_timeout_fired) {
    printf("[note] Some children were killed by timeout.\n");
  }
  fflush(NULL);

  return 0;
}
