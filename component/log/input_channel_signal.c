#include "channel.h"
#include <sys/signalfd.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static struct input_channel *signal_channel = NULL;

struct signal_priv {
    int fd;
};

static int signal_get_fd(void *priv)
{
    struct signal_priv *s = priv;
    return s->fd;
}

static int signal_read(void *priv, log_msg_t **msg)
{
    struct signal_priv *s = priv;
    struct signalfd_siginfo fdsi;
    ssize_t len;
    log_msg_t *tmp;

    len = read(s->fd, &fdsi, sizeof(fdsi));
    if (len != sizeof(fdsi)) {
        return -1;
    }

    tmp = malloc(sizeof(log_msg_t) + sizeof(fdsi));
    if (!tmp) {
        return -1;
    }

    // tmp->pid 保存发送者的进程ID
	tmp->pid = fdsi.ssi_pid;
	// tmp->level 保存信号的类型
    tmp->level = fdsi.ssi_signo;
    
	LocalDbg("Received signal %d from PID %d", fdsi.ssi_signo, fdsi.ssi_pid);

	memcpy(tmp->data, &fdsi, sizeof(fdsi));

    *msg = tmp;

	if (fdsi.ssi_signo == SIGINT || fdsi.ssi_signo == SIGTERM) {
		// 处理SIGINT或SIGTERM信号
		LocalDbg("Received termination signal, exiting...");
		return -2; // 返回-2表示需要退出
	}

    return 0;
}

static void signal_destroy(void *priv)
{
    struct signal_priv *s = priv;
    if (s) {
        if (s->fd >= 0) {
            close(s->fd);
        }
        free(s);
    }

	if (signal_channel) {
		if (signal_channel->name) {
			free(signal_channel->name);
		}
		free(signal_channel);
		signal_channel = NULL;
	}
}

static const struct input_channel_ops signal_ops = {
    .get_fd = signal_get_fd,
    .read = signal_read,
    .destroy = signal_destroy,
};

static struct input_channel *input_channel_signal_create(void)
{
    struct signal_priv *priv;
    struct input_channel *channel;
    sigset_t mask;
    int fd;

    // 设置要监听的信号
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    // 阻塞这些信号，防止它们被默认处理
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        return NULL;
    }

    // 创建signalfd
    fd = signalfd(-1, &mask, SFD_NONBLOCK);
    if (fd < 0) {
        return NULL;
    }

    priv = malloc(sizeof(*priv));
    if (!priv) {
        close(fd);
        return NULL;
    }

    priv->fd = fd;

    channel = malloc(sizeof(*channel));
    if (!channel) {
        free(priv);
        close(fd);
        return NULL;
    }

    channel->ops = &signal_ops;
    channel->priv = priv;
    channel->next = NULL;
    channel->name = strdup("signal");

    return channel;
}

static __attribute__((constructor)) void init(void)
{
    signal_channel = input_channel_signal_create();
    if (signal_channel) {
        input_channel_add(signal_channel);
    }
}

static __attribute__((destructor)) void cleanup(void)
{
	if (signal_channel) {
		input_channel_remove(signal_channel);
		signal_channel = NULL;
	}
}
