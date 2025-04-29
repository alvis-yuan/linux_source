/**
 * @file log_api.h
 * @brief 日志客户端API
 */

 #ifndef __LOG_API_H__
 #define __LOG_API_H__
 
 #include <sys/types.h>
 #include <stdio.h>
 
 /**
  * @enum log_level
  * @brief 日志级别定义
  */
 typedef enum {
	 LOG_LEVEL_CRITICAL,
	 LOG_LEVEL_ERROR,
	 LOG_LEVEL_WARNING,
	 LOG_LEVEL_INFO,
	 LOG_LEVEL_DEBUG,
	 LOG_LEVEL_MAX
 } log_level_t;

  /**
  * @struct log_msg
  * @brief 日志消息结构
  */
 typedef struct {
	int pid;            /**< 进程ID */
	log_level_t level;   /**< 日志级别 */
	char data[];        /**< 日志数据 */
} __attribute__((packed)) log_msg_t;
 
 /**
  * @brief 记录日志
  * @param level 日志级别
  * @param format 格式化字符串
  * @param file 文件名
  * @param line 行号
  * @param func 函数名
  * @param ... 可变参数
  */
 void log_write(char level,const char *file,int line,const char *func,const char *format,...);

 #define _LogWrite(n,fmt,args...) log_write(n,__FILE__,__LINE__,__func__,fmt,##args)

 #define LogInfo(fmt,args...)  _LogWrite(LOG_LEVEL_INFO,fmt,##args)
 #define LogDbg(fmt,args...)   _LogWrite(LOG_LEVEL_DEBUG,fmt,##args)

 #define LocalDbg(fmt,args...) fprintf(stderr,"%s|%d|%s() "fmt"\n",__FILE__,__LINE__,__func__,##args)

 #endif /* __LOG_API_H__ */