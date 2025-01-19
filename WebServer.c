#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 4096

int server_socket; // 全局变量，用于存储服务器套接字

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
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received < 0) {
        perror("Failed to receive data from client");
        close(client_socket);
        return;
    } else if (bytes_received == 0) {
        printf("Client disconnected\n");
        close(client_socket);
        return;
    }

    // 打印客户端请求
    buffer[bytes_received] = '\0';
    printf("Received request:\n%s\n", buffer);

    // 简单的 HTTP 响应
    const char *response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>Hello, World!</h1></body></html>";

    send(client_socket, response, strlen(response), 0);
    close(client_socket);
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
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Failed to set SO_REUSEADDR");
        close(server_socket);
        return 1;
    }

    // 绑定套接字到端口
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Failed to bind socket");
        close(server_socket);
        return 1;
    }

    // 监听连接
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
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_size);
        if (client_socket < 0) {
            perror("Failed to accept client connection");
            continue;
        }

        printf("Client connected: %s\n", inet_ntoa(client_address.sin_addr));

        // 创建一个新线程处理客户端请求
        handle_client(client_socket);
    }

    // 关闭服务器套接字（通常不会执行到这里）
    close(server_socket);
    return 0;
}