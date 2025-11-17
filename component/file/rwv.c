#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>  // readv/writev 头文件
#include <fcntl.h>    // 文件控制选项
#include <unistd.h>   // POSIX API

int main() {
    // ==================== writev 示例 ====================
    // 场景：将多个不连续缓冲区的数据一次性写入文件
    
    int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open failed");
        return EXIT_FAILURE;
    }

    // 准备三个不同的数据缓冲区
    char *header = "HEADER: writev demo\n";
    char *body = "This is the body content...\n";
    char *footer = "FOOTER: end of file\n";

    // 构造iovec结构数组
    struct iovec iov[3];
    iov[0].iov_base = header;
    iov[0].iov_len = strlen(header);
    iov[1].iov_base = body;
    iov[1].iov_len = strlen(body);
    iov[2].iov_base = footer;
    iov[2].iov_len = strlen(footer);

    // 一次性写入所有缓冲区数据
    ssize_t nwritten = writev(fd, iov, 3);
    if (nwritten == -1) {
        perror("writev failed");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("1. writev: 总共写入 %zd 字节到文件\n", nwritten);
    close(fd);

    // ==================== readv 示例 ====================
    // 场景：从文件读取数据分散存储到多个缓冲区
    
    fd = open("output.txt", O_RDONLY);
    if (fd == -1) {
        perror("open failed");
        return EXIT_FAILURE;
    }

    // 准备三个接收缓冲区
    char header_buf[50] = {0};
    char body_buf[100] = {0};
    char footer_buf[50] = {0};

    // 构造iovec结构数组
    struct iovec riov[3];
    riov[0].iov_base = header_buf;
    riov[0].iov_len = sizeof(header_buf) - 1;  // 保留空间给null终止符
    riov[1].iov_base = body_buf;
    riov[1].iov_len = sizeof(body_buf) - 1;
    riov[2].iov_base = footer_buf;
    riov[2].iov_len = sizeof(footer_buf) - 1;

    // 一次性读取数据到多个缓冲区
    ssize_t nread = readv(fd, riov, 3);
    if (nread == -1) {
        perror("readv failed");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("\n2. readv: 总共读取 %zd 字节\n", nread);

    // 确保字符串以null结尾
    header_buf[riov[0].iov_len] = '\0';
    body_buf[riov[1].iov_len] = '\0';
    footer_buf[riov[2].iov_len] = '\0';

    printf("Header: %s", header_buf);
    printf("Body: %s", body_buf);
    printf("Footer: %s", footer_buf);
    close(fd);

    // ==================== 网络编程示例 ====================
    // 场景：HTTP响应头的组装和发送
    
    printf("\n3. 模拟HTTP响应:\n");
    
    // 模拟HTTP响应各部分
    char *http_header = "HTTP/1.1 200 OK\r\n";
    char *content_type = "Content-Type: text/html\r\n";
    char *content_length = "Content-Length: 25\r\n";
    char *connection = "Connection: close\r\n\r\n";
    char *body_content = "<h1>Hello, world!</h1>";

    // 构造iovec数组
    struct iovec http_iov[5];
    http_iov[0].iov_base = http_header;
    http_iov[0].iov_len = strlen(http_header);
    http_iov[1].iov_base = content_type;
    http_iov[1].iov_len = strlen(content_type);
    http_iov[2].iov_base = content_length;
    http_iov[2].iov_len = strlen(content_length);
    http_iov[3].iov_base = connection;
    http_iov[3].iov_len = strlen(connection);
    http_iov[4].iov_base = body_content;
    http_iov[4].iov_len = strlen(body_content);

    // 模拟socket文件描述符 (实际使用中应为socket fd)
    int sock_fd = STDOUT_FILENO;  // 这里输出到标准输出演示
    
    // 一次性写入所有HTTP响应部分
    ssize_t http_written = writev(sock_fd, http_iov, 5);
    printf("\n总共发送 %zd 字节HTTP响应\n", http_written);

    return EXIT_SUCCESS;
}
