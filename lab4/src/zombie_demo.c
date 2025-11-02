#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

/*
  zombie_demo:
   - fork() -> child -> exit(0) немедленно
   - parent: НЕ ждёт N секунд (зомби виден), потом вызывает waitpid() и прибирает
*/

int main(int argc, char *argv[]) {
    int sleep_sec = 20;
    if (argc >= 2) {
        sleep_sec = atoi(argv[1]);
        if (sleep_sec < 0) sleep_sec = 0;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // === child ===
        printf("[child ] pid=%d ppid=%d: exiting now -> will become Z until parent waits\n",
               getpid(), getppid());
        _exit(0); // немедленное завершение (станет зомби, пока родитель не подберёт)
    }

    // === parent ===
    printf("[parent] pid=%d: child pid=%d\n", getpid(), pid);
    printf("[parent] sleeping %d sec without wait() — check the child in process list (STAT should be 'Z')\n",
           sleep_sec);
    printf("         try: ps -o pid,ppid,stat,cmd -p %d\n", pid);

    // Родитель демонстративно ничего не ждёт, даём время увидеть зомби
    for (int i = 0; i < sleep_sec; ++i) {
        sleep(1);
    }
    printf("\n[parent] now calling waitpid() to reap the child…\n");

    // Забираем код завершения ребёнка — запись исчезнет из таблицы процессов
    int status = 0;
    pid_t got = waitpid(pid, &status, 0);
    if (got == -1) {
        perror("waitpid");
    } else {
        if (WIFEXITED(status)) {
            printf("[parent] reaped child %d, exit_code=%d\n", got, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("[parent] reaped child %d, killed by signal %d\n", got, WTERMSIG(status));
        } else {
            printf("[parent] reaped child %d, status=0x%x\n", got, status);
        }
    }

    printf("[parent] check again: ps -o pid,ppid,stat,cmd -p %d  (should show nothing or non-Z)\n", pid);
    return 0;
}
