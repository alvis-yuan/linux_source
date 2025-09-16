/**
 * @file epoll_framework.h
 * @brief 基于 epoll 的事件驱动框架头文件
 */
#ifndef EPOLL_FRAMEWORK_H
#define EPOLL_FRAMEWORK_H

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <mqueue.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <stdio.h>

#define LogError(fmt, arg...) fprintf(stderr, "[%s-%d] " fmt "\n", __func__, __LINE__, ##arg)
#define LogInfo(fmt, arg...) fprintf(stderr, "[%s-%d] " fmt "\n", __func__, __LINE__, ##arg)

#define MAX_EVENTS 256

/**
 * @brief 事件类型枚举
 */
typedef enum {
    EVENT_TYPE_UNKNOWN = 0,   /**< 未知事件类型 */
    EVENT_TYPE_SOCKET,        /**< Socket 事件 */
    EVENT_TYPE_TIMER,         /**< 定时器事件 */
    EVENT_TYPE_SIGNAL,        /**< 信号事件 */
    EVENT_TYPE_MQ,            /**< 消息队列事件 */
    EVENT_TYPE_USER           /**< 用户自定义事件 */
} event_type_t;

/**
 * @brief 事件处理函数指针类型
 * @param fd 文件描述符
 * @param events 事件标志
 * @param user_data 用户数据
 */
typedef void (*event_handler_t)(int fd, uint32_t events, void *user_data);

/* 函数声明 */
/**
 * @brief 创建 epoll 实例
 * @return int 成功返回 0，失败返回 -1
 */
int epoll_instance_create(void);

/**
 * @brief 销毁 epoll 实例
 */
void epoll_instance_destroy(void);

/**
 * @brief 添加事件
 * @param instance epoll 实例指针
 * @param fd 文件描述符
 * @param type 事件类型
 * @param events 事件标志
 * @param handler 事件处理函数
 * @param user_data 用户数据
 * @return int 成功返回 0，失败返回 -1
 */
int epoll_add_event(int fd, event_type_t type, 
                   uint32_t events, event_handler_t handler, void *user_data);

/**
 * @brief 删除事件

 * @param fd 文件描述符
 * @return int 成功返回 0，失败返回 -1
 */
int epoll_remove_event(int fd);

/**
 * @brief 修改事件
 * @param fd 文件描述符
 * @param events 事件标志
 * @return int 成功返回 0，失败返回 -1
 */
int epoll_modify_event(int fd, uint32_t events);

/**
 * @brief 运行 epoll 实例
 */
void epoll_run(void);

/**
 * @brief 停止 epoll 实例
 */
void epoll_stop(void);


/**
 * @brief 恢复 epoll 实例
 */
void epoll_resume(void);

#endif /* EPOLL_FRAMEWORK_H */