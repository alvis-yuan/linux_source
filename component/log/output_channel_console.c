/**
 * @file output_channel_console.c
 * @brief 控制台输出通道实现
 */

 #include "channel.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <time.h>
 #include <string.h>

	 
 static struct output_channel *console_channel = NULL;
 
 struct console_priv {
	 FILE *stream;
 };
 
 
 static int console_write(void *priv, const log_msg_t *msg)
 {
	 struct console_priv *c = priv;
	 time_t now;
	 struct tm *tm_info;
	 char time_buf[20];
 
	 now = time(NULL);
	 tm_info = localtime(&now);
	 strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
 
	 fprintf(c->stream, "[%s] [%s] [PID:%d] %s\n",
			 time_buf, level_to_str(msg->level), msg->pid, msg->data);
	 
	 return 0;
 }
 
 static void console_destroy(void *priv)
 {
	 struct console_priv *c = priv;
	 if (c->stream)
		 fclose(c->stream);
	 free(c);
	 if (console_channel) {
		if (console_channel->name)
			free(console_channel->name);
		 free(console_channel);
		 console_channel = NULL;
	 }
 }
 
 static const struct output_channel_ops console_ops = {
	 .write = console_write,
	 .destroy = console_destroy,
 };
 
 static struct output_channel *output_channel_console_create(void)
 {
	 struct console_priv *priv;
	 struct output_channel *channel;
 
	 priv = malloc(sizeof(*priv));
	 if (!priv)
		 return NULL;
 
	 priv->stream = stdout;
 
	 channel = malloc(sizeof(*channel));
	 if (!channel) {
		 free(priv);
		 return NULL;
	 }
 
	 channel->ops = &console_ops;
	 channel->priv = priv;
	 channel->next = NULL;
	 channel->name = strdup("console");
 
	 return channel;
 }

 static __attribute__((constructor)) void init(void)
 {
	 /* 创建输出通道 */
	 console_channel = output_channel_console_create();
	 if (console_channel)
		 output_channel_add(console_channel);
 }


 static __attribute__((destructor)) void cleanup(void)
 {
	if (console_channel) {
		LocalDbg("destroy console channel");
		/* 移除输出通道 */
		output_channel_remove(console_channel);
		console_channel = NULL;
	}
}
