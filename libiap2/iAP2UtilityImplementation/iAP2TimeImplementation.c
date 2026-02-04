#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "iAP2TimeImplementation.h"
#include "iAP2Time.h"

#define MAX_EPOLL_EVENTS 1

/*
** 定义 Linux 平台特定的上下文结构
*/
typedef struct {
    int                 timerFd;      /* timerfd 文件描述符 */
    int                 epollFd;      /* epoll 文件描述符 */
    pthread_t           threadId;     /* 监听线程 ID */
    volatile bool       keepRunning;  /* 线程运行控制标志 */
    volatile bool       isInitialized;/* 初始化标志 */

    iAP2TimeCB_t
    pendingCB;    /* 保存中间层传入的回调函数指针 */
    iAP2Timer_t        *parent;       /* 反向指针 */
} LinuxTimerContext_t;

/*
** 线程工作函数
** 在单独的线程中运行，阻塞在 epoll_wait 上
*/
static void *_LinuxTimerThreadFunc(void *arg)
{
    LinuxTimerContext_t *ctx = (LinuxTimerContext_t *)arg;
    struct epoll_event events[MAX_EPOLL_EVENTS];

    while (ctx->keepRunning) {
        /*
        ** 等待事件
        ** 设置 timeout 为 500ms，以便在 keepRunning 变 false 后能有机会退出循环
        */
        int nfds = epoll_wait(ctx->epollFd, events, MAX_EPOLL_EVENTS, 500);

        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("epoll_wait failed");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == ctx->timerFd) {
                uint64_t exp;
                /* 必须读取以清除 timerfd 的状态 */
                ssize_t s = read(ctx->timerFd, &exp, sizeof(uint64_t));

                if (s == sizeof(uint64_t)) {
                    /*
                    ** 重点：在这里调用回调！
                    ** 当前处于后台线程上下文。
                    ** 回调函数内部会执行 lock -> signal -> unlock 操作通知主循环。
                    */
                    if (ctx->pendingCB && ctx->parent) {
                        uint32_t curTime = iAP2TimeGetCurTimeMs();
                        ctx->pendingCB(ctx->parent, curTime);
                    }
                }
            }
        }
    }

    return NULL;
}

/*
** 初始化资源 (Lazy Init)
*/
static LinuxTimerContext_t *_GetOrInitContext(iAP2Timer_t *timer)
{
    LinuxTimerContext_t *ctx = (LinuxTimerContext_t *)timer->context3;

    if (ctx == NULL) {
        ctx = (LinuxTimerContext_t *)malloc(sizeof(LinuxTimerContext_t));

        if (!ctx) {
            return NULL;
        }

        memset(ctx, 0, sizeof(LinuxTimerContext_t));
        ctx->parent = timer;
        ctx->keepRunning = true;
        /* 1. 创建 timerfd (单调时钟，非阻塞) */
        ctx->timerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

        if (ctx->timerFd == -1) {
            goto error;
        }

        /* 2. 创建 epoll */
        ctx->epollFd = epoll_create1(EPOLL_CLOEXEC);

        if (ctx->epollFd == -1) {
            goto error;
        }

        /* 3. 将 timerfd 加入 epoll */
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = ctx->timerFd;

        if (epoll_ctl(ctx->epollFd, EPOLL_CTL_ADD, ctx->timerFd, &ev) == -1) {
            goto error;
        }

        /* 4. 创建并启动后台线程 */
        if (pthread_create(&ctx->threadId, NULL, _LinuxTimerThreadFunc, ctx) != 0) {
            perror("pthread_create failed");
            goto error;
        }

        ctx->isInitialized = true;
        timer->context3 = ctx;
    }

    return ctx;
error:

    if (ctx->timerFd != -1) {
        close(ctx->timerFd);
    }

    if (ctx->epollFd != -1) {
        close(ctx->epollFd);
    }

    free(ctx);
    return NULL;
}

/*
** 实现 _iAP2TimeCallbackAfter
** 动作：设置内核定时器，立即返回
*/
BOOL _iAP2TimeCallbackAfter(iAP2Timer_t *timer,
                            uint32_t     delayMs,
                            iAP2TimeCB_t callback)
{
    LinuxTimerContext_t *ctx = _GetOrInitContext(timer);

    if (!ctx) {
        return FALSE;
    }

    struct itimerspec new_value;

    /* 保存回调，供后台线程使用 */
    ctx->pendingCB = callback;
    new_value.it_value.tv_sec = delayMs / 1000;
    new_value.it_value.tv_nsec = (delayMs % 1000) * 1000000;

    if (new_value.it_value.tv_sec == 0 && new_value.it_value.tv_nsec == 0) {
        new_value.it_value.tv_nsec = 1;
    }

    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;

    /* 线程安全注记：timerfd_settime 是系统调用，多线程安全 */
    if (timerfd_settime(ctx->timerFd, 0, &new_value, NULL) == -1) {
        perror("timerfd_settime failed");
        return FALSE;
    }

    return TRUE;
}

/*
** 实现 _iAP2TimeCancelCallback
*/
void _iAP2TimeCancelCallback(iAP2Timer_t *timer)
{
    LinuxTimerContext_t *ctx = (LinuxTimerContext_t *)timer->context3;

    if (ctx && ctx->isInitialized) {
        struct itimerspec new_value;
        memset(&new_value, 0, sizeof(new_value));
        /* 设置为 0 停止定时器 */
        timerfd_settime(ctx->timerFd, 0, &new_value, NULL);
    }
}

/*
** 实现 _iAP2TimeCleanupCallback
** 动作：停止线程，回收资源
*/
void _iAP2TimeCleanupCallback(iAP2Timer_t *timer)
{
    LinuxTimerContext_t *ctx = (LinuxTimerContext_t *)timer->context3;

    if (ctx) {
        /* 1. 通知线程退出 */
        ctx->keepRunning = false;

        /* 2. 等待线程结束 */
        if (ctx->isInitialized) {
            pthread_join(ctx->threadId, NULL);
            close(ctx->timerFd);
            close(ctx->epollFd);
        }

        free(ctx);
        timer->context3 = NULL;
    }
}

/*
** 补充实现：因为是在线程中回调，所以这里可以直接调用 CallbackAfter 逻辑
** 但实际上这个函数很少被直接用到，除非用于立即触发
*/
void _iAP2TimePerformCallback(iAP2Timer_t *timer,
                              iAP2TimeCB_t callback)
{
    _iAP2TimeCallbackAfter(timer, 0, callback);
}