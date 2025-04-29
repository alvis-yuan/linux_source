/**
 * @file timer.c
 * @brief 通用定时器组件实现
 */
//#define _POSIX_C_SOURCE 199309L

 #include "timer.h"
 #include <unistd.h>
 #include <sys/timerfd.h>
 #include <time.h>
 #include <stdlib.h>
 #include <stdio.h>
 #include <string.h>
 
 struct timer {
     int fd;                     /**< timerfd文件描述符 */
     uint64_t interval;          /**< 定时器间隔(毫秒) */
     bool is_periodic;           /**< 是否周期性定时器 */
     void (*callback)(void *);   /**< 回调函数指针 */
     void *user_data;            /**< 用户数据指针 */
 };
 
 timer_st *timer_creat(uint64_t interval, void (*callback)(void *), 
                      void *user_data, bool is_periodic)
 {
     timer_st *timer;
     
     if (interval == 0 || callback == NULL) {
         return NULL;
     }
     
     timer = (timer_st *)malloc(sizeof(timer_st));
     if (timer == NULL) {
         perror("malloc");
         return NULL;
     }
     
     memset(timer, 0, sizeof(timer_st));
     
     timer->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
     if (timer->fd == -1) {
         perror("timerfd_create");
         free(timer);
         return NULL;
     }
     
     timer->interval = interval;
     timer->callback = callback;
     timer->user_data = user_data;
     timer->is_periodic = is_periodic;
     
     return timer;
 }
 
 int timer_get_fd(const timer_st *timer)
 {
     if (timer == NULL) {
         return -1;
     }
     return timer->fd;
 }
 
 int timer_handle_event(timer_st *timer)
 {
     uint64_t exp;
     ssize_t s;
     int ret = 0;
     
     if (timer == NULL) {
         return -1;
     }
     
     s = read(timer->fd, &exp, sizeof(uint64_t));
     if (s != sizeof(uint64_t)) {
         perror("read");
         return -1;
     }
     
     if (timer->callback) {
         timer->callback(timer->user_data);
     }
     
     /* 如果是one-shot定时器，处理完事件后自动删除 */
     if (!timer->is_periodic) {
         timer_destroy(timer);
         ret = 1; /* 返回1表示定时器已被删除 */
     }
     
     return ret;
 }
 
 int timer_start(timer_st *timer)
 {
     struct itimerspec new_value;
     uint64_t ns_interval;
     
     if (timer == NULL) {
         return -1;
     }
     
     ns_interval = timer->interval * 1000000ULL;
     
     new_value.it_value.tv_sec = ns_interval / 1000000000ULL;
     new_value.it_value.tv_nsec = ns_interval % 1000000000ULL;
     
     if (timer->is_periodic) {
         new_value.it_interval.tv_sec = new_value.it_value.tv_sec;
         new_value.it_interval.tv_nsec = new_value.it_value.tv_nsec;
     } else {
         new_value.it_interval.tv_sec = 0;
         new_value.it_interval.tv_nsec = 0;
     }
     
     if (timerfd_settime(timer->fd, 0, &new_value, NULL) == -1) {
         perror("timerfd_settime");
         return -1;
     }
     
     return 0;
 }
 
 int timer_stop(timer_st *timer)
 {
     struct itimerspec new_value;
     
     if (timer == NULL) {
         return -1;
     }
     
     new_value.it_value.tv_sec = 0;
     new_value.it_value.tv_nsec = 0;
     new_value.it_interval.tv_sec = 0;
     new_value.it_interval.tv_nsec = 0;
     
     if (timerfd_settime(timer->fd, 0, &new_value, NULL) == -1) {
         perror("timerfd_settime");
         return -1;
     }
     
     return 0;
 }
 
 void timer_destroy(timer_st *timer)
 {
     if (timer == NULL) {
         return;
     }
     
     if (timer->fd != -1) {
         close(timer->fd);
     }
     
     free(timer);
 }