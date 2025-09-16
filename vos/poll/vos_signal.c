#define _GNU_SOURCE
#include <sys/signalfd.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include "vos_signal.h"
#include "pollable.h"

#define SIGNAL_NUM 64

/**
 * @brief 信号处理器结构体
 */
typedef struct _signal {
    int fd;                       /**< signalfd文件描述符 */
    sigset_t sigset;              /**< 信号集 */
    struct {
        signal_cb_t signal_cb;        /**< 信号回调函数 */
        void *user_data;              /**< 用户自定义数据 */
    }cb[SIGNAL_NUM];
    bool polled;                  /**< 信号处理器已加入epoll */
} vos_signal_t;

static vos_signal_t *sig_handler = NULL;

static int my_sigisemptyset(const sigset_t *set) {
    if (set == NULL) {
        return -1; // EINVAL
    }
    
    // 检查整个 sigset_t 的内存是否全为0
    const unsigned char *bytes = (const unsigned char *)set;
    for (size_t i = 0; i < sizeof(sigset_t); i++) {
        if (bytes[i] != 0) {
            return 0; // 不为空
        }
    }
    return 1; // 为空
}

/**
 * @brief 初始化信号处理器
 * @return 成功返回0，失败返回-1
 */
int vos_signal_init(void)
{
    int ret;

    sig_handler = malloc(sizeof(*sig_handler));
    if (!sig_handler) {
        LogError("malloc failed");
        goto err_out;
    }

    memset(sig_handler, 0, sizeof(*sig_handler));

    ret = sigemptyset(&sig_handler->sigset);
    if (ret < 0) {
        LogError("sigemptyset failed: %s", strerror(errno));
        goto err_free;
    }

    sig_handler->fd = -1;
    sig_handler->polled = false;

    return 0;

err_free:
    free(sig_handler);
err_out:
    return -1;
}

/**
 * @brief 销毁信号处理器
 */
void vos_signal_destroy(void)
{
    if (sig_handler) {
        if (sig_handler->fd >= 0) {
            close(sig_handler->fd);
        }

        // 解除之前屏蔽的信号
        sigprocmask(SIG_UNBLOCK, &sig_handler->sigset, NULL);

        free(sig_handler);
        sig_handler = NULL;
    }
}

/**
 * @brief 安装信号处理器
 * @param signo 信号编号
 * @param signal_cb 信号回调函数
 * @param user_data 用户自定义数据
 * @return 成功返回0，失败返回-1
 */
int vos_signal_install(int signo, signal_cb_t signal_cb, void *user_data)
{
    if (!sig_handler) {
        LogError("sig_handler is not init");
        return -1;
    }

    if (signo < 0 || signo >= SIGNAL_NUM || signo == 32 || signo == 33) {
        LogError("signo is invalid: %d", signo);
        return -1;
    }

    sig_handler->cb[signo].signal_cb = signal_cb;
    sig_handler->cb[signo].user_data = user_data;

    // 加入信号集
    if (sigaddset(&sig_handler->sigset, signo) == -1) {
        LogError("sigaddset failed for signal %d: %s", signo, strerror(errno));
        return -1;
    }

    return 0;
}

/**
 * @brief 卸载信号处理器
 * @param signo 信号编号
 * @return 成功返回0，失败返回-1
 */
int vos_signal_uninstall(int signo)
{
    if (!sig_handler) {
        LogError("sig_handler is NULL");
        return -1;
    }

    if (signo < 0 || signo >= SIGNAL_NUM || signo == 32 || signo == 33) {
        LogError("signo is invalid: %d", signo);
        return -1;
    }

    // 从信号集移除
    sigdelset(&sig_handler->sigset, signo);

    // 清空回调函数
    sig_handler->cb[signo].signal_cb = NULL;
    sig_handler->cb[signo].user_data = NULL;

    return 0;
}

/**
 * @brief 处理epoll事件中的信号触发
 */
static void _signal_handle_event(int fd, uint32_t events, void *user_data)
{
    struct signalfd_siginfo info;
    ssize_t s;

    vos_signal_t *sig_handler = (vos_signal_t *)user_data;
    if (!sig_handler) {
        LogError("sig_handler is NULL");
        return;
    }

    s = read(sig_handler->fd, &info, sizeof(info));
    if (s != sizeof(info)) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            LogInfo("no sig_handler data available");
        } else {
            LogError("read sig_handler fd failed: %s", strerror(errno));
        }
        return;
    }

    LogInfo("sig_handler %d received from PID %d", info.ssi_signo, info.ssi_pid);

    if (sig_handler->cb[info.ssi_signo].signal_cb) {
        sig_handler->cb[info.ssi_signo].signal_cb(&info, sig_handler->cb[info.ssi_signo].user_data);
    }
}

/**
 * @brief 启动信号处理器
 * @return 成功返回0，失败返回-1
 */
int vos_signal_start(void)
{
    if (!sig_handler) {
        LogError("sig_handler is NULL");
        return -1;
    }

    if (sig_handler->polled) {
        LogInfo("sig_handler already polled");
        return -1;
    }

    // 如果当前fd已存在，先删除旧的
    if (sig_handler->fd >= 0) {
        LogInfo("sig_handler fd already exists");
    }

    // 检查信号集是否为空
    if (my_sigisemptyset(&sig_handler->sigset)) {
        LogError("sig_handler sigset is empty");
        return -1;
    }

    // 首先阻塞这些信号，防止默认处理
    int ret = sigprocmask(SIG_BLOCK, &sig_handler->sigset, NULL);
    if (ret < 0) {
        LogError("sigprocmask failed: %s", strerror(errno));
        return -1;
    }

    sig_handler->fd = signalfd(-1, &sig_handler->sigset, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sig_handler->fd < 0) {
        LogError("signalfd_create failed: %s", strerror(errno));
        return -1;
    }

    // 此间发生的信号会被 suspending，直到 epoll 事件触发

    if (epoll_add_event(sig_handler->fd, EVENT_TYPE_SIGNAL, EPOLLIN, 
        _signal_handle_event, sig_handler) < 0) {
        LogError("epoll_add_signal failed");
        return -1;
    }

    sig_handler->polled = true;
    LogInfo("sig_handler polled");

    return 0;
}



