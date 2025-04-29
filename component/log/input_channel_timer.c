#include "channel.h"
#include <sys/timerfd.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <stddef.h>

// 定义container_of宏
#define container_of(ptr, type, member) ({ \
    const typeof( ((type *)0)->member ) *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type,member) );})

struct timer_priv {
    int fd;
    int interval_sec;  // 定时器间隔(秒)
    const char *name;  // 定时器名称
};

struct timer_channel {
    struct input_channel base;
    struct timer_priv priv;
    struct timer_channel *next;
};

static struct timer_channel *timer_channels = NULL;

static int timer_get_fd(void *priv)
{
	struct timer_channel *chnl = priv;
    return chnl->priv.fd;
}

static int timer_read(void *priv, log_msg_t **msg)
{
	struct timer_channel *chnl = priv;
    struct timer_priv *t = &chnl->priv;
    uint64_t expirations;
    ssize_t len;
    log_msg_t *tmp;

    len = read(t->fd, &expirations, sizeof(expirations));
    if (len != sizeof(expirations)) {
        return -1;
    }

    tmp = malloc(sizeof(log_msg_t) + 128);
    if (!tmp) {
        return -1;
    }

    tmp->pid = getpid();
    tmp->level = LOG_LEVEL_INFO;
    
	LocalDbg("Timer %s triggered (interval: %ds, count: %lu)", 
			t->name, t->interval_sec, expirations);
    snprintf(tmp->data, 128, "Timer %s triggered (interval: %ds, count: %lu)", 
            t->name, t->interval_sec, expirations);

    *msg = tmp;
    return 0;
}

static void timer_destroy(void *priv)
{
	LocalDbg("Destroying timer channel");
	LocalDbg("timer_channels=%p", timer_channels);
	// 根据priv地址获取到定时器通道
	struct timer_channel *chnl = priv;
	if (chnl) {
		if (chnl->priv.fd > 0) {
			close(chnl->priv.fd);
		}
		chnl->base.ops = NULL;
		free(chnl);
		chnl = NULL;
	}
	LocalDbg("timer_channels=%p", timer_channels);
}

static const struct input_channel_ops timer_ops = {
    .get_fd = timer_get_fd,
    .read = timer_read,
    .destroy = timer_destroy,
};

static int setup_timer(int interval_sec)
{
    struct itimerspec new_value;
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (fd < 0) {
        return -1;
    }

    new_value.it_value.tv_sec = interval_sec;
    new_value.it_value.tv_nsec = 0;
    new_value.it_interval.tv_sec = interval_sec;
    new_value.it_interval.tv_nsec = 0;

    if (timerfd_settime(fd, 0, &new_value, NULL) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

struct input_channel *input_channel_timer_create(const char *name, int interval_sec)
{
    struct timer_priv *priv;
    struct timer_channel *channel;
    int fd;

    fd = setup_timer(interval_sec);
    if (fd < 0) {
        return NULL;
    }

    channel = malloc(sizeof(*channel));
    if (!channel) {
        close(fd);
        return NULL;
    }

	priv = &channel->priv;
    priv->fd = fd;
    priv->interval_sec = interval_sec;
    priv->name = name;

    channel->base.ops = &timer_ops;
    channel->base.priv = channel;
    channel->base.next = NULL;
    channel->base.name = name;

    // 添加到定时器通道链表
    channel->next = timer_channels;
    timer_channels = channel;

	LocalDbg("%s timer created", name);

    return &channel->base;
}

static __attribute__((constructor)) void init(void)
{
    // 默认创建一个1分钟的定时器
	struct input_channel *timer = input_channel_timer_create("20s_timer", 20);
    if (timer) {
        input_channel_add(timer);
    }
#if 1
	timer = input_channel_timer_create("10s_timer", 10);
    if (timer) {
        input_channel_add(timer);
    }
#endif		
}

#if 0
static __attribute__((destructor)) void cleanup(void)
{
	// 销毁所有定时器通道
	struct timer_channel *tmp;
	while (timer_channels) {
		tmp = timer_channels;
		timer_channels = tmp->next;
		free(tmp);
		tmp = NULL;
	}
}
#endif
