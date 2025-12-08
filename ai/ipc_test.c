#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libipc.h"

/* --- 服务端逻辑 --- */

int on_echo(const void *arg, size_t arg_len, void *ret_buf, size_t max_ret_len)
{
    printf("[Server] Echo request: %.*s\n", (int)arg_len, (char*)arg);
    int len = snprintf((char*)ret_buf, max_ret_len, "ACK: %.*s", (int)arg_len, (char*)arg);
    return len + 1;
}

/* --- 客户端逻辑 --- */

void on_async_done(int status, void *data, size_t len, void *priv)
{
    printf("[Client Async] Status: %d, Data: %s\n", status, (char*)data);
}

void on_event(const char *topic, const void *data, size_t len)
{
    printf("[Event] Topic: %s, Data: %s\n", topic, (char*)data);
}

int main(int argc, char **argv)
{
    myipc_ctx_t *ctx = myipc_init();
    if (!ctx) {
        fprintf(stderr, "Init failed\n");
        return 1;
    }

    if (argc > 1 && strcmp(argv[1], "server") == 0) {
        printf("Starting Server...\n");
        myipc_register_service(ctx, "my.service");
        myipc_add_method(ctx, "echo", on_echo);
        
        while(1) sleep(1); /* 主线程阻塞，IPC 线程在后台运行 */
    } 
    else if (argc > 1 && strcmp(argv[1], "sub") == 0) {
        printf("Starting Subscriber...\n");
        myipc_subscribe(ctx, on_event);
        while(1) sleep(1);
    }
    else if (argc > 1 && strcmp(argv[1], "pub") == 0) {
        myipc_publish(ctx, "sys.alert", "Power Down", 11);
        printf("Published event\n");
        //while(1) sleep(1);
    }
    else {
        /* Client Mode */
        char buf[64];
        
        printf("Starting Client...\n");
        printf("Looking up 'my.service'...\n");
        
        /* 1. 服务发现 */
        int32_t pid = myipc_lookup_service(ctx, "my.service");
        if (pid < 0) {
            fprintf(stderr, "Lookup failed: %s (%d)\n", strerror(-pid), pid);
            myipc_cleanup(ctx);
            return 1;
        }
        printf("Found service at PID: %d\n", pid);
        
        /* 2. 同步调用 */
        int ret = myipc_call_sync(ctx, pid, "echo", "Hello", 5, buf, 64, 1000);
        if (ret >= 0) {
            printf("[Client Sync] Result: %s\n", buf);
        } else {
            printf("[Client Sync] Failed: %d\n", ret);
        }

        /* 3. 异步调用 */
        myipc_call_async(ctx, pid, "echo", "AsyncWorld", 10, on_async_done, NULL);
        
        sleep(2); /* 等待异步回调 */
    }

    myipc_cleanup(ctx);
    return 0;
}