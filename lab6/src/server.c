#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>  // Добавляем для true/false
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include "factorial.h"

volatile sig_atomic_t server_running = 1;
int server_fd = -1;

struct FactorialArgs {
    uint64_t begin;
    uint64_t end;
    uint64_t mod;
};

void *ThreadFactorial(void *args) {
    struct FactorialArgs *fargs = (struct FactorialArgs *)args;
    uint64_t *result = malloc(sizeof(uint64_t));
    if (result) {
        *result = ComputeFactorial(fargs->begin, fargs->end, fargs->mod);
    }
    return result;
}

void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down server...\n", sig);
    server_running = 0;
    
    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    int tnum = -1;
    int port = -1;

    while (true) {
        static struct option options[] = {
            {"port", required_argument, 0, 0},
            {"tnum", required_argument, 0, 0},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "", options, &option_index);

        if (c == -1) break;

        switch (c) {
        case 0:
            switch (option_index) {
            case 0:
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Invalid port: %s\n", optarg);
                    return 1;
                }
                break;
            case 1:
                tnum = atoi(optarg);
                if (tnum <= 0) {
                    fprintf(stderr, "Invalid thread number: %s\n", optarg);
                    return 1;
                }
                break;
            default:
                printf("Index %d is out of options\n", option_index);
            }
            break;
        case '?':
            printf("Unknown argument\n");
            break;
        default:
            fprintf(stderr, "getopt returned character code 0%o?\n", c);
        }
    }

    if (!server_running) {
        return 0;
    }

    if (port == -1 || tnum == -1) {
        fprintf(stderr, "Using: %s --port 20001 --tnum 4\n", argv[0]);
        return 1;
    }
    //1 2 Создание и настройка сокета
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons((uint16_t)port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
    // 3. Привязка и начало прослушивания
    int err = bind(server_fd, (struct sockaddr *)&server, sizeof(server));
    if (err < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    err = listen(server_fd, 128);
    if (err < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server listening on port %d with %d threads\n", port, tnum);
    printf("Press Ctrl+C to stop the server\n");
    // 4. Основной цикл обработки соединений
    while (server_running) {
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);
        // 5. Принятие соединения
        int client_fd = accept(server_fd, (struct sockaddr *)&client, &client_len);

        if (client_fd < 0) {
            if (errno != EINTR && server_running) {
                perror("accept");
            }
            continue;
        }

        printf("New client connected\n");

        while (server_running) {
            unsigned int buffer_size = sizeof(uint64_t) * 3;
            char from_client[buffer_size];
            // 6. Получение задания от клиента
            int read_bytes = recv(client_fd, from_client, buffer_size, MSG_WAITALL);
            
            if (read_bytes == 0) {
                printf("Client disconnected\n");
                break;
            }
            if (read_bytes < 0) {
                perror("recv");
                break;
            }
            if (read_bytes != (int)buffer_size) {
                fprintf(stderr, "Client sent wrong data format: %d bytes instead of %u\n", 
                        read_bytes, buffer_size);
                break;
            }
            // 7. Извлечение данных
            uint64_t begin = 0, end = 0, mod = 0;
            memcpy(&begin, from_client, sizeof(uint64_t));
            memcpy(&end, from_client + sizeof(uint64_t), sizeof(uint64_t));
            memcpy(&mod, from_client + 2 * sizeof(uint64_t), sizeof(uint64_t));

            printf("Received task: factorial from %lu to %lu mod %lu\n", begin, end, mod);

            if (mod == 0) {
                fprintf(stderr, "Error: mod cannot be zero\n");
                break;
            }
            // 8. Распараллеливание вычислений
            pthread_t threads[tnum];
            struct FactorialArgs args[tnum];
            uint64_t range_size = (end - begin + 1) / tnum;
            uint64_t remainder = (end - begin + 1) % tnum;
            uint64_t current = begin;

            for (int i = 0; i < tnum && server_running; i++) {
                args[i].begin = current;
                args[i].end = current + range_size - 1 + ((uint64_t)i < remainder ? 1 : 0);
                args[i].mod = mod;
                current = args[i].end + 1;

                if (pthread_create(&threads[i], NULL, ThreadFactorial, &args[i])) {
                    perror("pthread_create");
                    break;
                }
            }
            // 9. Сбор результатов от потоков
            uint64_t total = 1;
            for (int i = 0; i < tnum && server_running; i++) {
                uint64_t *result = NULL;
                pthread_join(threads[i], (void **)&result);
                if (result) {
                    total = MultModulo(total, *result, mod);
                    free(result);
                }
            }

            printf("Computed result: %lu\n", total);
            // 10. Отправка результата клиенту
            char buffer[sizeof(total)];
            memcpy(buffer, &total, sizeof(total));
            if (send(client_fd, buffer, sizeof(total), 0) < 0) {
                perror("send");
                break;
            }
            
            printf("Result sent to client\n");
            break;
        }
        // 11. Закрытие соединения
        close(client_fd);
    }

    printf("Server shutdown complete\n");
    if (server_fd != -1) {
        close(server_fd);
    }
    
    return 0;
}