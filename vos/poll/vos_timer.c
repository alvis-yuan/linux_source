#include <sys/timerfd.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "vos_timer.h"
#include "pollable.h"

/**
 * @brief 定时器结构体
 */
typedef struct _timer {
    int fd;
    uint32_t period;               /**< 定时器运行周期，单位毫秒 */
    timer_cb_t timer_cb;           /**< 定时器回调函数 */
    void *user_data;               /**< 用户自定义数据 */
    int32_t repeat_count;          /**< 1: 单次; -1: 无限次; n>0: 剩余次数 */
    uint32_t expire_count;         /**< 到期次数 */
    uint32_t paused:1;             /**< 暂停标志 */
    uint32_t auto_delete:1;        /**< 自动删除标志 */
    bool polled;                   /**< 定时器已加入epoll */
} vos_timer_t;

/**
 * @brief 创建定时器
 * @param timer_cb 定时器回调函数
 * @param period 定时器周期，单位毫秒
 * @param user_data 用户自定义数据
 * @return 成功返回定时器指针，失败返回NULL
 */
vos_timer_t *vos_timer_create(timer_cb_t timer_cb, uint32_t period, void *user_data)
{
    vos_timer_t *timer = NULL;
    int fd = -1;
    int ret;

    timer = malloc(sizeof(*timer));
    if (!timer) {
        LogError("malloc failed");
        goto err_out;
    }

    memset(timer, 0, sizeof(*timer));

    fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0) {
        LogError("timerfd_create failed: %s", strerror(errno));
        goto err_free;
    }

    timer->fd = fd;
    timer->timer_cb = timer_cb;
    timer->period = period;
    timer->user_data = user_data;
    timer->repeat_count = -1;
    timer->paused = 1;
    timer->auto_delete = 1;
    timer->polled = false;

    return timer;

err_free:
    free(timer);
err_out:
    return NULL;
}

/**
 * @brief 删除定时器
 * @param timer 定时器指针
 */
void vos_timer_delete(vos_timer_t * timer)
{
    if (!timer) {
        LogError("timer is NULL");
        return;
    }

    if (timer->polled) {
        epoll_remove_event(timer->fd);
        timer->polled = false;
    }

    // 如果定时器正在运行，先暂停它
    if (!timer->paused) {
        vos_timer_pause(timer);
    }    

    close(timer->fd);
    free(timer);
}

/**
 * @brief 暂停定时器
 * @param timer 定时器指针
 */
void vos_timer_pause(vos_timer_t * timer)
{
    struct itimerspec its = { 0 };

    if (!timer) {
        LogError("timer is NULL");
        return;
    }

    if (timer->paused) {
        LogInfo("timer already paused");
        return;
    }

    if (timerfd_settime(timer->fd, 0, &its, NULL) < 0) {
        LogError("timerfd_settime failed: %s", strerror(errno));
        return;
    }

    timer->paused = 1;
    LogInfo("timer paused");
}

/**
 * @brief 处理epoll事件中的定时器触发
 * @param fd 定时器文件描述符
 * @param events 事件类型
 * @param user_data 用户数据
 */
static void _timer_handle_event(int fd, uint32_t events, void *user_data)
{
    uint64_t expirations;
    ssize_t s;

    vos_timer_t *timer = (vos_timer_t *)user_data;
    if (!timer) {
        LogError("timer is NULL");
        return;
    }

    // 如果定时器正在暂停状态
    if (timer->paused) {
        LogInfo("WANRING: timer is paused");
    }

    s = read(timer->fd, &expirations, sizeof(expirations));
    if (s != sizeof(expirations)) {
        LogError("read timer fd failed: %s", strerror(errno));
        return;
    }

    if (expirations == 0) {
        LogInfo("no expiration occurred");
        return;
    }

    if (expirations > 1) {
        LogInfo("WARNING: timer overrun %lu times", expirations - 1);
    }

    LogInfo("timer expired %lu times", expirations);

    timer->expire_count += expirations;

    if (timer->timer_cb) {
        timer->timer_cb(timer);
    }

    if (timer->repeat_count > 0) {
        timer->repeat_count -= expirations;
        if (timer->repeat_count <= 0) {
            LogInfo("timer reached repeat count limit");
            if (timer->auto_delete) {
                // 自动删除定时器
                vos_timer_delete(timer);
            }
        }
    }

    return;
}

