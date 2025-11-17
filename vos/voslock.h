#ifndef _OBJLOCK_H
#define _OBJLOCK_H

#include <stdio.h>
#include <pthread.h>

/** @brief 定义互斥锁并初始化 */
#define DEFINE_VOSLOCK(x)            pthread_mutex_t x = PTHREAD_MUTEX_INITIALIZER

/** @brief 声明外部互斥锁 */
#define DECLARE_VOSLOCK(x)           extern pthread_mutex_t x

/** @brief 互斥锁类型定义 */
#define VOSLOCK                      pthread_mutex_t

/** @brief 初始化互斥锁 */
#define voslkinit(x)                 pthread_mutex_init((x), NULL)

/** @brief 获取互斥锁 */
#define voslklock(x)                 pthread_mutex_lock(x)

/** @brief 释放互斥锁 */
#define voslkunlock(x)               pthread_mutex_unlock(x)

/** @brief 对象锁加锁/解锁宏（使用当前线程ID） */
#define objlocklk(x, l)              objlock_lock((x), 0, (l))

/** @brief 对象锁初始化宏 */
#define objlockinit(x)               objlock_init(x)


/**
 * @brief 对象锁结构体
 * 
 * 用于实现可重入的对象级别锁，支持线程ID识别和计数
 */
typedef struct {
    pthread_mutex_t lock;    /**< 底层互斥锁 */
    unsigned long tid;       /**< 当前持有锁的线程ID */
    unsigned long cnt;       /**< 锁计数（可重入次数） */
} OBJLOCK;

/**
 * @brief 初始化对象锁
 * @param lock 指向OBJLOCK结构的指针
 * @return 成功返回OBJLOCK_SUCCESS，失败返回错误码
 */
int objlock_init(OBJLOCK *lock);

/**
 * @brief 加锁或解锁对象锁
 * @param lock 指向OBJLOCK结构的指针
 * @param tid 线程ID，如果为0则使用当前线程ID
 * @param lk 操作类型：1表示加锁，0表示解锁
 * @return 成功返回OBJLOCK_SUCCESS，失败返回错误码
 */
int objlock_lock(OBJLOCK *lock, unsigned long tid, char lk);

/**
 * @brief 带超时的加锁或解锁对象锁
 * @param lock 指向OBJLOCK结构的指针
 * @param tid 线程ID，如果为0则使用当前线程ID
 * @param msecs 超时时间（毫秒）
 * @param lk 操作类型：1表示加锁，0表示解锁
 * @return 成功返回OBJLOCK_SUCCESS，失败返回错误码
 */
int objlock_locktm(OBJLOCK *lock, unsigned long tid, unsigned long msecs, char lk);

#endif /* _OBJLOCK_H */