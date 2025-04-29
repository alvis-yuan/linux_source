/**
 * @file channel.h
 * @brief 通道通用框架定义
 * @author Your Name
 * @version 1.0
 * @date 2023-08-01
 */

 #ifndef __CHANNEL_H__
 #define __CHANNEL_H__
 
 #include <sys/select.h>
 #include <stdbool.h>
 #include "log_api.h"
 
 /**
  * @struct input_channel_ops
  * @brief 输入通道操作函数表
  */
 struct input_channel_ops {
	 /** 获取文件描述符 */
	 int (*get_fd)(void *priv);
	 /** 读取数据 */
	 int (*read)(void *priv, log_msg_t **msg);
	 /** 销毁通道 */
	 void (*destroy)(void *priv);
 };
 
 /**
  * @struct input_channel
  * @brief 输入通道结构
  */
 struct input_channel {
	 char *name;                       /**< 通道名称 */
	 const struct input_channel_ops *ops; /**< 操作函数表 */
	 void *priv;                         /**< 私有数据 */
	 struct input_channel *next;         /**< 下一个通道 */
 };
 
 /**
  * @struct output_channel_ops
  * @brief 输出通道操作函数表
  */
 struct output_channel_ops {
	 /** 写入日志 */
	 int (*write)(void *priv, const log_msg_t *msg);
	 /** 销毁通道 */
	 void (*destroy)(void *priv);
	 /** 轮转日志 */
	 int (*rotate)(void *priv);
 };
 
 /**
  * @struct output_channel
  * @brief 输出通道结构
  */
 struct output_channel {
	char *name;                       /**< 通道名称 */
	 const struct output_channel_ops *ops; /**< 操作函数表 */
	 void *priv;                          /**< 私有数据 */
	 struct output_channel *next;         /**< 下一个通道 */
 };
 
 /**
  * @brief 创建输入通道链表
  * @return 成功返回0，失败返回-1
  */
 int input_channels_init(void);

 /**
  * @brief 移除输入通道
  * @param channel 输入通道指针
  */
 void input_channel_remove(struct input_channel *channel);

 
 /**
  * @brief 添加输入通道
  * @param channel 输入通道指针
  */
 void input_channel_add(struct input_channel *channel);
 
 /**
  * @brief 创建输出通道链表
  * @return 成功返回0，失败返回-1
  */
 int output_channels_init(void);
 
 /**
  * @brief 添加输出通道
  * @param channel 输出通道指针
  */
 void output_channel_add(struct output_channel *channel);
 
 /**
  * @brief 移除输出通道
  * @param channel 输出通道指针
  */
 void output_channel_remove(struct output_channel *channel);

 /**
  * @brief 销毁所有通道
  */
 void destroy_channels(void);

 /**
  * @brief 主循环处理函数
  */
 void channel_main_loop();


 const char *level_to_str(log_level_t level);

 
 #endif /* __CHANNEL_H__ */