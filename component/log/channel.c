/**
 * @file channel.c
 * @brief 通道通用框架实现
 */

 #include "channel.h"
 #include "filter.h"
 #include <stdlib.h>
 #include <unistd.h>
 #include <string.h>
 #include <errno.h>
 #include <time.h>
 #include <sys/epoll.h>
 #include <signal.h>

 #define MAX_EPOLL_EVENTS 1024
 
 static struct input_channel *input_channels;
 static struct output_channel *output_channels;
 static time_t last_rotate_time;
 
 int input_channels_init(void)
 {
	 input_channels = NULL;
	 return 0;
 }
 
 void input_channel_add(struct input_channel *channel)
 {
	 if (!channel)
		 return;
 
	 channel->next = input_channels;
	 input_channels = channel;
 }

 // 移除输入通道
 void input_channel_remove(struct input_channel *channel)
 {
	 struct input_channel **p = &input_channels;
	 while (*p) {
		 if (*p == channel) {
			 *p = channel->next;
			 if (channel->ops->destroy)
				 channel->ops->destroy(channel->priv);
			 return;
		 }
		 p = &(*p)->next;
	 }
 }
 
 int output_channels_init(void)
 {
	 output_channels = NULL;
	 last_rotate_time = time(NULL);
	 return 0;
 }

 // 移除输出通道
 void output_channel_remove(struct output_channel *channel)
 {
	 struct output_channel **p = &output_channels;
	 while (*p) {
		 if (*p == channel) {
			 *p = channel->next;
			 if (channel->ops->destroy)
				 channel->ops->destroy(channel->priv);
			 return;
		 }
		 p = &(*p)->next;
	 }
 }
 
 
 void output_channel_add(struct output_channel *channel)
 {
	 if (!channel)
		 return;
 
	 channel->next = output_channels;
	 output_channels = channel;
 }
 
 static void process_message(log_msg_t *msg)
 {
	struct output_channel *channel = output_channels;
	// 处理日志消息
	//LocalDbg("Received message from channel %s", channel->name);
	while (channel) {
		if (!filter_process(msg)) {
			channel->ops->write(channel->priv, msg);
		}
		channel = channel->next;
	}

 }

 // 遍历输入输出通道链表，调用destroy回调函数销毁通道
 void destroy_channels(void)
 {
	 struct input_channel *input_channel = input_channels;
	 struct output_channel *output_channel = output_channels;
	 
	 while (input_channel) {
		 struct input_channel *tmp = input_channel;
		 input_channel = input_channel->next;
		 if (tmp->ops->destroy)
			 tmp->ops->destroy(tmp->priv);
	 }
	 while (output_channel) {
		 struct output_channel *tmp = output_channel;
		 output_channel = output_channel->next;
		 if (tmp->ops->destroy)
			 tmp->ops->destroy(tmp->priv);
	 }
 }
 
 void channel_main_loop()
 {
	struct epoll_event ev, events[MAX_EPOLL_EVENTS];
	int epoll_fd, nfds, i;
	struct input_channel *channel;
	log_msg_t *msg;
	volatile sig_atomic_t running = 1;
	int ret = 0;

	// 创建epoll实例
	epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		LocalDbg("epoll_create1 error(%d),%s", errno, strerror(errno));
		return;
	}

	// 添加所有输入通道到epoll
	channel = input_channels;
	while (channel) {
		int fd = channel->ops->get_fd(channel->priv);
		ev.events = EPOLLIN;
		ev.data.ptr = channel;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
			LocalDbg("epoll_ctl add error(%d),%s", errno, strerror(errno));
		}
		channel = channel->next;
	}

	while (running) {
		// 等待事件，超时1分钟
		nfds = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, 60 * 1000);
		if (nfds == -1) {
			//if (errno == EINTR)
				//continue;
			LocalDbg("epoll_wait error(%d),%s", errno, strerror(errno));
			break;
		}

		// 处理所有就绪的事件
		for (i = 0; i < nfds; ++i) {
			channel = events[i].data.ptr;
			
			if (channel->ops->read) {
				ret = channel->ops->read(channel->priv, &msg);
				if (ret == -1) {
					LocalDbg("read error(%d),%s", errno, strerror(errno));
					continue;
				} else if (ret == 0) {
					process_message(msg);
					free(msg);
				} else if (ret == -2) {
					// 退出信号
					running = 0;
					break;
				}
			}
			
		}
	}

	close(epoll_fd);	
 }

 const char *level_to_str(log_level_t level)
 {
	 static const char * const levels[] = {
		 "CRITICAL"
		 "ERROR",
		 "WARNING",
		 "INFO",
		 "DEBUG",
	 };
	 
	 if (level >= LOG_LEVEL_MAX)
		 return "UNKNOWN";
	 
	 return levels[level];
 }