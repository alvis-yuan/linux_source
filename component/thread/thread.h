/**
 * @file thread_ops.h
 * @brief 线程操作接口头文件
 */

#ifndef THREAD_OPS_H
#define THREAD_OPS_H

#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/** 调试信息开关 */
#define MUTEX_DEBUG

typedef struct _mutex_ctx mutex_ctx_t;

/**
 * @brief 初始化互斥锁
 * @param lck 互斥锁上下文指针
 * @return 成功返回互斥锁上下文指针，失败返回NULL
 */
mutex_ctx_t *lock_init(void);

/**
 * @brief 获取互斥锁
 * @param lck 互斥锁上下文指针
 * @return 成功返回0，失败返回-1
 */
int lock_acquire(mutex_ctx_t *lck, const char *file, const char *func, int line);

/**
 * @brief 释放互斥锁
 * @param lck 互斥锁上下文指针
 * @return 成功返回0，失败返回-1
 */
int lock_release(mutex_ctx_t *lck, const char *file, const char *func, int line);

/**
 * @brief 销毁互斥锁
 * @param lck 互斥锁上下文指针
 * @return 成功返回0，失败返回-1
 */
int lock_destroy(mutex_ctx_t *lck);

/**
 * @brief 打印锁的状态信息
 * @param lck 互斥锁上下文指针
 */
void lock_stat(mutex_ctx_t *lck);


#define LOCK_ACQUIRE(lck) lock_acquire(lck, __FILE__, __func__, __LINE__)
#define LOCK_RELEASE(lck) lock_release(lck, __FILE__, __func__, __LINE__)

/**
 * @brief 创建线程
 * 
 * @param thread 线程ID指针
 * @param attr 线程属性指针
 * @param start_routine 线程启动函数指针
 * @param arg 线程启动函数参数
 * @return 成功返回0，失败返回-1
 */
int thread_create(pthread_t *thread, const pthread_attr_t *attr,
                  void *(*start_routine)(void *), void *arg, const char *name);
#define THREAD_CREATE(thread, attr, start_routine, arg) \
    thread_create(thread, attr, start_routine, arg, #start_routine)

typedef struct _cond_ctx cond_ctx_t;

/**
 * @brief 初始化条件变量
 * @return 成功返回条件变量上下文指针，失败返回NULL
 */
cond_ctx_t *cond_var_init(void);

/**
 * @brief 等待条件变量
 * @param cond 条件变量上下文指针
 * @return 成功返回0，失败返回-1
 */
int cond_var_wait(cond_ctx_t *cond);

/**
 * @brief 发送信号量
 * @param cond 条件变量上下文指针
 * @return 成功返回0，失败返回-1
 */
int cond_var_signal(cond_ctx_t *cond);

/**
 * @brief 广播信号量
 * @param cond 条件变量上下文指针
 * @return 成功返回0，失败返回-1
 */
int cond_var_broadcast(cond_ctx_t *cond);

/**
 * @brief 销毁条件变量
 * @param cond 条件变量上下文指针
 * @return 成功返回0，失败返回-1
 */
int cond_var_destroy(cond_ctx_t *cond);


#endif /* THREAD_OPS_H */