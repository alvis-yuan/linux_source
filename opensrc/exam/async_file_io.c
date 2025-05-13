#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <uv.h>  
  
#define DEFAULT_FILE "test_file"  
#define TEST_CONTENT "Hello, libuv异步文件I/O测试!"  
  
// 全局变量  
uv_loop_t* loop;  
uv_fs_t open_req;  
uv_fs_t write_req;  
uv_fs_t read_req;  
uv_fs_t close_req;  
uv_buf_t iov;  
char buffer[1024];  
  
// 关闭文件回调  
void on_close(uv_fs_t* req) {  
    if (req->result < 0) {  
        fprintf(stderr, "关闭文件错误: %s\n", uv_strerror((int)req->result));  
    } else {  
        printf("文件已成功关闭\n");  
    }  
    uv_fs_req_cleanup(req);  
}  
  
// 读取文件回调  
void on_read(uv_fs_t* req) {  
    if (req->result < 0) {  
        fprintf(stderr, "读取文件错误: %s\n", uv_strerror((int)req->result));  
    } else if (req->result == 0) {  
        // 文件读取完毕，关闭文件  
        printf("文件读取完毕，没有更多数据\n");  
        uv_fs_close(loop, &close_req, open_req.result, on_close);  
    } else {  
        // 成功读取数据  
        buffer[req->result] = '\0';  
        printf("异步读取的数据: %s\n", buffer);  
          
        // 关闭文件  
        uv_fs_close(loop, &close_req, open_req.result, on_close);  
    }  
    uv_fs_req_cleanup(req);  
}  
  
// 写入文件回调  
void on_write(uv_fs_t* req) {  
    if (req->result < 0) {  
        fprintf(stderr, "写入文件错误: %s\n", uv_strerror((int)req->result));  
    } else {  
        printf("成功写入 %lld 字节数据\n", req->result);  
          
        // 准备读取文件  
        iov = uv_buf_init(buffer, sizeof(buffer));  
        uv_fs_read(loop, &read_req, open_req.result, &iov, 1, 0, on_read);  
    }  
    uv_fs_req_cleanup(req);  
}  
  
// 打开文件回调  
void on_open(uv_fs_t* req) {  
    if (req->result < 0) {  
        fprintf(stderr, "打开文件错误: %s\n", uv_strerror((int)req->result));  
        return;  
    }  
      
    printf("文件已成功打开，文件描述符: %d\n", (int)req->result);  
      
    // 准备写入数据  
    iov = uv_buf_init((char*)TEST_CONTENT, strlen(TEST_CONTENT));  
    uv_fs_write(loop, &write_req, req->result, &iov, 1, 0, on_write);  
      
    uv_fs_req_cleanup(req);  
}  
  
int main() {  
    loop = uv_default_loop();  
      
    // 删除可能存在的旧文件  
    unlink(DEFAULT_FILE);  
      
    // 异步打开文件（创建新文件）  
    uv_fs_open(loop, &open_req, DEFAULT_FILE,   
               UV_FS_O_WRONLY | UV_FS_O_CREAT,   
               S_IRUSR | S_IWUSR,   
               on_open);  
      
    // 运行事件循环  
    printf("开始运行事件循环...\n");  
    uv_run(loop, UV_RUN_DEFAULT);  
      
    // 清理  
    uv_loop_close(loop);  
    printf("程序结束\n");  
      
    return 0;  
}
