#include "iAP2LinkRunLoop.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

//#include "iAP2LinkRunLoop.h"

/* 定义存储在 otherData 中的私有上下文结构 */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    BOOL            isSignaled; /* 用于处理虚假唤醒和信号状态 */
    void*           pendingPacket; /* 待处理的数据包 */
} LinuxRLContext_t;

/*
*****************************************************************
**  iAP2LinkRunLoopInitImplementation
**  初始化：分配内存，初始化递归锁和条件变量
*****************************************************************
*/
void iAP2LinkRunLoopInitImplementation(iAP2LinkRunLoop_t* linkRunLoop)
{
    if (!linkRunLoop) return;

    LinuxRLContext_t* ctx = (LinuxRLContext_t*)malloc(sizeof(LinuxRLContext_t));
    if (!ctx) {
        fprintf(stderr, "iAP2LinkRunLoop: Failed to allocate linux context\n");
        return;
    }

    /* 初始化互斥锁属性为递归锁 */
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    if (pthread_mutex_init(&ctx->mutex, &attr) != 0) {
        fprintf(stderr, "iAP2LinkRunLoop: Mutex init failed\n");
        free(ctx);
        pthread_mutexattr_destroy(&attr);
        return;
    }

    pthread_mutexattr_destroy(&attr);

    /* 初始化条件变量 */
    if (pthread_cond_init(&ctx->cond, NULL) != 0) {
        fprintf(stderr, "iAP2LinkRunLoop: Cond init failed\n");
        pthread_mutex_destroy(&ctx->mutex);
        free(ctx);
        return;
    }

    ctx->isSignaled = FALSE;
    ctx->pendingPacket = NULL;
    
    /* 将私有上下文挂载到 otherData */
    linkRunLoop->otherData = ctx;
}

/*
*****************************************************************
**  iAP2LinkRunLoopCleanupImplementation
**  清理：销毁锁和条件变量，释放内存
*****************************************************************
*/
void iAP2LinkRunLoopCleanupImplementation(iAP2LinkRunLoop_t* linkRunLoop)
{
    if (linkRunLoop && linkRunLoop->otherData) {
        LinuxRLContext_t* ctx = (LinuxRLContext_t*)linkRunLoop->otherData;

        pthread_mutex_destroy(&ctx->mutex);
        pthread_cond_destroy(&ctx->cond);
        
        free(ctx);
        linkRunLoop->otherData = NULL;
    }
}

/*
*****************************************************************
**  iAP2LinkRunLoopWait
**  等待：阻塞线程直到收到 Signal
*****************************************************************
*/
BOOL iAP2LinkRunLoopWait(iAP2LinkRunLoop_t* linkRunLoop)
{
    if (!linkRunLoop || !linkRunLoop->otherData) return FALSE;

    LinuxRLContext_t* ctx = (LinuxRLContext_t*)linkRunLoop->otherData;

    pthread_mutex_lock(&ctx->mutex);

    /* 
    ** 使用 while 循环检查谓词，防止虚假唤醒 (Spurious Wakeup)。
    ** 只有当 isSignaled 为 TRUE 或者正在关闭时才跳出等待。
    */
    while (!ctx->isSignaled && !linkRunLoop->shuttingDown) {
        pthread_cond_wait(&ctx->cond, &ctx->mutex);
    }

    /* 复位信号状态，表示信号已被消费 */
    if (ctx->isSignaled) {
        ctx->isSignaled = FALSE;
    }
    
    BOOL result = !linkRunLoop->shuttingDown;

    pthread_mutex_unlock(&ctx->mutex);

    return result;
}

/*
*****************************************************************
**  iAP2LinkRunLoopSignal
**  通知：唤醒等待的线程
*****************************************************************
*/
void iAP2LinkRunLoopSignal(iAP2LinkRunLoop_t* linkRunLoop, void* arg)
{
    if (!linkRunLoop || !linkRunLoop->otherData) return;

    LinuxRLContext_t* ctx = (LinuxRLContext_t*)linkRunLoop->otherData;

    pthread_mutex_lock(&ctx->mutex);

    /* 保存待处理的数据包 */
    if (arg != NULL) {
        ctx->pendingPacket = arg;
    }
    
    /* 设置谓词状态 */
    ctx->isSignaled = TRUE;
    
    /* 发送信号唤醒 Wait */
    pthread_cond_signal(&ctx->cond);

    pthread_mutex_unlock(&ctx->mutex);
}

/*
*****************************************************************
**  iAP2LinkRunLoopProtectedCall
**  受保护调用：加锁执行函数
*****************************************************************
*/
BOOL iAP2LinkRunLoopProtectedCall(iAP2LinkRunLoop_t* linkRunLoop,
                                  void* arg,
                                  BOOL (*func)(iAP2LinkRunLoop_t* linkRunLoop, void* arg))
{
    if (!linkRunLoop || !linkRunLoop->otherData || !func) return FALSE;

    LinuxRLContext_t* ctx = (LinuxRLContext_t*)linkRunLoop->otherData;
    BOOL result = FALSE;

    /* 加锁 - 因为是递归锁，所以同一个线程可以多次进入这里 */
    pthread_mutex_lock(&ctx->mutex);

    result = func(linkRunLoop, arg);

    pthread_mutex_unlock(&ctx->mutex);

    return result;
}

/*
*****************************************************************
**  iAP2LinkRunLoopSetEventMaskBit
**  原子操作：设置 EventMask
*****************************************************************
*/
void iAP2LinkRunLoopSetEventMaskBit(iAP2LinkRunLoop_t* linkRunLoop,
                                    iAP2LinkRunLoopEventMask_t bit)
{
    if (!linkRunLoop || !linkRunLoop->otherData) return;

    LinuxRLContext_t* ctx = (LinuxRLContext_t*)linkRunLoop->otherData;

    pthread_mutex_lock(&ctx->mutex);
    
    linkRunLoop->eventMask |= bit;
    
    pthread_mutex_unlock(&ctx->mutex);
}

/*
*****************************************************************
**  iAP2LinkRunLoopGetResetEventMask
**  原子操作：获取并清除 EventMask
*****************************************************************
*/
uint32_t iAP2LinkRunLoopGetResetEventMask(iAP2LinkRunLoop_t* linkRunLoop)
{
    if (!linkRunLoop || !linkRunLoop->otherData) return 0;

    LinuxRLContext_t* ctx = (LinuxRLContext_t*)linkRunLoop->otherData;
    uint32_t currentMask = 0;

    pthread_mutex_lock(&ctx->mutex);
    
    currentMask = linkRunLoop->eventMask;
    /* 获取后立即清除，标准的 Read-Modify-Write 模式 */
    linkRunLoop->eventMask = kiAP2LinkRunLoopEventMaskNone;
    
    pthread_mutex_unlock(&ctx->mutex);

    return currentMask;
}

/*
*****************************************************************
**  iAP2LinkRunLoopGetPendingPacket
**  原子操作：获取并清除待处理的数据包
*****************************************************************
*/
void* iAP2LinkRunLoopGetPendingPacket(iAP2LinkRunLoop_t* linkRunLoop)
{
    if (!linkRunLoop || !linkRunLoop->otherData) return NULL;

    LinuxRLContext_t* ctx = (LinuxRLContext_t*)linkRunLoop->otherData;
    void* packet = NULL;

    pthread_mutex_lock(&ctx->mutex);
    
    packet = ctx->pendingPacket;
    ctx->pendingPacket = NULL;  /* 清除，避免重复处理 */
    
    pthread_mutex_unlock(&ctx->mutex);

    return packet;
}