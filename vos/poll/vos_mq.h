/**
 * @file vos_mq.h
 * @brief POSIX消息队列封装，支持epoll集成
 */
#ifndef VOS_MQ_H
#define VOS_MQ_H

#include <mqueue.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 消息队列回调函数类型
 * @param mq 消息队列指针
 * @param msg 接收到的消息
 * @param msg_len 消息长度
 * @param user_data 用户数据
 */
typedef void (*mq_cb_t)(mqd_t mq, const char *msg, size_t msg_len, void *user_data);

/**
 * @brief 消息队列结构体
 */
typedef struct _vos_mq {
    mqd_t mq;                  /**< 消息队列描述符 */
    char *name;                /**< 消息队列名称 */
    mq_cb_t mq_cb;            /**< 消息回调函数 */
    void *user_data;          /**< 用户自定义数据 */
    struct mq_attr attr;      /**< 消息队列属性 */
    bool polled;               /**< 是否已加入epoll */
} vos_mq_t;

/* 函数声明 */

/**
 * @brief 创建消息队列
 * @param name 消息队列名称（必须以/开头）
 * @param max_msgs 最大消息数
 * @param max_msg_size 最大消息大小
 * @param mq_cb 消息回调函数
 * @param user_data 用户数据
 * @return 成功返回消息队列指针，失败返回NULL
 */
vos_mq_t *vos_mq_create(const char *name, long max_msgs, long max_msg_size, 
                       mq_cb_t mq_cb, void *user_data);

/**
 * @brief 打开现有消息队列
 * @param name 消息队列名称
 * @param flags 打开标志（O_RDONLY, O_WRONLY, O_RDWR）
 * @param mq_cb 消息回调函数
 * @param user_data 用户数据
 * @return 成功返回消息队列指针，失败返回NULL
 */
vos_mq_t *vos_mq_open(const char *name, int flags, mq_cb_t mq_cb, void *user_data);

/**
 * @brief 关闭消息队列
 * @param mq 消息队列指针
 */
void vos_mq_close(vos_mq_t *mq);

/**
 * @brief 删除消息队列
 * @param mq 消息队列指针
 */
void vos_mq_unlink(vos_mq_t *mq);

/**
 * @brief 发送消息
 * @param mq 消息队列指针
 * @param msg 消息内容
 * @param msg_len 消息长度
 * @param prio 消息优先级
 * @return 成功返回0，失败返回-1
 */
int vos_mq_send(vos_mq_t *mq, const char *msg, size_t msg_len, unsigned int prio);

/**
 * @brief 接收消息
 * @param mq 消息队列指针
 * @param buf 接收缓冲区
 * @param buf_size 缓冲区大小
 * @param prio 接收到的消息优先级（输出参数）
 * @return 成功返回接收到的字节数，失败返回-1
 */
ssize_t vos_mq_receive(vos_mq_t *mq, char *buf, size_t buf_size, unsigned int *prio);

/**
 * @brief 获取消息队列属性
 * @param mq 消息队列指针
 * @param attr 属性结构体（输出参数）
 * @return 成功返回0，失败返回-1
 */
int vos_mq_getattr(vos_mq_t *mq, struct mq_attr *attr);

/**
 * @brief 设置消息队列属性
 * @param mq 消息队列指针
 * @param attr 属性结构体
 * @return 成功返回0，失败返回-1
 */
int vos_mq_setattr(vos_mq_t *mq, const struct mq_attr *attr);

/**
 * @brief 启动消息队列监听（加入epoll）
 * @param mq 消息队列指针
 * @return 成功返回0，失败返回-1
 */
int vos_mq_start(vos_mq_t *mq);

/**
 * @brief 停止消息队列监听（从epoll移除）
 * @param mq 消息队列指针
 */
void vos_mq_stop(vos_mq_t *mq);

#endif /* VOS_MQ_H */
