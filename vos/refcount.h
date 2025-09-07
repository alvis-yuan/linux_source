
/**
 * @file refcount.h
 * @brief 用户态引用计数实现，参考Linux内核实现
 */

#ifndef _REFCOUNT_H_
#define _REFCOUNT_H_

#include "atomic.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 引用计数结构体
 * 
 * 计数器在达到REFCOUNT_SATURATED时会饱和，避免环绕计数导致的use-after-free问题
 */
typedef struct refcount_struct {
    atomic_int refs; /**< 原子计数器 */
} refcount_t;

/** @brief 引用计数最大值 */
#define REFCOUNT_MAX        INT_MAX
/** @brief 引用计数饱和值 */
#define REFCOUNT_SATURATED  (INT_MIN / 2)

/**
 * @brief 引用计数饱和类型枚举
 */
enum refcount_saturation_type {
    REFCOUNT_ADD_NOT_ZERO_OVF, /**< 加非零操作溢出 */
    REFCOUNT_ADD_OVF,          /**< 加法操作溢出 */
    REFCOUNT_ADD_UAF,          /**< 在0上加法操作（use-after-free） */
    REFCOUNT_SUB_UAF,          /**< 减法操作下溢（use-after-free） */
    REFCOUNT_DEC_LEAK,         /**< 减到0以下（内存泄漏） */
};

/**
 * @brief 引用计数警告宏
 */
#define REFCOUNT_WARN(str) printf("refcount_t: " str ".\n")

/**
 * @brief 初始化引用计数
 * @param n 初始值
 */
#define REFCOUNT_INIT(n) { .refs = (n) }

/**
 * @brief 设置引用计数值
 * @param r 引用计数对象
 * @param n 要设置的值
 */
static inline void refcount_set(refcount_t *r, int n)
{
    atomic_store(&r->refs, n);
}

/**
 * @brief 引用计数饱和警告处理
 * @param r 引用计数对象
 * @param t 饱和类型
 */
static inline void refcount_warn_saturate(refcount_t *r, enum refcount_saturation_type t)
{
    refcount_set(r, REFCOUNT_SATURATED);

    switch (t) {
    case REFCOUNT_ADD_NOT_ZERO_OVF:
        REFCOUNT_WARN("饱和；内存泄漏");
        break;
    case REFCOUNT_ADD_OVF:
        REFCOUNT_WARN("饱和；内存泄漏");
        break;
    case REFCOUNT_ADD_UAF:
        REFCOUNT_WARN("在0上加法操作；use-after-free");
        break;
    case REFCOUNT_SUB_UAF:
        REFCOUNT_WARN("下溢；use-after-free");
        break;
    case REFCOUNT_DEC_LEAK:
        REFCOUNT_WARN("减少到0；内存泄漏");
        break;
    default:
        REFCOUNT_WARN("未知饱和事件!?");
    }
}

/**
 * @brief 读取引用计数值
 * @param r 引用计数对象
 * @return 当前的引用计数值
 */
static inline unsigned int refcount_read(const refcount_t *r)
{
    return atomic_load(&r->refs);
}

/**
 * @brief 内部函数：添加值到引用计数（非零时）
 * @param i 要添加的值
 * @param r 引用计数对象
 * @param oldp 用于返回旧值的指针（可为NULL）
 * @return 如果旧值不为0则返回true，否则返回false
 */
static inline __attribute__((warn_unused_result)) bool 
__refcount_add_not_zero(int i, refcount_t *r, int *oldp)
{
    int old = refcount_read(r);
    int expected;

    do {
        if (!old)
            break;
        
        expected = old;
    } while (!atomic_compare_exchange_strong(&r->refs, &expected, old + i));

    if (oldp)
        *oldp = old;

    if (old < 0 || old + i < 0)
        refcount_warn_saturate(r, REFCOUNT_ADD_NOT_ZERO_OVF);

    return old;
}

/**
 * @brief 添加值到引用计数（除非它为0）
 * @param i 要添加的值
 * @param r 引用计数对象
 * @return 如果引用计数不为0则返回true，否则返回false
 * 
 * @warning 不推荐在正常的引用计数场景中使用此函数
 */
static inline __attribute__((warn_unused_result)) bool 
refcount_add_not_zero(int i, refcount_t *r)
{
    return __refcount_add_not_zero(i, r, NULL);
}

/**
 * @brief 内部函数：添加值到引用计数
 * @param i 要添加的值
 * @param r 引用计数对象
 * @param oldp 用于返回旧值的指针（可为NULL）
 */
static inline void __refcount_add(int i, refcount_t *r, int *oldp)
{
    printf("before value: %d\n", refcount_read(&r));
    int old = atomic_fetch_add(&r->refs, i);
    printf("after value: %d\n", refcount_read(&r));
    if (oldp)
        *oldp = old;

    if (!old)
        refcount_warn_saturate(r, REFCOUNT_ADD_UAF);
    else if (old < 0 || old + i < 0)
        refcount_warn_saturate(r, REFCOUNT_ADD_OVF);
}

