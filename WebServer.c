#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 4096

int server_socket; // 全局变量，用于存储服务器套接字
int response_count = 0; // 全局变量，用于记录请求编号
pthread_mutex_t response_count_mutex = PTHREAD_MUTEX_INITIALIZER; // 互斥锁，用于保护 response_count

// 信号处理函数
void handle_signal(int signal) {
    if (signal == SIGINT) {
        printf("\nReceived SIGINT (Ctrl+C). Stopping server...\n");
    } else if (signal == SIGTERM) {
        printf("\nReceived SIGTERM. Stopping server...\n");
    }
    printf("Closing server socket...\n");
    close(server_socket);
    printf("Server stopped.\n");
    exit(0);
}

// 发送 HTTP 响应
void send_http_response(int client_socket, const char *path, int current_response_number) {
    char response[BUFFER_SIZE];
    const char *content_type = "text/html";
    const char *status_line = "HTTP/1.1 200 OK\r\n";
    const char *body = "";

    // 根据请求路径设置响应内容
    if (strcmp(path, "/") == 0) {
        // 网页请求
        body = "<html><body><h1>Hello, World!</h1><p>This is request #%d</p></body></html>";
    } else if (strcmp(path, "/favicon.ico") == 0) {
        // favicon.ico 请求（返回空内容）
        body = "";
        content_type = "image/x-icon";
    } else {
        // 未知路径（返回 404 错误）
        status_line = "HTTP/1.1 404 Not Found\r\n";
        body = "<html><body><h1>404 Not Found</h1></body></html>";
    }

    // snprintf 是 C 标准库中的一个函数，用于将格式化的数据写入字符串缓冲区
    // 阻止缓冲区溢出，确保 response 的大小不超过 BUFFER_SIZE
    // 线程安全

    // 生成 HTTP 响应头
    snprintf(response, sizeof(response),
            "%s"
            "Content-Type: %s\r\n"
            "\r\n", // 空行分隔头部和正文
            status_line, content_type);

    // 生成 HTTP 正文
    if (strlen(body) > 0) {
        char body_buffer[BUFFER_SIZE];
        // 使用 snprintf 将 current_response_number 格式化到 body 中。
        snprintf(body_buffer, sizeof(body_buffer), body, current_response_number);
        // 使用 strncat 将格式化后的 body 追加到 response 中。
        strncat(response, body_buffer, sizeof(response) - strlen(response) - 1);
    }

    // 发送响应
    send(client_socket, response, strlen(response), 0);
    printf("Sent response:\n<<<<<<<<<<<<<<<<<<<<\n%s\n>>>>>>>>>>>>>>>>>>>>\n", response);
    close(client_socket);
}

// 处理客户端请求
void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg); // 释放动态分配的内存

    char receive_buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, receive_buffer, BUFFER_SIZE, 0);
    if (bytes_received < 0) {
        perror("Failed to receive data from client");
        close(client_socket);
        return NULL;
    } else if (bytes_received == 0) {
        printf("Client disconnected\n");
        close(client_socket);
        return NULL;
    }

    // 打印客户端请求
    receive_buffer[bytes_received] = '\0';
    printf("Received request:\n<<<<<<<<<<<<<<<<<<<<\n%s\n>>>>>>>>>>>>>>>>>>>>\n", receive_buffer);

    // 提取请求路径
    char *path = strtok(receive_buffer, " "); // 获取请求方法（如 GET）
    if (path != NULL) {
        path = strtok(NULL, " "); // 获取请求路径（如 / 或 /favicon.ico）
    }

    // 如果没有路径，默认返回根路径
    if (path == NULL) {
        path = "/";
    }

    // 获取响应编号
    pthread_mutex_lock(&response_count_mutex);
    int current_response_number = ++response_count;
    pthread_mutex_unlock(&response_count_mutex);

    
    // 发送 HTTP 响应
    send_http_response(client_socket, path, current_response_number);

    return NULL; // 确保所有路径都有返回值
}

int main() {
    // 注册信号处理函数
    signal(SIGINT, handle_signal);  // 捕获 Ctrl+C 信号
    signal(SIGTERM, handle_signal); // 捕获终止信号

    // 创建套接字
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        return 1;
    }

    // 设置 SO_REUSEADDR 选项
    // 默认情况下，当一个套接字关闭后，操作系统会保留该套接字绑定的地址和端口一段时间（称为 TIME_WAIT 状态）。
    // 启用 SO_REUSEADDR 后，可以立即重用该地址和端口，即使它仍处于 TIME_WAIT 状态。
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Failed to set SO_REUSEADDR");
        close(server_socket);
        return 1;
    }

    // 绑定套接字到端口
    struct sockaddr_in server_address;
    // 设置地址族为 AF_INET，表示使用 IPv4 地址。
    server_address.sin_family = AF_INET;
    // 设置 IP 地址为 INADDR_ANY，表示服务器将监听所有可用的网络接口（即所有网卡的 IP 地址）。
    server_address.sin_addr.s_addr = INADDR_ANY;
    // 设置端口号为 PORT（例如 8080）。
    // htons() 函数将主机字节序（通常是小端序）转换为网络字节序（大端序）。
    // 这是网络编程中的标准做法，以确保不同系统之间的兼容性。
    server_address.sin_port = htons(PORT);

    // (struct sockaddr *)&server_address 将 sockaddr_in 结构体强制转换为 sockaddr 类型，
    // 因为 bind() 需要 sockaddr 类型的指针。
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Failed to bind socket");
        close(server_socket);
        return 1;
    }

    // 监听连接
    // backlog 参数设置为 5，表示内核可以为该套接字排队最多 5 个连接请求。
    if (listen(server_socket, 5) < 0) {
        perror("Failed to listen on socket");
        close(server_socket);
        return 1;
    }

    printf("Server is listening on port %d...\n", PORT);

    // 接受客户端连接
    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_size = sizeof(client_address);
        int *client_socket = malloc(sizeof(int)); // 动态分配内存以存储客户端套接字
        if (client_socket == NULL) {
            perror("Failed to allocate memory for client socket");
            continue;
        }

        *client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_size);
        if (*client_socket < 0) {
            perror("Failed to accept client connection");
            free(client_socket);
            continue;
        }

        // inet_ntop() 是一个用于将二进制格式的 IP 地址转换为人类可读的字符串格式的函数。
        // 它是 inet_ntoa() 的替代品，支持 IPv4 和 IPv6 地址，并且是线程安全的。
        char client_ip[INET_ADDRSTRLEN]; // 存储点分十进制字符串
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Client connected: %s\n", client_ip);

        // 创建一个新线程处理客户端请求
        // 定义一个 pthread_t 类型的变量 thread_id，用于存储新线程的 ID。
        pthread_t thread_id;
        // 创建一个新线程，执行 handle_client 函数，传递 client_socket 作为参数。
        // &thread_id：存储新线程的 ID。
        // NULL：使用默认线程属性。
        // handle_client：线程函数，用于处理客户端请求。
        // client_socket：传递给线程函数的参数，表示客户端套接字。
        if (pthread_create(&thread_id, NULL, handle_client, client_socket) != 0) {
            perror("Failed to create thread");
            close(*client_socket);
            free(client_socket);
        } else {
            // 分离后的线程在结束时会自动释放资源，无需调用 pthread_join()。
            // 适用于不需要获取线程返回值的场景。
            pthread_detach(thread_id); // 分离线程，使其独立运行
        }
    }

    // 关闭服务器套接字（通常不会执行到这里）
    close(server_socket);
    return 0;
}