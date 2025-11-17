#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <aio.h>        // 异步I/O头文件
#include <fcntl.h>      // 文件控制选项
#include <unistd.h>     // POSIX API
#include <errno.h>      // 错误处理
#include <time.h>       // 计时
#include <signal.h>

#define BUFFER_SIZE 4096

// 异步I/O的优势注释说明：
/*
 * 异步I/O的主要优势：
 * 1. 非阻塞性：I/O操作不会阻塞调用线程，CPU可以继续执行其他任务
 * 2. 高性能：可以同时发起多个I/O操作，充分利用现代存储设备的并行能力
 * 3. 可扩展性：特别适合高并发场景，如网络服务器处理大量连接
 * 4. 精准控制：可以精确控制每个I/O操作的优先级、完成通知方式等
 * 5. 资源高效：相比多线程方案，减少了线程上下文切换的开销
 */

// 异步读取完成回调函数
void aio_read_completion_handler(sigval_t sigval) {
    struct aiocb *req = (struct aiocb *)sigval.sival_ptr;
    
    if (aio_error(req) == 0) {
        ssize_t ret = aio_return(req);
        printf("异步读取完成: %zd字节\n", ret);
        //printf("读取内容: %.*s\n", (int)ret < 50 ? (int)ret : 50, (char *)req->aio_buf);
        printf("读取内容: %.*s\n", (int)ret, (char *)req->aio_buf);
    } else {
        perror("异步读取失败");
    }
    
    free((void *)req->aio_buf);
    free(req);
}

// 异步写入完成回调函数
void aio_write_completion_handler(sigval_t sigval) {
    struct aiocb *req = (struct aiocb *)sigval.sival_ptr;
    
    if (aio_error(req) == 0) {
        ssize_t ret = aio_return(req);
        printf("异步写入完成: %zd字节\n", ret);
    } else {
        perror("异步写入失败");
    }
    
    free((void *)req->aio_buf);
    free(req);
}

int main() {
    const char *filename = "testfile.txt";
    int fd;
    
    // 1. 准备测试文件
    fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("创建测试文件失败");
        return EXIT_FAILURE;
    }
    
    // 写入一些测试数据
    const char *test_data = "这是一个测试异步I/O操作的文件内容。"
                           "This is a test file content for async I/O operations.\n";
    write(fd, test_data, strlen(test_data));
    fsync(fd);
    
    // 2. 演示异步写入
    struct aiocb *write_req = malloc(sizeof(struct aiocb));
    memset(write_req, 0, sizeof(struct aiocb));
    
    char *write_buffer = strdup("这是异步写入的额外数据。\nAdditional data written asynchronously.\n");
    
    write_req->aio_fildes = fd;
    write_req->aio_offset = 0;  // 追加写入
    write_req->aio_buf = write_buffer;
    write_req->aio_nbytes = strlen(write_buffer);
    write_req->aio_reqprio = 0;
    write_req->aio_sigevent.sigev_notify = SIGEV_THREAD;
    write_req->aio_sigevent.sigev_notify_function = aio_write_completion_handler;
    write_req->aio_sigevent.sigev_notify_attributes = NULL;
    write_req->aio_sigevent.sigev_value.sival_ptr = write_req;
    
    printf("发起异步写入操作...\n");
    if (aio_write(write_req)) {
        perror("aio_write失败");
        close(fd);
        return EXIT_FAILURE;
    }
    
    // 3. 演示异步读取
    struct aiocb *read_req = malloc(sizeof(struct aiocb));
    memset(read_req, 0, sizeof(struct aiocb));
    
    char *read_buffer = malloc(BUFFER_SIZE);
    
    read_req->aio_fildes = fd;
    read_req->aio_offset = 0;
    read_req->aio_buf = read_buffer;
    read_req->aio_nbytes = BUFFER_SIZE;
    read_req->aio_reqprio = 0;
    read_req->aio_sigevent.sigev_notify = SIGEV_THREAD;
    read_req->aio_sigevent.sigev_notify_function = aio_read_completion_handler;
    read_req->aio_sigevent.sigev_notify_attributes = NULL;
    read_req->aio_sigevent.sigev_value.sival_ptr = read_req;
    
    printf("发起异步读取操作...\n");
    if (aio_read(read_req)) {
        perror("aio_read失败");
        close(fd);
        return EXIT_FAILURE;
    }
    
    // 4. 主线程可以继续做其他工作
    printf("主线程继续执行其他任务...\n");
    for (int i = 0; i < 5; i++) {
        printf("处理其他任务 %d/5\n", i+1);
        sleep(1);
    }
    
    // 5. 等待所有异步操作完成
    printf("等待异步操作完成...\n");
    const struct aiocb *const reqs[] = {write_req, read_req};
    aio_suspend(reqs, 2, NULL);
    
    close(fd);
    return EXIT_SUCCESS;
}
