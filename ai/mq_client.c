#include "libipc.h"
#include "mq_protocol.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    myipc_ctx_t *ctx = myipc_init();
    
    /* 假设已知服务端 PID (实际应通过 lookup 获取) */
    int32_t pid = myipc_lookup_service(ctx, "monitor.service");
    if (pid < 0) {
        fprintf(stderr, "服务未找到\n");
        return -1;
    }

    /* 1. 填充结构体 */
    struct sys_status_msg msg;
    msg.cpu_usage = 45;
    msg.mem_usage = 60;
    strcpy(msg.last_error, "Network Jitter");

    printf("正在发送结构体消息...\n");

    /* 2. 发送消息 (即发即弃) */
    /* 
     * cb = NULL: 表示我不关心结果，发送完我就不管了
     * priv = NULL: 私有数据
     */
    int ret = myipc_call_async(ctx, pid, "report_status", 
                               &msg, sizeof(msg), 
                               NULL, NULL);

    if (ret == 0) {
        printf("消息已放入发送队列 (非阻塞)\n");
    } else {
        printf("发送失败: %d\n", ret);
    }

    /* 稍微等待一下，防止进程退出太快导致 socket 关闭，消息还没发出去 */
    /* 在实际的长运行进程中不需要 sleep */
    sleep(1); 

    myipc_cleanup(ctx);
    return 0;
}