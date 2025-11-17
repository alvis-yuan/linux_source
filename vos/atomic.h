/**
 * @file atomic.h
 * @brief 基于GCC内置原子操作的用户态实现，接口与Linux内核保持一致
 * @note 此实现仅用于用户态程序，依赖于GCC的__atomic内置函数
 */

#ifndef _USER_ATOMIC_H
#define _USER_ATOMIC_H

#include "vos.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup atomic_api 原子操作接口
 * @{
 */

typedef struct {
    int counter;
} atomic_t;

/**
 * @brief 原子变量初始化器
 * @param i 初始值
 */
#define ATOMIC_INIT(i) { (i) }

/**
 * @brief 读取原子变量的值
 * @param v 原子变量指针
 * @return 当前值
 */
#define atomic_read(v) __atomic_load_n(&(v)->counter, __ATOMIC_RELAXED)

/**
 * @brief 设置原子变量的值
 * @param v 原子变量指针
 * @param i 要设置的值
 */
#define atomic_set(v, i) __atomic_store_n(&(v)->counter, i, __ATOMIC_RELAXED)

/**
 * @brief 原子加法
 * @param i 要加的值
 * @param v 原子变量指针
 */
#define atomic_add(i, v) __atomic_add_fetch(&(v)->counter, i, __ATOMIC_RELAXED)

/**
 * @brief 原子减法
 * @param i 要减的值
 * @param v 原子变量指针
 */
#define atomic_sub(i, v) __atomic_sub_fetch(&(v)->counter, i, __ATOMIC_RELAXED)

/**
 * @brief 原子自增
 * @param v 原子变量指针
 */
#define atomic_inc(v) atomic_add(1, v)

/**
 * @brief 原子自减
 * @param v 原子变量指针
 */
#define atomic_dec(v) atomic_sub(1, v)

/**
 * @brief 原子加法并返回新值
 * @param i 要加的值
 * @param v 原子变量指针
 * @return 加法后的新值
 */
#define atomic_add_return(i, v) __atomic_add_fetch(&(v)->counter, i, __ATOMIC_SEQ_CST)

/**
 * @brief 原子减法并返回新值
 * @param i 要减的值
 * @param v 原子变量指针
 * @return 减法后的新值
 */
#define atomic_sub_return(i, v) __atomic_sub_fetch(&(v)->counter, i, __ATOMIC_SEQ_CST)

/**
 * @brief 原子自增并返回新值
 * @param v 原子变量指针
 * @return 自增后的新值
 */
#define atomic_inc_return(v) atomic_add_return(1, v)

/**
 * @brief 原子自减并返回新值
 * @param v 原子变量指针
 * @return 自减后的新值
 */
#define atomic_dec_return(v) atomic_sub_return(1, v)

/**
 * @brief 原子加法并返回旧值
 * @param i 要加的值
 * @param v 原子变量指针
 * @return 加法前的旧值
 */
#define atomic_fetch_add(i, v) __atomic_fetch_add(&(v)->counter, i, __ATOMIC_SEQ_CST)

/**
 * @brief 原子减法并返回旧值
 * @param i 要减的值
 * @param v 原子变量指针
 * @return 减法前的旧值
 */
#define atomic_fetch_sub(i, v) __atomic_fetch_sub(&(v)->counter, i, __ATOMIC_SEQ_CST)

/**
 * @brief 原子自增并返回旧值
 * @param v 原子变量指针
 * @return 自增前的旧值
 */
#define atomic_fetch_inc(v) atomic_fetch_add(1, v)

/**
 * @brief 原子自减并返回旧值
 * @param v 原子变量指针
 * @return 自减前的旧值
 */
#define atomic_fetch_dec(v) atomic_fetch_sub(1, v)

/**
 * @brief 原子与操作
 * @param i 操作数
 * @param v 原子变量指针
 */
#define atomic_and(i, v) __atomic_and_fetch(&(v)->counter, i, __ATOMIC_RELAXED)

/**
 * @brief 原子或操作
 * @param i 操作数
 * @param v 原子变量指针
 */
#define atomic_or(i, v) __atomic_or_fetch(&(v)->counter, i, __ATOMIC_RELAXED)

/**
 * @brief 原子异或操作
 * @param i 操作数
 * @param v 原子变量指针
 */
#define atomic_xor(i, v) __atomic_xor_fetch(&(v)->counter, i, __ATOMIC_RELAXED)

/**
 * @brief 原子与操作并返回旧值
 * @param i 操作数
 * @param v 原子变量指针
 * @return 操作前的旧值
 */
#define atomic_fetch_and(i, v) __atomic_fetch_and(&(v)->counter, i, __ATOMIC_SEQ_CST)

/**
 * @brief 原子或操作并返回旧值
 * @param i 操作数
 * @param v 原子变量指针
 * @return 操作前的旧值
 */
#define atomic_fetch_or(i, v) __atomic_fetch_or(&(v)->counter, i, __ATOMIC_SEQ_CST)

/**
 * @brief 原子异或操作并返回旧值
 * @param i 操作数
 * @param v 原子变量指针
 * @return 操作前的旧值
 */
#define atomic_fetch_xor(i, v) __atomic_fetch_xor(&(v)->counter, i, __ATOMIC_SEQ_CST)

/**
 * @brief 原子比较交换
 * @param v 原子变量指针
 * @param old 期望的旧值
 * @param new 要设置的新值
 * @return 如果交换成功返回true，否则返回false
 */
#define atomic_cmpxchg(v, old, new) \
    __atomic_compare_exchange_n(&(v)->counter, &(old), new, 0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)

/**
 * @brief 原子交换
 * @param v 原子变量指针
 * @param new 新值
 * @return 交换前的旧值
 */
#define atomic_xchg(v, new) __atomic_exchange_n(&(v)->counter, new, __ATOMIC_SEQ_CST)

/**
 * @brief 原子变量减1后测试是否为0
 * @param v 原子变量指针
 * @return 如果减1后为0返回true，否则返回false
 */
#define atomic_dec_and_test(v) (atomic_sub_return(1, v) == 0)

/**
 * @brief 原子变量加1后测试是否为0
 * @param v 原子变量指针
 * @return 如果加1后为0返回true，否则返回false
 */
#define atomic_inc_and_test(v) (atomic_add_return(1, v) == 0)

/**
 * @brief 测试原子变量是否为负
 * @param v 原子变量指针
 * @return 如果值为负返回true，否则返回false
 */
#define atomic_sub_and_test(i, v) (atomic_sub_return(i, v) == 0)

/**
 * @brief 测试原子变量是否为0
 * @param v 原子变量指针
 * @return 如果值为0返回true，否则返回false
 */
#define atomic_test_and_set(v) (atomic_read(v) != 0)

/** @} */ // atomic_api

#ifdef __cplusplus
}
#endif

#endif /* _USER_ATOMIC_H */