#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include "factorial.h"

volatile sig_atomic_t stop_flag = 0;

struct Server {
    char ip[255];      // IP-адрес сервера
    int port;          // Порт сервера
};

struct ThreadData {
    struct Server server;  // Информация о сервере
    uint64_t begin;        // Начало диапазона вычислений
    uint64_t end;          // Конец диапазона вычислений  
    uint64_t mod;          // Модуль для вычислений
    uint64_t result;       // Результат от сервера
    int success;           // Флаг успешности
};

bool ConvertStringToUI64(const char *str, uint64_t *val) {
    char *end = NULL;
    unsigned long long i = strtoull(str, &end, 10);
    if (errno == ERANGE) {
        fprintf(stderr, "Out of uint64_t range: %s\n", str);
        return false;
    }
    if (errno != 0)
        return false;
    *val = i;
    return true;
}

void signal_handler(int sig) {
    stop_flag = 1;
    printf("\nReceived signal %d, shutting down...\n", sig);
}

void *ThreadServer(void *args) {
    struct ThreadData *data = (struct ThreadData *)args;
    data->success = 0;
    
    if (stop_flag) {
        pthread_exit(NULL);
    }
     // 1 DNS-запрос для получения IP
    struct hostent *hostname = gethostbyname(data->server.ip);
    if (hostname == NULL) {
        fprintf(stderr, "gethostbyname failed with %s\n", data->server.ip);
        pthread_exit(NULL);
    }
    // 2 Настройка адреса сервера
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(data->server.port);
    
    if (hostname->h_addr_list[0] != NULL) {
        server.sin_addr.s_addr = *((unsigned long *)hostname->h_addr_list[0]);
    } else {
        fprintf(stderr, "No address found for %s\n", data->server.ip);
        pthread_exit(NULL);
    }
    // 3. Создание сокета
    int sck = socket(AF_INET, SOCK_STREAM, 0);
    if (sck < 0) {
        fprintf(stderr, "Socket creation failed!\n");
        pthread_exit(NULL);
    }
    // 4. Подключение к серверу
    if (connect(sck, (struct sockaddr *)&server, sizeof(server)) < 0) {
        fprintf(stderr, "Connection to %s:%d failed\n", data->server.ip, data->server.port);
        close(sck);
        pthread_exit(NULL);
    }
    // 5. Подготовка данных для отправки(3 числа старт конец мод)
    char task[sizeof(uint64_t) * 3];
    memcpy(task, &data->begin, sizeof(uint64_t));
    memcpy(task + sizeof(uint64_t), &data->end, sizeof(uint64_t));
    memcpy(task + 2 * sizeof(uint64_t), &data->mod, sizeof(uint64_t));

    if (stop_flag) {
        close(sck);
        pthread_exit(NULL);
    }
    // 6. Отправка задания
    if (send(sck, task, sizeof(task), 0) < 0) {
        fprintf(stderr, "Send failed to %s:%d\n", data->server.ip, data->server.port);
        close(sck);
        pthread_exit(NULL);
    }
    // 7. Получение результата
    char response[sizeof(uint64_t)];
    int bytes_received = recv(sck, response, sizeof(response), 0);
    if (bytes_received < 0) {
        fprintf(stderr, "Receive failed from %s:%d\n", data->server.ip, data->server.port);
        close(sck);
        pthread_exit(NULL);
    }
    if (bytes_received != sizeof(response)) {
        fprintf(stderr, "Incomplete response from %s:%d\n", data->server.ip, data->server.port);
        close(sck);
        pthread_exit(NULL);
    }
    // 8. Сохранение результата
    memcpy(&data->result, response, sizeof(uint64_t));
    data->success = 1;
    close(sck);
    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    uint64_t k = 0;
    uint64_t mod = 0;
    char servers_file_path[255] = {'\0'};

    while (true) {
        if (stop_flag) {
            printf("Interrupted by user\n");
            return 1;
        }
        
        static struct option options[] = {
            {"k", required_argument, 0, 0},
            {"mod", required_argument, 0, 0},
            {"servers", required_argument, 0, 0},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "", options, &option_index);

        if (c == -1) break;

        switch (c) {
        case 0:
            switch (option_index) {
            case 0:
                if (!ConvertStringToUI64(optarg, &k)) {
                    fprintf(stderr, "Invalid k value\n");
                    return 1;
                }
                break;
            case 1:
                if (!ConvertStringToUI64(optarg, &mod)) {
                    fprintf(stderr, "Invalid mod value\n");
                    return 1;
                }
                break;
            case 2:
                strncpy(servers_file_path, optarg, sizeof(servers_file_path) - 1);
                servers_file_path[sizeof(servers_file_path) - 1] = '\0';
                break;
            default:
                printf("Index %d is out of options\n", option_index);
            }
            break;
        case '?':
            printf("Arguments error\n");
            break;
        default:
            fprintf(stderr, "getopt returned character code 0%o?\n", c);
        }
    }

    if (stop_flag) {
        printf("Interrupted by user\n");
        return 1;
    }

    if (k == 0 || mod == 0 || !strlen(servers_file_path)) {
        fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n", argv[0]);
        return 1;
    }

    FILE *servers_file = fopen(servers_file_path, "r");
    if (!servers_file) {
        perror("fopen servers file");
        return 1;
    }

    struct Server *servers = NULL;
    size_t servers_num = 0;
    char line[255];
    
    while (fgets(line, sizeof(line), servers_file)) {
        if (stop_flag) {
            fclose(servers_file);
            free(servers);
            return 1;
        }
        
        line[strcspn(line, "\n")] = 0;
        
        char *colon = strchr(line, ':');
        if (!colon) {
            fprintf(stderr, "Invalid server format: %s (expected ip:port)\n", line);
            continue;
        }
        *colon = '\0';
        int port = atoi(colon + 1);
        
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number: %s\n", colon + 1);
            continue;
        }
        
        servers = realloc(servers, (servers_num + 1) * sizeof(struct Server));
        if (!servers) {
            perror("realloc");
            fclose(servers_file);
            return 1;
        }
        
        strncpy(servers[servers_num].ip, line, sizeof(servers[servers_num].ip) - 1);
        servers[servers_num].ip[sizeof(servers[servers_num].ip) - 1] = '\0';
        servers[servers_num].port = port;
        servers_num++;
    }
    fclose(servers_file);

    if (stop_flag) {
        free(servers);
        return 1;
    }

    if (servers_num == 0) {
        fprintf(stderr, "No valid servers found in %s\n", servers_file_path);
        free(servers);
        return 1;
    }

    printf("Starting computation of %lu! mod %lu with %zu servers...\n", k, mod, servers_num);
    printf("Press Ctrl+C to cancel\n");

    struct ThreadData *threads_data = malloc(servers_num * sizeof(struct ThreadData));
    pthread_t *threads = malloc(servers_num * sizeof(pthread_t));
    
    if (!threads_data || !threads) {
        perror("malloc");
        free(servers);
        free(threads_data);
        free(threads);
        return 1;
    }
    
    uint64_t current_start = 1;
    for (size_t i = 0; i < servers_num; i++) {
        if (stop_flag) break;
        // Распределение вычислений
        uint64_t range_size = k / servers_num;
        uint64_t remaining = k % servers_num;
        
        threads_data[i].server = servers[i];
        threads_data[i].begin = current_start;
        threads_data[i].end = current_start + range_size - 1 + (i < remaining ? 1 : 0);
        threads_data[i].mod = mod;
        threads_data[i].result = 1;
        threads_data[i].success = 0;

        printf("Server %zu: %lu-%lu\n", i, threads_data[i].begin, threads_data[i].end);
        current_start = threads_data[i].end + 1;
        // Создание потоков для каждого сервера
        if (pthread_create(&threads[i], NULL, ThreadServer, &threads_data[i])) {
            fprintf(stderr, "Error creating thread for server %zu\n", i);
        }
    }

    uint64_t total = 1;
    int successful_servers = 0;
    
    for (size_t i = 0; i < servers_num; i++) {
        if (stop_flag) {
            printf("Cancelling... waiting for threads to finish\n");
        }
        pthread_join(threads[i], NULL);
        
        if (threads_data[i].success) {
            total = MultModulo(total, threads_data[i].result, mod);
            successful_servers++;
            printf("Server %zu returned: %lu\n", i, threads_data[i].result);
        } else {
            printf("Server %zu failed\n", i);
        }
    }

    if (stop_flag) {
        printf("Computation cancelled by user\n");
    } else if (successful_servers == (int)servers_num) {
        printf("Success! %lu! mod %lu = %lu\n", k, mod, total);
    } else {
        printf("Warning: only %d out of %zu servers completed successfully\n", 
               successful_servers, servers_num);
        if (successful_servers > 0) {
            printf("Partial result: %lu\n", total);
        } else {
            printf("No servers completed successfully\n");
        }
    }

    free(servers);
    free(threads_data);
    free(threads);
    return 0;
}