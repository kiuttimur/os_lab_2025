#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

void *thread_func1(void *arg)
{
    printf("[T1] trying to lock mutex1...\n");
    pthread_mutex_lock(&mutex1);
    printf("[T1] locked mutex1\n");

    // небольшая пауза, чтобы T2 успел захватить mutex2
    sleep(1);

    printf("[T1] trying to lock mutex2...\n");
    pthread_mutex_lock(&mutex2);   // ← здесь можно застрять навсегда
    printf("[T1] locked mutex2 (this line will likely never print)\n");

    pthread_mutex_unlock(&mutex2);
    pthread_mutex_unlock(&mutex1);
    return NULL;
}

void *thread_func2(void *arg)
{
    printf("[T2] trying to lock mutex2...\n");
    pthread_mutex_lock(&mutex2);
    printf("[T2] locked mutex2\n");

    // небольшая пауза, чтобы T1 успел захватить mutex1
    sleep(1);

    printf("[T2] trying to lock mutex1...\n");
    pthread_mutex_lock(&mutex1);   // ← а тут застрянет второй поток
    printf("[T2] locked mutex1 (this line will likely never print)\n");

    pthread_mutex_unlock(&mutex1);
    pthread_mutex_unlock(&mutex2);
    return NULL;
}

int main(void)
{
    pthread_t t1, t2;

    if (pthread_create(&t1, NULL, thread_func1, NULL) != 0) {
        perror("pthread_create t1");
        return 1;
    }
    if (pthread_create(&t2, NULL, thread_func2, NULL) != 0) {
        perror("pthread_create t2");
        return 1;
    }

    // будем пытаться подождать оба потока
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("This line will almost never be reached because of deadlock\n");

    return 0;
}
