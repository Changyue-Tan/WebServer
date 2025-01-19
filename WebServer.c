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
int request_count = 0; // 全局变量，用于记录请求编号
pthread_mutex_t request_count_mutex = PTHREAD_MUTEX_INITIALIZER; // 互斥锁，用于保护 request_count

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
    printf("Received request:\n%s\n", receive_buffer);

    // 获取请求编号
    pthread_mutex_lock(&request_count_mutex);
    int current_request_number = ++request_count;
    pthread_mutex_unlock(&request_count_mutex);

    // 简单的 HTTP 响应，包含请求编号
    char response[BUFFER_SIZE];
    // snprintf 是 C 标准库中的一个函数，用于将格式化的数据写入字符串缓冲区
    // 阻止缓冲区溢出，确保 response 的大小不超过 BUFFER_SIZE
    // 线程安全
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "\r\n"
             "<html><body><h1>Hello, World!</h1><p>This is request #%d</p></body></html>",
             current_request_number);

    send(client_socket, response, strlen(response), 0);
    close(client_socket);

    return NULL;
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

        printf("Client connected: %s\n", inet_ntoa(client_address.sin_addr));

        // 创建一个新线程处理客户端请求
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, client_socket) != 0) {
            perror("Failed to create thread");
            close(*client_socket);
            free(client_socket);
        } else {
            pthread_detach(thread_id); // 分离线程，使其独立运行
        }
    }

    // 关闭服务器套接字（通常不会执行到这里）
    close(server_socket);
    return 0;
}