#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <json-c/json.h>
#include <signal.h>
#include <sys/stat.h>
#include "libmysyslog.h"

// Размер буфера для обработки запросов
#define BUFFER_SIZE 4096
// Путь к конфигурационным файлам
#define CONFIG_PATH "/etc/myRPC/myRPC.conf"
#define USERS_LIST_PATH "/etc/myRPC/users.conf"

// Получение текущего времени в миллисекундах
static inline double get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// Проверка разрешён ли пользователь
static int check_allowed_user(const char *user) {
    FILE *file = fopen(USERS_LIST_PATH, "r");
    if (!file) {
        log_error("Не удалось открыть %s: %s", USERS_LIST_PATH, strerror(errno));
        return 0;
    }

    char line[128];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, user) == 0) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

// Чтение содержимого файла
static char* read_file_content(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) return strdup("(не найден)");

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char *content = malloc(size + 1);
    if (!content) {
        fclose(file);
        return strdup("(ошибка памяти)");
    }

    fread(content, 1, size, file);
    content[size] = '\0';
    fclose(file);
    return content;
}

// Экранирование команды для безопасного выполнения
static char* escape_shell_command(const char *input) {
    size_t len = strlen(input);
    char *buf = malloc(len * 4 + 3);
    if (!buf) return NULL;

    char *p = buf;
    *p++ = '\'';
    for (size_t i = 0; i < len; ++i) {
        if (input[i] == '\'') {
            memcpy(p, "'\\''", 4);
            p += 4;
        } else {
            *p++ = input[i];
        }
    }
    *p++ = '\'';
    *p = '\0';
    return buf;
}

// Обработка JSON-запроса
static void handle_request(const char *request_str, char *response_buffer) {
    struct json_object *parsed = json_tokener_parse(request_str);
    struct json_object *response = json_object_new_object();

    if (!parsed || !json_object_is_type(parsed, json_type_object)) {
        json_object_object_add(response, "code", json_object_new_int(1));
        json_object_object_add(response, "result", json_object_new_string("Некорректный формат JSON"));
        goto cleanup;
    }

    struct json_object *login_obj, *command_obj;
    if (!json_object_object_get_ex(parsed, "login", &login_obj) ||
        !json_object_object_get_ex(parsed, "command", &command_obj) ||
        !json_object_is_type(login_obj, json_type_string) ||
        !json_object_is_type(command_obj, json_type_string)) {

        json_object_object_add(response, "code", json_object_new_int(1));
        json_object_object_add(response, "result", json_object_new_string("Отсутствуют обязательные поля"));
        goto cleanup;
    }

    const char *username = json_object_get_string(login_obj);
    const char *command = json_object_get_string(command_obj);

    if (!check_allowed_user(username)) {
        json_object_object_add(response, "code", json_object_new_int(1));
        json_object_object_add(response, "result", json_object_new_string("Пользователь не авторизован"));
        goto cleanup;
    }

    char tmp_path[] = "/tmp/myRPC_XXXXXX";
    int fd = mkstemp(tmp_path);
    if (fd < 0) {
        log_error("mkstemp: %s", strerror(errno));
        json_object_object_add(response, "code", json_object_new_int(1));
        json_object_object_add(response, "result", json_object_new_string("Ошибка создания временного файла"));
        goto cleanup;
    }
    close(fd);

    char stdout_path[256], stderr_path[256];
    snprintf(stdout_path, sizeof(stdout_path), "%s.stdout", tmp_path);
    snprintf(stderr_path, sizeof(stderr_path), "%s.stderr", tmp_path);

    char *escaped_cmd = escape_shell_command(command);
    if (!escaped_cmd) {
        json_object_object_add(response, "code", json_object_new_int(1));
        json_object_object_add(response, "result", json_object_new_string("Ошибка экранирования команды"));
        goto cleanup;
    }

    char exec_cmd[1024];
    snprintf(exec_cmd, sizeof(exec_cmd), "sh -c %s > %s 2> %s", escaped_cmd, stdout_path, stderr_path);
    free(escaped_cmd);

    double start_time = get_current_time_ms();
    int exit_code = system(exec_cmd);
    double duration = get_current_time_ms() - start_time;

    char *output = read_file_content(exit_code == 0 ? stdout_path : stderr_path);

    json_object_object_add(response, "code", json_object_new_int(exit_code == 0 ? 0 : 1));
    json_object_object_add(response, "duration_ms", json_object_new_double(duration));
    json_object_object_add(response, "result", json_object_new_string(output));

    free(output);

cleanup:
    strncpy(response_buffer, json_object_to_json_string(response), BUFFER_SIZE - 1);
    response_buffer[BUFFER_SIZE - 1] = '\0';
    json_object_put(response);
    if (parsed) json_object_put(parsed);
}

// Загрузка параметров из конфигурации
static void load_server_config(int *port, int *tcp_mode) {
    FILE *file = fopen(CONFIG_PATH, "r");
    if (!file) {
        log_warning("Не удалось открыть %s: %s", CONFIG_PATH, strerror(errno));
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        if (sscanf(line, "port = %d", port) == 1) continue;
        if (strstr(line, "socket_type = dgram")) *tcp_mode = 0;
        if (strstr(line, "socket_type = stream")) *tcp_mode = 1;
    }

    fclose(file);
}

int main() {
    log_info("Запуск сервера myRPC");

    int server_port = 1234;
    int use_tcp = 1;
    load_server_config(&server_port, &use_tcp);

    log_info("Конфигурация загружена: порт=%d, тип соединения: %s", server_port, use_tcp ? "TCP" : "UDP");

    int sockfd = socket(AF_INET, use_tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (sockfd < 0) {
        log_error("Ошибка создания сокета: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(server_port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(sockfd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        log_error("Ошибка привязки сокета: %s", strerror(errno));
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (use_tcp && listen(sockfd, 5) < 0) {
        log_error("Ошибка запуска прослушивания: %s", strerror(errno));
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    log_info("Сервер ожидает подключений...");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        char buffer[BUFFER_SIZE] = {0};

        if (use_tcp) {
            int client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_len);
            if (client_fd < 0) continue;

            ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            buffer[received > 0 ? received : 0] = '\0';

            log_info("TCP-запрос от %s: %s", inet_ntoa(client_addr.sin_addr), buffer);

            char response[BUFFER_SIZE];
            handle_request(buffer, response);
            send(client_fd, response, strlen(response), 0);
            close(client_fd);
        } else {
            ssize_t received = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                        (struct sockaddr*)&client_addr, &addr_len);
            buffer[received > 0 ? received : 0] = '\0';

            log_info("UDP-запрос от %s: %s", inet_ntoa(client_addr.sin_addr), buffer);

            char response[BUFFER_SIZE];
            handle_request(buffer, response);
            sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&client_addr, addr_len);
        }
    }

    close(sockfd);
    return 0;
}
