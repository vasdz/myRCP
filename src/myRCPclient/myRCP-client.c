#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>
#include <getopt.h>
#include <pwd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "libmysyslog.h"

// Размер буфера для получения ответа от сервера
#define BUFFER_SIZE 4096
// Таймаут ожидания ответа по UDP
#define UDP_TIMEOUT_SEC 5

// Вывод справки по использованию программы
void print_usage(const char *program_name) {
    printf("Использование: %s [ОПЦИИ]\n", program_name);
    printf("Доступные опции:\n");
    printf("  -c, --command \"bash_command\"     Команда bash для выполнения\n");
    printf("  -h, --host \"ip_address\"          IP-адрес сервера\n");
    printf("  -p, --port PORT                  Порт сервера\n");
    printf("  -s, --stream                     Использовать TCP (потоковый сокет)\n");
    printf("  -d, --dgram                      Использовать UDP (датаграмный сокет)\n");
    printf("      --help                       Показать эту справку\n");
}

int main(int argc, char *argv[]) {
    char *user_command = NULL;
    char *server_ip = NULL;
    int server_port = 0;
    int is_tcp = 0, is_udp = 0;

    // Опции командной строки
    static struct option options_list[] = {
        {"command", required_argument, 0, 'c'},
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"stream", no_argument, 0, 's'},
        {"dgram", no_argument, 0, 'd'},
        {"help", no_argument, 0, 0},
        {NULL, 0, NULL, 0}
    };

    // Парсинг аргументов командной строки
    int current_opt, option_index;
    while ((current_opt = getopt_long(argc, argv, "c:h:p:sd", options_list, &option_index)) != -1) {
        switch (current_opt) {
            case 'c':
                user_command = strdup(optarg);
                break;
            case 'h':
                server_ip = strdup(optarg);
                break;
            case 'p':
                server_port = atoi(optarg);
                break;
            case 's':
                is_tcp = 1;
                break;
            case 'd':
                is_udp = 1;
                break;
            case 0:
                if (strcmp(options_list[option_index].name, "help") == 0) {
                    print_usage(argv[0]);
                    return 0;
                }
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    // Проверка обязательных параметров
    if (!user_command || !server_ip || server_port <= 0 || (!is_tcp && !is_udp)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Получение имени текущего пользователя
    struct passwd *pw_info = getpwuid(getuid());
    if (!pw_info) {
        log_error("Не удалось получить имя пользователя: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    const char *username = pw_info->pw_name;

    // Формирование JSON-запроса
    struct json_object *request = json_object_new_object();
    if (!request) {
        log_error("Ошибка создания JSON-объекта");
        return EXIT_FAILURE;
    }

    json_object_object_add(request, "login", json_object_new_string(username));
    json_object_object_add(request, "command", json_object_new_string(user_command));
    const char *json_request = json_object_to_json_string(request);

    log_info("Сформирован запрос от '%s': %s", username, json_request);

    // Создание сокета
    int socket_fd = socket(AF_INET, is_tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        log_error("Ошибка создания сокета: %s", strerror(errno));
        json_object_put(request);
        return EXIT_FAILURE;
    }

    // Настройка адреса сервера
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        log_error("Неверный формат IP-адреса: %s", server_ip);
        close(socket_fd);
        json_object_put(request);
        return EXIT_FAILURE;
    }

    // Обработка TCP
    if (is_tcp) {
        if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            log_error("Ошибка подключения по TCP: %s", strerror(errno));
            close(socket_fd);
            json_object_put(request);
            return EXIT_FAILURE;
        }

        if (send(socket_fd, json_request, strlen(json_request), 0) < 0) {
            log_error("Ошибка отправки TCP-запроса: %s", strerror(errno));
            close(socket_fd);
            json_object_put(request);
            return EXIT_FAILURE;
        }

        char buffer[BUFFER_SIZE] = {0};
        ssize_t received_bytes = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
        if (received_bytes > 0) {
            buffer[received_bytes] = '\0';
            printf("Ответ от сервера:\n%s\n", buffer);
            log_info("TCP-ответ: %s", buffer);
        } else {
            log_warning("Не получен ответ от сервера: %s", strerror(errno));
        }

    // Обработка UDP
    } else {
        struct timeval timeout = {UDP_TIMEOUT_SEC, 0};
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        if (sendto(socket_fd, json_request, strlen(json_request), 0,
                   (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            log_error("Ошибка отправки UDP-запроса: %s", strerror(errno));
            close(socket_fd);
            json_object_put(request);
            return EXIT_FAILURE;
        }

        char buffer[BUFFER_SIZE] = {0};
        socklen_t addr_len = sizeof(server_addr);
        ssize_t received_bytes = recvfrom(socket_fd, buffer, sizeof(buffer) - 1, 0,
                                          (struct sockaddr *)&server_addr, &addr_len);

        if (received_bytes > 0) {
            buffer[received_bytes] = '\0';
            printf("Ответ от сервера:\n%s\n", buffer);
            log_info("UDP-ответ: %s", buffer);
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                log_warning("Таймаут ожидания ответа от сервера");
                fprintf(stderr, "UDP: время ожидания истекло\n");
            } else {
                log_error("Ошибка получения UDP-ответа: %s", strerror(errno));
            }
        }
    }

    // Очистка ресурсов
    close(socket_fd);
    json_object_put(request);
    free(user_command);
    free(server_ip);

    return 0;
}
