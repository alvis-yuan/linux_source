#ifndef VOS_TIMER_H
#define VOS_TIMER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct _timer vos_timer_t;

typedef void (*timer_cb_t)(vos_timer_t *);

/**
 * @brief 创建定时器
 * @param timer_cb 定时器回调函数
 * @param period 定时器周期，单位毫秒
 * @param user_data 用户自定义数据
 * @return 成功返回定时器指针，失败返回NULL
 */
vos_timer_t *vos_timer_create(timer_cb_t timer_cb, uint32_t period, void *user_data);

/**
 * @brief 删除定时器
 * @param timer 定时器指针
 */
void vos_timer_delete(vos_timer_t * timer);

/**
 * @brief 暂停定时器
 * @param timer 定时器指针
 */
void vos_timer_pause(vos_timer_t * timer);
#define vos_timer_stop(timer) vos_timer_pause(timer)

/**
 * @brief 恢复定时器
 * @param timer 定时器指针
 */
void vos_timer_resume(vos_timer_t * timer);

/**
 * @brief 启动定时器
 * @param timer 定时器指针
 */
void vos_timer_start(vos_timer_t * timer);

/**
 * @brief 设置定时器回调函数
 * @param timer 定时器指针
 * @param timer_cb 回调函数
 * 
 * @note 设置前，需要暂停定时器，设置后需要自行调用 vos_timer_resume 恢复定时器
 */
void vos_timer_set_cb(vos_timer_t * timer, timer_cb_t timer_cb);

/**
 * @brief 设置定时器周期
 * @param timer 定时器指针
 * @param period 定时器周期，单位毫秒
 * 
 * @note 设置前，需要暂停定时器，设置后需要自行调用 vos_timer_resume 恢复定时器
 */
void vos_timer_set_period(vos_timer_t * timer, uint32_t period);

/**
 * @brief 设置定时器重复次数
 * @param timer 定时器指针
 * @param repeat_count 重复次数，1: 单次; -1: 无限次; n>0: 有限次数
 * 
 * @note 设置前，需要暂停定时器，设置后需要自行调用 vos_timer_resume 恢复定时器
 */
void vos_timer_set_repeat_count(vos_timer_t * timer, int32_t repeat_count);

/**
 * @brief 设置定时器自动删除标志
 * @param timer 定时器指针
 * @param auto_delete 自动删除标志
 * 
 * @note 设置前，需要暂停定时器，设置后需要自行调用 vos_timer_resume 恢复定时器
 */
void vos_timer_set_auto_delete(vos_timer_t * timer, bool auto_delete);

/**
 * @brief 设置定时器用户数据
 * @param timer 定时器指针
 * @param user_data 用户数据
 * 
 * @note 设置前，需要暂停定时器，设置后需要自行调用 vos_timer_resume 恢复定时器
 */
void vos_timer_set_user_data(vos_timer_t * timer, void *user_data);

/**
 * @brief 设置定时器是否已被polled
 * @param timer 定时器指针
 * @param polled 定时器是否已被polled
 */
void vos_timer_set_polled(vos_timer_t * timer, bool polled);

/**
 * @brief 获取定时器暂停状态
 * @param timer 定时器指针
 * @return 暂停状态
 */
bool vos_timer_get_paused(vos_timer_t * timer);

/**
 * @brief 获取定时器用户数据
 * @param timer 定时器指针
 * @return 用户数据
 */
void *vos_timer_get_user_data(vos_timer_t * timer);

#endif /* VOS_TIMER_H */