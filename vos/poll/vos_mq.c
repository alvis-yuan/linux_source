#include <mqueue.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "vos_mq.h"
#include "pollable.h"

/**
 * @brief 处理epoll事件中的消息队列触发
 */
static void _mq_handle_event(int fd, uint32_t events, void *user_data)
{
    char *buf = NULL;
    ssize_t received;
    unsigned int prio;
    
    vos_mq_t *mq = (vos_mq_t *)user_data;
    if (!mq) {
        LogError("mq is NULL");
        return;
    }

    if (!(events & EPOLLIN)) {
        LogInfo("mq event not EPOLLIN: %u", events);
        return;
    }

    // 分配接收缓冲区
    buf = malloc(mq->attr.mq_msgsize);
    if (!buf) {
        LogError("malloc failed for message buffer");
        return;
    }

    // 接收消息
    received = mq_receive(mq->mq, buf, mq->attr.mq_msgsize, &prio);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            LogInfo("no message available");
        } else {
            LogError("mq_receive failed: %s", strerror(errno));
        }
        free(buf);
        return;
    }

    LogInfo("received message: %.*s (prio: %u)", (int)received, buf, prio);

    // 调用用户回调函数
    if (mq->mq_cb) {
        mq->mq_cb(mq->mq, buf, received, mq->user_data);
    }

    free(buf);
}

/**
 * @brief 创建消息队列
 */
vos_mq_t *vos_mq_create(const char *name, long max_msgs, long max_msg_size, 
                       mq_cb_t mq_cb, void *user_data)
{
    vos_mq_t *mq = NULL;
    mqd_t mq_desc;
    struct mq_attr attr;
    
    if (!name || name[0] != '/') {
        LogError("mq name must start with '/': %s", name);
        return NULL;
    }

    mq = malloc(sizeof(*mq));
    if (!mq) {
        LogError("malloc failed");
        goto err_out;
    }

    memset(mq, 0, sizeof(*mq));

    // 设置消息队列属性
    attr.mq_flags = 0;
    attr.mq_maxmsg = max_msgs;
    attr.mq_msgsize = max_msg_size;
    attr.mq_curmsgs = 0;

    // 创建消息队列（读写权限：0660）
    mq_desc = mq_open(name, O_CREAT | O_RDWR | O_NONBLOCK, 0660, &attr);
    if (mq_desc == (mqd_t)-1) {
        LogError("mq_open failed: %s", strerror(errno));
        goto err_free;
    }

    mq->mq = mq_desc;
    mq->name = strdup(name);
    mq->mq_cb = mq_cb;
    mq->user_data = user_data;
    mq->polled = false;

    // 获取实际属性
    if (mq_getattr(mq->mq, &mq->attr) < 0) {
        LogError("mq_getattr failed: %s", strerror(errno));
        goto err_close;
    }

    return mq;

err_close:
    mq_close(mq->mq);
    mq_unlink(name);
err_free:
    free(mq);
err_out:
    return NULL;
}

/**
 * @brief 打开现有消息队列
 */
vos_mq_t *vos_mq_open(const char *name, int flags, mq_cb_t mq_cb, void *user_data)
{
    vos_mq_t *mq = NULL;
    mqd_t mq_desc;
    
    if (!name || name[0] != '/') {
        LogError("mq name must start with '/': %s", name);
        return NULL;
    }

    mq = malloc(sizeof(*mq));
    if (!mq) {
        LogError("malloc failed");
        goto err_out;
    }

    memset(mq, 0, sizeof(*mq));

    // 打开消息队列
    mq_desc = mq_open(name, flags | O_NONBLOCK);
    if (mq_desc == (mqd_t)-1) {
        LogError("mq_open failed: %s", strerror(errno));
        goto err_free;
    }

    mq->mq = mq_desc;
    mq->name = strdup(name);
    mq->mq_cb = mq_cb;
    mq->user_data = user_data;
    mq->polled = false;

    // 获取属性
    if (mq_getattr(mq->mq, &mq->attr) < 0) {
        LogError("mq_getattr failed: %s", strerror(errno));
        goto err_close;
    }

    return mq;

err_close:
    mq_close(mq->mq);
err_free:
    free(mq);
err_out:
    return NULL;
}

/**
 * @brief 关闭消息队列
 */
void vos_mq_close(vos_mq_t *mq)
{
    if (!mq) {
        LogError("mq is NULL");
        return;
    }

    if (mq->polled) {
        vos_mq_stop(mq);
    }

    if (mq->mq != (mqd_t)-1) {
        mq_close(mq->mq);
        mq->mq = (mqd_t)-1;
    }

    if (mq->name) {
        free(mq->name);
        mq->name = NULL;
    }

    free(mq);
}

/**
 * @brief 删除消息队列
 */
void vos_mq_unlink(vos_mq_t *mq)
{
    if (!mq) {
        LogError("mq is NULL");
        return;
    }

    if (mq->name) {
        mq_unlink(mq->name);
    }
}

/**
 * @brief 发送消息
 */
int vos_mq_send(vos_mq_t *mq, const char *msg, size_t msg_len, unsigned int prio)
{
    if (!mq) {
        LogError("mq is NULL");
        return -1;
    }

    if (msg_len > mq->attr.mq_msgsize) {
        LogError("message too long: %zu > %ld", msg_len, mq->attr.mq_msgsize);
        return -1;
    }

    if (mq_send(mq->mq, msg, msg_len, prio) < 0) {
        LogError("mq_send failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

/**
 * @brief 接收消息
 */
ssize_t vos_mq_receive(vos_mq_t *mq, char *buf, size_t buf_size, unsigned int *prio)
{
    if (!mq) {
        LogError("mq is NULL");
        return -1;
    }

    if (buf_size < mq->attr.mq_msgsize) {
        LogError("buffer too small: %zu < %ld", buf_size, mq->attr.mq_msgsize);
        return -1;
    }

    return mq_receive(mq->mq, buf, buf_size, prio);
}

/**
 * @brief 获取消息队列属性
 */
int vos_mq_getattr(vos_mq_t *mq, struct mq_attr *attr)
{
    if (!mq) {
        LogError("mq is NULL");
        return -1;
    }

    return mq_getattr(mq->mq, attr);
}

/**
 * @brief 设置消息队列属性
 */
int vos_mq_setattr(vos_mq_t *mq, const struct mq_attr *attr)
{
    if (!mq) {
        LogError("mq is NULL");
        return -1;
    }

    return mq_setattr(mq->mq, attr, NULL);
}

/**
 * @brief 启动消息队列监听
 */
int vos_mq_start(vos_mq_t *mq)
{
    if (!mq) {
        LogError("mq is NULL");
        return -1;
    }

    if (mq->polled) {
        LogInfo("mq already polled");
        return -1;
    }

    // 将消息队列描述符添加到epoll
    if (epoll_add_event(mq->mq, EVENT_TYPE_MQ, EPOLLIN, 
                       _mq_handle_event, mq) < 0) {
        LogError("epoll_add_event failed");
        return -1;
    }

    mq->polled = true;
    LogInfo("mq started listening");

    return 0;
}

/**
 * @brief 停止消息队列监听
 */
void vos_mq_stop(vos_mq_t *mq)
{
    if (!mq) {
        LogError("mq is NULL");
        return;
    }

    if (!mq->polled) {
        LogInfo("mq not polled");
        return;
    }

    epoll_remove_event(mq->mq);
    mq->polled = false;
    LogInfo("mq stopped listening");
}
