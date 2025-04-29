/**
 * @file timer.h
 * @brief 通用定时器组件头文件
 */

 #ifndef _TIMER_H_
 #define _TIMER_H_
 
 #include <stdint.h>
 #include <stdbool.h>
 
 /** 不完整的定时器类型定义，隐藏内部实现细节 */
 typedef struct timer timer_st;
 
 /**
  * @brief 创建定时器
  * @param interval 定时器间隔(毫秒)
  * @param callback 定时器回调函数
  * @param user_data 用户数据指针
  * @param is_periodic 是否周期性定时器
  * @return 成功返回定时器指针，失败返回NULL
  * @note 对于one-shot定时器，回调后会自动删除定时器
  */
 timer_st *timer_creat(uint64_t interval, void (*callback)(void *), 
                      void *user_data, bool is_periodic);
 
 /**
  * @brief 获取定时器的文件描述符
  * @param timer 定时器指针
  * @return 成功返回文件描述符，失败返回-1
  * @note 用户应使用此fd进行poll/epoll等操作
  */
 int timer_get_fd(const timer_st *timer);
 
 /**
  * @brief 处理定时器事件
  * @param timer 定时器指针
  * @return 成功返回0，失败返回-1
  * @note 当检测到fd可读时调用此函数
  */
 int timer_handle_event(timer_st *timer);
 
 /**
  * @brief 启动定时器
  * @param timer 定时器指针
  * @return 成功返回0，失败返回-1
  */
 int timer_start(timer_st *timer);
 
 /**
  * @brief 停止定时器
  * @param timer 定时器指针
  * @return 成功返回0，失败返回-1
  */
 int timer_stop(timer_st *timer);
 
 /**
  * @brief 销毁定时器
  * @param timer 定时器指针
  * @note 会关闭内部fd并释放资源
  */
 void timer_destroy(timer_st *timer);
 
 #endif /* _TIMER_H_ */