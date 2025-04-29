/**
 * @file input_channel_kernel.c
 * @brief 内核日志输入通道实现
 */

 #include "channel.h"
 #include <fcntl.h>
 #include <unistd.h>
 #include <stdlib.h>
 #include <string.h>

 static struct input_channel *kernel_channel = NULL;

 struct kernel_priv {
	 int fd;
 };
 
 static int kernel_get_fd(void *priv)
 {
	 struct kernel_priv *k = priv;
	 return k->fd;
 }
 
 static int kernel_read(void *priv, log_msg_t **msg)
 {
	 struct kernel_priv *k = priv;
	 ssize_t len;
	 char buf[4096];
	 log_msg_t *tmp;
 
	 len = read(k->fd, buf, sizeof(buf) - 1);
	 if (len <= 0)
		 return -1;
 
	 buf[len] = '\0';
 
	 tmp = malloc(sizeof(log_msg_t) + len + 1);
	 if (!tmp)
		 return -1;
 
	 tmp->pid = 0; // 内核日志pid为0
	 tmp->level = LOG_LEVEL_INFO; // 默认级别，实际应该解析内核日志级别
	 memcpy(tmp->data, buf, len + 1);
 
	 *msg = tmp;
	 return 0;
 }
 
 static void kernel_destroy(void *priv)
 {
	 struct kernel_priv *k = priv;
	 if (k->fd >= 0)
		 close(k->fd);
	 free(k);
 }
 
 static const struct input_channel_ops kernel_ops = {
	 .get_fd = kernel_get_fd,
	 .read = kernel_read,
	 .destroy = kernel_destroy,
 };
 
 static struct input_channel *input_channel_kernel_create(void)
 {
	 struct kernel_priv *priv;
	 struct input_channel *channel;
	 int fd;
 
	 fd = open("/dev/kmsg", O_RDONLY);
	 if (fd < 0)
		 return NULL;
 
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
 
	 channel->ops = &kernel_ops;
	 channel->priv = priv;
	 channel->next = NULL;
 
	 return channel;
 }

 static __attribute__((constructor)) void init(void)
 {
	 // 创建内核日志输入通道
	 kernel_channel = input_channel_kernel_create();
	 if (kernel_channel) {
		 input_channel_add(kernel_channel);
	 }
 }

 static __attribute__((destructor)) void cleanup(void)
 {
	input_channel_remove(kernel_channel);
	kernel_channel = NULL;
 }