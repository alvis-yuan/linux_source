#ifndef VOS_SIGNAL_H
#define VOS_SIGNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <signal.h>

// 信号回调函数类型
typedef void (*signal_cb_t)(const struct signalfd_siginfo *info, void *user_data);

/**
 * @brief 初始化信号处理器
 * @return 成功返回0，失败返回-1
 */
int vos_signal_init(void);

/**
 * @brief 销毁信号处理器
 */
void vos_signal_destroy(void);

/**
 * @brief 安装信号处理器
 * @param signo 信号编号
 * @param signal_cb 信号回调函数
 * @param user_data 用户自定义数据
 * @return 成功返回0，失败返回-1
 */
int vos_signal_install(int signo, signal_cb_t signal_cb, void *user_data);

/**
 * @brief 移除信号处理器
 * @param signo 信号编号
 * @return 成功返回0，失败返回-1
 * 
 * @note 该接口只能在 vos_signal_start 之前调用
 */
int vos_signal_uninstall(int signo);

/**
 * @brief 启动信号处理器
 * @return 成功返回0，失败返回-1
 * 
 * @note 该函数一般是主线程调用，且需要在创建新线程之前调用，因为新线程会继承主线程的信号掩码
 *      该接口会屏蔽信号，继承主线程的信号掩码的线程就不会处理对应的信号
 */
int vos_signal_start(void);

/**
 * @brief 停止信号处理器
 * @return 成功返回0，失败返回-1
 */
int vos_signal_stop(void);

#endif /* VOS_SIGNAL_H */
