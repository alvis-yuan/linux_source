#include "libipc.h"
#include "mq_protocol.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>


/* 处理接收到的结构体消息 */
int on_recv_status(const void *arg, size_t arg_len, void *ret_buf, size_t max_ret_len)
{
    /* 1. 校验参数长度 */
    if (arg_len != sizeof(struct sys_status_msg)) {
        return -EINVAL;
    }

    /* 2. 转换结构体 */
    const struct sys_status_msg *msg = (const struct sys_status_msg *)arg;

    /* 3. 处理业务逻辑 */
    printf("[Server] 收到状态报告:\n");
    printf("  CPU: %d%%\n", msg->cpu_usage);
    printf("  MEM: %d%%\n", msg->mem_usage);
    printf("  ERR: %s\n", msg->last_error);

    /* 4. 返回 0，表示成功但不携带数据 */
    return 0;
}

int main(int argc, char **argv) {
    myipc_ctx_t *ctx = myipc_init();
    
    myipc_register_service(ctx, "monitor.service");
    myipc_add_method(ctx, "report_status", on_recv_status);

    printf("Server started. Waiting for messages...\n");
    
    while(1) sleep(1); // 阻塞主线程
    
    myipc_cleanup(ctx);
    return 0;
}