/**
 * @brief 添加值到引用计数
 * @param i 要添加的值
 * @param r 引用计数对象
 * 
 * @warning 不推荐在正常的引用计数场景中使用此函数
 */
static inline void refcount_add(int i, refcount_t *r)
{
    __refcount_add(i, r, NULL);
}

/**
 * @brief 内部函数：增加引用计数（除非它为0）
 * @param r 引用计数对象
 * @param oldp 用于返回旧值的指针（可为NULL）
 * @return 如果增加成功则返回true，否则返回false
 */
static inline __attribute__((warn_unused_result)) bool 
__refcount_inc_not_zero(refcount_t *r, int *oldp)
{
    return __refcount_add_not_zero(1, r, oldp);
}

/**
 * @brief 增加引用计数（除非它为0）
 * @param r 引用计数对象
 * @return 如果增加成功则返回true，否则返回false
 */
static inline __attribute__((warn_unused_result)) bool 
refcount_inc_not_zero(refcount_t *r)
{
    return __refcount_inc_not_zero(r, NULL);
}

/**
 * @brief 内部函数：增加引用计数
 * @param r 引用计数对象
 * @param oldp 用于返回旧值的指针（可为NULL）
 */
static inline void __refcount_inc(refcount_t *r, int *oldp)
{
    __refcount_add(1, r, oldp);
}

/**
 * @brief 增加引用计数
 * @param r 引用计数对象
 * 
 * @warning 如果引用计数为0会产生警告（可能use-after-free）
 */
static inline void refcount_inc(refcount_t *r)
{
    __refcount_inc(r, NULL);
}

/**
 * @brief 内部函数：从引用计数减去值并测试是否为0
 * @param i 要减去的值
 * @param r 引用计数对象
 * @param oldp 用于返回旧值的指针（可为NULL）
 * @return 如果结果为0则返回true，否则返回false
 */
static inline __attribute__((warn_unused_result)) bool 
__refcount_sub_and_test(int i, refcount_t *r, int *oldp)
{
    int old = atomic_fetch_sub(&r->refs, i);

    if (oldp)
        *oldp = old;

    if (old == i) {
        memory_barrier(); /* 获取屏障，确保free操作在此之后 */
        return true;
    }

    if (old < 0 || old - i < 0)
        refcount_warn_saturate(r, REFCOUNT_SUB_UAF);

    return false;
}

/**
 * @brief 从引用计数减去值并测试是否为0
 * @param i 要减去的值
 * @param r 引用计数对象
 * @return 如果结果为0则返回true，否则返回false
 * 
 * @warning 不推荐在正常的引用计数场景中使用此函数
 */
static inline __attribute__((warn_unused_result)) bool 
refcount_sub_and_test(int i, refcount_t *r)
{
    return __refcount_sub_and_test(i, r, NULL);
}

/**
 * @brief 内部函数：减少引用计数并测试是否为0
 * @param r 引用计数对象
 * @param oldp 用于返回旧值的指针（可为NULL）
 * @return 如果结果为0则返回true，否则返回false
 */
static inline __attribute__((warn_unused_result)) bool 
__refcount_dec_and_test(refcount_t *r, int *oldp)
{
    return __refcount_sub_and_test(1, r, oldp);
}

/**
 * @brief 减少引用计数并测试是否为0
 * @param r 引用计数对象
 * @return 如果结果为0则返回true，否则返回false
 */
static inline __attribute__((warn_unused_result)) bool 
refcount_dec_and_test(refcount_t *r)
{
    return __refcount_dec_and_test(r, NULL);
}

/**
 * @brief 内部函数：减少引用计数
 * @param r 引用计数对象
 * @param oldp 用于返回旧值的指针（可为NULL）
 */
static inline void __refcount_dec(refcount_t *r, int *oldp)
{
    int old = atomic_fetch_sub(&r->refs, 1);

    if (oldp)
        *oldp = old;

    if (old <= 1)
        refcount_warn_saturate(r, REFCOUNT_DEC_LEAK);
}

/**
 * @brief 减少引用计数
 * @param r 引用计数对象
 */
static inline void refcount_dec(refcount_t *r)
{
    __refcount_dec(r, NULL);
}

/**
 * @brief 如果引用计数为1则减少到0
 * @param r 引用计数对象
 * @return 如果成功减少到0则返回true，否则返回false
 */
static inline bool refcount_dec_if_one(refcount_t *r)
{
    int val = 1;
    int expected = val;
    
    return atomic_compare_exchange_strong(&r->refs, &expected, 0);
}

/**
 * @brief 减少引用计数（除非它为1）
 * @param r 引用计数对象
 * @return 如果减少成功则返回true，否则返回false
 */
static inline bool refcount_dec_not_one(refcount_t *r)
{
    int new_val, val = refcount_read(r);

    do {
        if (val == REFCOUNT_SATURATED)
            return true;

        if (val == 1)
            return false;

        new_val = val - 1;
        if (new_val > val) {
            printf("refcount_t: underflow: use-after-free.\n");
            return true;
        }

    } while (!atomic_compare_exchange_strong(&r->refs, &val, new_val));

    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* _REFCOUNT_H_ */