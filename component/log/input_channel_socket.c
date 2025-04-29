/**
 * @file input_channel_socket.c
 * @brief Unix socket输入通道实现
 */

 #include "channel.h"
 #include <sys/socket.h>
 #include <sys/un.h>
 #include <unistd.h>
 #include <stdlib.h>
 #include <stdio.h>
 #include <string.h>
 #include <errno.h>
#include <fcntl.h>

 static struct input_channel *socket_channel = NULL;
 #define SOCK_LOG_SERVER "/tmp/log_server.sock"
 
 struct socket_priv {
	 int fd;
	 char *path;
 };
 
 static int socket_get_fd(void *priv)
 {
	 struct socket_priv *s = priv;
	 return s->fd;
 }
 
 static int socket_read(void *priv, log_msg_t **msg)
 {
	 struct socket_priv *s = priv;
	 ssize_t len;
	 log_msg_t *tmp;
 
	 tmp = malloc(4096);
	 if (!tmp)
		 return -1;
 
	 len = read(s->fd, tmp, 4096);
	 if (len <= 0) {
		 free(tmp);
		 return -1;
	 }
 
	 *msg = realloc(tmp, len);
	 if (!*msg) {
		 free(tmp);
		 return -1;
	 }

	 return 0;
 }
 
 static void socket_destroy(void *priv)
 {
	 struct socket_priv *s = priv;

	 if (socket_channel) {
		if (socket_channel->name) {
			free(socket_channel->name);
		}
		free(socket_channel);
		socket_channel = NULL;
	}

	 free(s->path);
	 free(s);

	 if (s->fd >= 0)
	 close(s->fd);
 }
 
 static const struct input_channel_ops socket_ops = {
	 .get_fd = socket_get_fd,
	 .read = socket_read,
	 .destroy = socket_destroy,
 };
 
 static struct input_channel *input_channel_socket_create(const char *path)
 {
	 struct socket_priv *priv;
	 struct sockaddr_un addr;
	 struct input_channel *channel;
	 int fd;
 
	 fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	 if (fd < 0) {
		 LocalDbg("socket error(%d),%s", errno, strerror(errno));
		 return NULL;
	 }

	 // 设置fd为非阻塞
	 if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		 close(fd);
		 LocalDbg("fcntl error(%d),%s", errno, strerror(errno));
		 return NULL;
	 }
 
	 memset(&addr, 0, sizeof(addr));
	 addr.sun_family = AF_UNIX;
	 strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
 
	 unlink(path);
	 if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		 close(fd);
		 LocalDbg("bind error(%d),%s", errno, strerror(errno));
		 return NULL;
	 }
 
	 priv = malloc(sizeof(*priv));
	 if (!priv) {
		 close(fd);
		 return NULL;
	 }
 
	 LocalDbg("socket fd=%d", fd);
	 priv->fd = fd;
	 priv->path = strdup(path);
	 if (!priv->path) {
		 free(priv);
		 close(fd);
		 return NULL;
	 }
 
	 channel = malloc(sizeof(*channel));
	 if (!channel) {
		 free(priv->path);
		 free(priv);
		 close(fd);
		 return NULL;
	 }
 
	 channel->ops = &socket_ops;
	 channel->priv = priv;
	 channel->next = NULL;
	 channel->name = strdup("socket");
 
	 LocalDbg("socket channel created");
	 return channel;
 }

 static __attribute__((constructor)) void init(void)
 {
	socket_channel = input_channel_socket_create(SOCK_LOG_SERVER);
	if (!socket_channel) {
		fprintf(stderr, "Failed to create socket channel\n");
		exit(EXIT_FAILURE);
	}
	// 添加到输入通道链表
	input_channel_add(socket_channel);
 }

 #if 0
 static __attribute__((destructor)) void cleanup(void)
 {
	if (socket_channel) {
		LocalDbg("destroy socket channel");
		// 移除输入通道
		input_channel_remove(socket_channel);
		socket_channel = NULL;
	}
 }
#endif