#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <program> [args...]\n", argv[0]);
        return 1;
    }

    // Создаём новый процесс
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    }

    if (pid == 0) {
        // === Код дочернего процесса ===
        // Запускаем другую программу вместо текущей
        // argv[1] — это имя программы, argv+1 — её аргументы
        execvp(argv[1], &argv[1]);

        // Если execvp не выполнился, выводим ошибку
        perror("exec failed");
        exit(1);
    } else {
        // === Код родительского процесса ===
        int status;
        waitpid(pid, &status, 0);  // ждём завершения дочернего процесса
        printf("Child process finished with code %d\n", WEXITSTATUS(status));
    }

    return 0;
}