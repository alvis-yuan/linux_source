/**
 * @file epoll_framework.c
 * @brief 基于 epoll 的事件驱动框架实现
 */
#include "pollable.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

/**
 * @brief 事件监控项结构体
 */
typedef struct event_item {
    int fd;                   /**< 文件描述符 */
    event_type_t type;        /**< 事件类型 */
    event_handler_t handler;  /**< 事件处理函数 */
    void *user_data;          /**< 用户数据 */
    struct event_item *next;  /**< 下一个节点 */
} event_item_t;

/**
 * @brief epoll 实例结构体
 */
typedef struct {
    int epoll_fd;             /**< epoll 文件描述符 */
    event_item_t *events;     /**< 事件列表头指针 */
    bool running;             /**< 运行标志 */
} epoll_instance_t;

static epoll_instance_t *epoll_instance = NULL;

/**
 * @brief 创建 epoll 实例
 * @return 成功返回 0，失败返回 -1
 */
int epoll_instance_create(void)
{
    epoll_instance_t *instance;
    int epoll_fd;

    instance = malloc(sizeof(epoll_instance_t));
    if (!instance) {
        LogError("malloc failed");
        return -1;
    }

    memset(instance, 0, sizeof(epoll_instance_t));

    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        LogError("epoll_create1 failed: %s", strerror(errno));
        goto err_free_instance;
    }

    instance->epoll_fd = epoll_fd;
    instance->running = false;
    instance->events = NULL;

    LogInfo("epoll instance created successfully, fd: %d", epoll_fd);
    epoll_instance = instance;
    return 0;

err_free_instance:
    free(instance);
    return -1;
}

/**
 * @brief 销毁 epoll 实例
 * @return 成功返回 0，失败返回 -1
 */
void epoll_instance_destroy(void)
{
    event_item_t *current;
    event_item_t *next;

    if (!epoll_instance) {
        return;
    }

    if (epoll_instance->epoll_fd >= 0) {
        close(epoll_instance->epoll_fd);
    }

    current = epoll_instance->events;
    while (current) {
        next = current->next;
        free(current);
        current = next;
    }

    free(epoll_instance);
    epoll_instance = NULL;
    LogInfo("epoll instance destroyed");
}

/**
 * @brief 查找事件监控项
 * @param instance epoll 实例指针
 * @param fd 文件描述符
 * @return 找到返回事件项指针，未找到返回 NULL
 */
static event_item_t *find_event_item(epoll_instance_t *instance, int fd)
{
    event_item_t *current;

    if (!instance) {
        return NULL;
    }

    current = instance->events;
    while (current) {
        if (current->fd == fd) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/**
 * @brief 添加事件到 epoll 监控
 * @param fd 文件描述符
 * @param type 事件类型
 * @param events 事件标志
 * @param handler 事件处理函数
 * @param user_data 用户数据
 * @return 成功返回 0，失败返回 -1
 */
int epoll_add_event(int fd, event_type_t type,
                   uint32_t events, event_handler_t handler, void *user_data)
{
    event_item_t *new_item;
    struct epoll_event ev;
    int ret;

    if (!epoll_instance || fd < 0 || !handler) {
        LogError("invalid parameters");
        return -1;
    }

    if (find_event_item(epoll_instance, fd)) {
        LogError("fd %d already exists", fd);
        return -1;
    }

    new_item = malloc(sizeof(event_item_t));
    if (!new_item) {
        LogError("malloc failed");
        return -1;
    }

    new_item->fd = fd;
    new_item->type = type;
    new_item->handler = handler;
    new_item->user_data = user_data;
    new_item->next = epoll_instance->events;
    epoll_instance->events = new_item;

    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = fd;

    ret = epoll_ctl(epoll_instance->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    if (ret < 0) {
        LogError("epoll_ctl add failed for fd %d: %s", fd, strerror(errno));
        goto err_remove_item;
    }

    LogInfo("event added successfully, fd: %d, type: %d", fd, type);
    return 0;

err_remove_item:
    epoll_instance->events = new_item->next;
    free(new_item);
    return -1;
}

/**
 * @brief 从 epoll 移除事件监控
 * @param fd 文件描述符
 * @return 成功返回 0，失败返回 -1
 */
int epoll_remove_event(int fd)
{
    event_item_t *current;
    event_item_t *prev;
    int ret;

    if (!epoll_instance) {
        LogError("invalid instance");
        return -1;
    }

    ret = epoll_ctl(epoll_instance->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    if (ret < 0) {
        LogError("epoll_ctl del failed for fd %d: %s", fd, strerror(errno));
        return -1;
    }

    current = epoll_instance->events;
    prev = NULL;

    while (current) {
        if (current->fd == fd) {
            if (prev) {
                prev->next = current->next;
            } else {
                epoll_instance->events = current->next;
            }
            free(current);
            LogInfo("event removed successfully, fd: %d", fd);
            return 0;
        }
        prev = current;
        current = current->next;
    }

    LogError("fd %d not found in event list", fd);
    return -1;
}

/**
 * @brief 修改事件监控标志
 * @param fd 文件描述符
 * @param events 新的事件标志
 * @return 成功返回 0，失败返回 -1
 */
int epoll_modify_event(int fd, uint32_t events)
{
    struct epoll_event ev;
    int ret;
    event_item_t *item;

    if (!epoll_instance) {
        LogError("invalid instance");
        return -1;
    }

    item = find_event_item(epoll_instance, fd);
    if (!item) {
        LogError("fd %d not found", fd);
        return -1;
    }

    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = fd;

    ret = epoll_ctl(epoll_instance->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
    if (ret < 0) {
        LogError("epoll_ctl mod failed for fd %d: %s", fd, strerror(errno));
        return -1;
    }

    LogInfo("event modified successfully, fd: %d, events: 0x%x", fd, events);
    return 0;
}

/**
 * @brief 运行 epoll 事件循环
 */
void epoll_run(void)
{
    struct epoll_event events[MAX_EVENTS];
    event_item_t *item;
    int nfds;
    int i;

    if (!epoll_instance) {
        LogError("invalid instance");
        return;
    }

    epoll_instance->running = true;
    LogInfo("epoll event loop started");

    while (epoll_instance->running) {
        nfds = epoll_wait(epoll_instance->epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            //if (errno == EINTR) {
                //continue;
            //}
            LogError("epoll_wait failed: %s", strerror(errno));
            break;
        }

        for (i = 0; i < nfds; i++) {
            item = find_event_item(epoll_instance, events[i].data.fd);
            if (item && item->handler) {
                item->handler(events[i].data.fd, events[i].events, 
                             item->user_data);
            } else {
                LogError("no handler found for fd %d", events[i].data.fd);
            }
        }
    }

    LogInfo("epoll event loop stopped");
}

/**
 * @brief 停止 epoll 事件循环
 */
void epoll_stop(void)
{
    if (!epoll_instance) {
        return;
    }

    epoll_instance->running = false;
    LogInfo("stopping epoll event loop");
}

/**
 * @brief 恢复 epoll 事件循环
 */
void epoll_resume(void)
{
    if (!epoll_instance) {
        return;
    }

    epoll_instance->running = true;
    LogInfo("resuming epoll event loop");
}