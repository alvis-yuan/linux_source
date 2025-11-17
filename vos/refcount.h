/**
 * @file refcount.h
 * @brief 用户态引用计数实现
 * @note 基于Linux内核refcount_t接口，但移除了内核特性
 */

#ifndef _USER_REFCOUNT_H
#define _USER_REFCOUNT_H

#include "vos.h"

/**
 * @struct refcount_t
 * @brief 引用计数结构体
 */
typedef struct {
    atomic_t refs;
} refcount_t;

#define REFCOUNT_INIT(n)	{ .refs = ATOMIC_INIT(n), }

/**
 * @brief 初始化引用计数
 * @param r 引用计数指针
 * @param n 初始值
 */
static inline void refcount_set(refcount_t *r, unsigned int n)
{
    atomic_set(&r->refs, n);
}

/**
 * @brief 读取引用计数值
 * @param r 引用计数指针
 * @return 当前引用计数值
 */
static inline unsigned int refcount_read(const refcount_t *r)
{
    return atomic_read(&r->refs);
}

/**
 * @brief 增加引用计数（非零时）
 * @param i 要增加的值
 * @param r 引用计数指针
 * @return 成功返回true，计数为0时返回false
 */
static inline bool refcount_add_not_zero_checked(unsigned int i, refcount_t *r)
{
    unsigned int new;
    unsigned int val = atomic_read(&r->refs);

    do {
        if (!val) {
            return false;
        }

        if (val == UINT_MAX) {
            LogError("refcount saturated, leaking memory");
            return true;
        }

        new = val + i;
        if (new < val) {
            new = UINT_MAX;
        }

    } while (!atomic_cmpxchg(&r->refs, val, new));

    if (new == UINT_MAX) {
        LogError("refcount saturated, leaking memory");
    }

    return true;
}

/**
 * @brief 增加引用计数
 * @param i 要增加的值
 * @param r 引用计数指针
 */
static inline void refcount_add_checked(unsigned int i, refcount_t *r)
{
    bool success;

    success = refcount_add_not_zero_checked(i, r);
    if (!success) {
        LogError("addition on 0, use-after-free");
    }
}

/**
 * @brief 递增引用计数（非零时）
 * @param r 引用计数指针
 * @return 成功返回true，计数为0时返回false
 */
static inline bool refcount_inc_not_zero_checked(refcount_t *r)
{
    unsigned int new;
    unsigned int val = atomic_read(&r->refs);

    do {
        new = val + 1;

        if (!val) {
            return false;
        }

        if (new == 0) {
            LogError("refcount saturated, leaking memory");
            return true;
        }

    } while (!atomic_cmpxchg(&r->refs, val, new));

    if (new == UINT_MAX) {
        LogError("refcount saturated, leaking memory");
    }

    return true;
}

/**
 * @brief 递增引用计数
 * @param r 引用计数指针
 */
static inline void refcount_inc_checked(refcount_t *r)
{
    bool success;

    success = refcount_inc_not_zero_checked(r);
    if (!success) {
        LogError("increment on 0, use-after-free");
    }
}

/**
 * @brief 减少引用计数并测试是否为0
 * @param i 要减少的值
 * @param r 引用计数指针
 * @return 为0返回true，否则返回false
 */
static inline bool refcount_sub_and_test_checked(unsigned int i, refcount_t *r)
{
    unsigned int new;
    unsigned int val = atomic_read(&r->refs);

    do {
        if (val == UINT_MAX) {
            LogError("refcount saturated, cannot decrement");
            return false;
        }

        new = val - i;
        if (new > val) {
            LogError("refcount underflow, use-after-free");
            return false;
        }

    } while (!atomic_cmpxchg(&r->refs, val, new));

    if (new == 0) {
        return true;
    }

    return false;
}

/**
 * @brief 递减引用计数并测试是否为0
 * @param r 引用计数指针
 * @return 为0返回true，否则返回false
 */
static inline bool refcount_dec_and_test_checked(refcount_t *r)
{
    return refcount_sub_and_test_checked(1, r);
}

/**
 * @brief 递减引用计数
 * @param r 引用计数指针
 */
static inline void refcount_dec_checked(refcount_t *r)
{
    bool result;

    result = refcount_dec_and_test_checked(r);
    if (result) {
        LogError("decrement hit 0, leaking memory");
    }
}

/**
 * @brief 如果引用计数为1则递减
 * @param r 引用计数指针
 * @return 成功递减返回true，否则返回false
 */
static inline bool refcount_dec_if_one(refcount_t *r)
{
    unsigned int val = 1;

    return atomic_cmpxchg(&r->refs, val, 0);
}

/**
 * @brief 如果引用计数不为1则递减
 * @param r 引用计数指针
 * @return 成功递减返回true，否则返回false
 */
static inline bool refcount_dec_not_one(refcount_t *r)
{
    unsigned int new;
    unsigned int val = atomic_read(&r->refs);

    do {
        if (val == UINT_MAX) {
            LogInfo("refcount saturated");
        }

        if (val == 1) {
            return false;
        }

        new = val - 1;
        if (new > val) {
            LogError("refcount underflow, use-after-free");
            return false;
        }

    } while (!atomic_cmpxchg(&r->refs, val, new));

    return true;
}


#define refcount_add_not_zero	refcount_add_not_zero_checked
#define refcount_add		refcount_add_checked

#define refcount_inc_not_zero	refcount_inc_not_zero_checked
#define refcount_inc		refcount_inc_checked

#define refcount_sub_and_test	refcount_sub_and_test_checked

#define refcount_dec_and_test	refcount_dec_and_test_checked
#define refcount_dec		refcount_dec_checked

#endif /* _USER_REFCOUNT_H */