/**
 * @brief 恢复定时器
 * @param timer 定时器指针
 */
void vos_timer_resume(vos_timer_t * timer)
{
    struct itimerspec its = { 0 };
    uint64_t ns;

    if (!timer) {
        LogError("timer is NULL");
        return;
    }

    if (!timer->paused) {
        LogInfo("timer not paused");
        return;
    }

    ns = (uint64_t) timer->period * 1000000;
    its.it_value.tv_sec = ns / 1000000000;
    its.it_value.tv_nsec = ns % 1000000000;
    its.it_interval = its.it_value;

    if (timerfd_settime(timer->fd, 0, &its, NULL) < 0) {
        LogError("timerfd_settime failed: %s", strerror(errno));
        return;
    }
    timer->paused = 0;

    LogInfo("timer resumed");
}

/**
 * @brief 启动定时器
 * @param timer 定时器指针
 */
void vos_timer_start(vos_timer_t *timer)
{
    if (!timer) {
        LogError("timer is NULL");
        return;
    }

    if (timer->fd < 0) {
        LogError("timer fd is invalid");
        return;
    }

    if (timer->polled) {
        LogInfo("timer already polled");
        return;
    }

    if (epoll_add_event(timer->fd, EVENT_TYPE_TIMER, EPOLLIN, _timer_handle_event, timer) < 0) {
        LogError("epoll_add_timer failed");
        return;
    }
    timer->polled = true;

    vos_timer_resume(timer);
}

/**
 * @brief 设置定时器回调函数
 * @param timer 定时器指针
 * @param timer_cb 回调函数
 */
void vos_timer_set_cb(vos_timer_t * timer, timer_cb_t timer_cb)
{
    if (!timer) {
        LogError("timer is NULL");
        return;
    }

    // 如果定时器正在运行，先暂停它
    if (!timer->paused) {
        vos_timer_pause(timer);
    }

    timer->timer_cb = timer_cb;
}

/**
 * @brief 设置定时器周期
 * @param timer 定时器指针
 * @param period 周期，单位毫秒
 */
void vos_timer_set_period(vos_timer_t * timer, uint32_t period)
{
    if (!timer) {
        LogError("timer is NULL");
        return;
    }

    if (!timer->paused) {
        vos_timer_pause(timer);
    }

    timer->period = period;
}

/**
 * @brief 设置定时器重复次数
 * @param timer 定时器指针
 * @param repeat_count 重复次数
 */
void vos_timer_set_repeat_count(vos_timer_t * timer, int32_t repeat_count)
{
    if (!timer) {
        LogError("timer is NULL");
        return;
    }

    if (!timer->paused) {
        vos_timer_pause(timer);
    }

    timer->repeat_count = repeat_count;
}

/**
 * @brief 设置定时器自动删除标志
 * @param timer 定时器指针
 * @param auto_delete 自动删除标志
 */
void vos_timer_set_auto_delete(vos_timer_t * timer, bool auto_delete)
{
    if (!timer) {
        LogError("timer is NULL");
        return;
    }

    if (!timer->paused) {
        vos_timer_pause(timer);
    }

    timer->auto_delete = auto_delete;
}

/**
 * @brief 设置定时器用户数据
 * @param timer 定时器指针
 * @param user_data 用户数据
 */
void vos_timer_set_user_data(vos_timer_t * timer, void *user_data)
{
    if (!timer) {
        LogError("timer is NULL");
        return;
    }

    if (!timer->paused) {
        vos_timer_pause(timer);
    }

    timer->user_data = user_data;
}

void vos_timer_set_polled(vos_timer_t * timer, bool polled)
{
    if (!timer) {
        LogError("timer is NULL");
        return;
    }

    if (!timer->paused) {
        LogInfo("timer not paused");
        return;
    }

    timer->polled = polled;
}

/**
 * @brief 获取定时器暂停状态
 * @param timer 定时器指针
 * @return 暂停状态
 */
bool vos_timer_get_paused(vos_timer_t * timer)
{
    if (!timer) {
        LogError("timer is NULL");
        return false;
    }

    return timer->paused;
}

/**
 * @brief 获取定时器用户数据
 * @param timer 定时器指针
 * @return 用户数据
 */
void *vos_timer_get_user_data(vos_timer_t * timer)
{
    if (!timer) {
        LogError("timer is NULL");
        return NULL;
    }

    return timer->user_data;
}

