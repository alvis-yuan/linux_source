#ifndef _ATOMIC_H_
#define _ATOMIC_H_

#include <stdint.h>

/**
 * @file atomic.h
 * @brief 原子操作封装，基于GCC __sync内置函数
 * 
 * 提供类似C11标准的原子操作接口，兼容C99之前的标准
 */

typedef int atomic_int;
typedef unsigned int atomic_uint;
typedef int32_t atomic_int32;
typedef uint32_t atomic_uint32;
typedef int64_t atomic_int64;
typedef uint64_t atomic_uint64;
typedef void* atomic_ptr;

/* 内存顺序（简化版本，实际GCC __sync函数使用顺序一致性） */
#define memory_order_relaxed 0
#define memory_order_acquire 1
#define memory_order_release 2
#define memory_order_acq_rel 3
#define memory_order_seq_cst 4

/**
 * @brief 原子加载（带内存顺序）
 */
#define atomic_load(ptr) (*(ptr))

/**
 * @brief 原子存储（带内存顺序）
 */
#define atomic_store(ptr, value) do { \
    __sync_synchronize(); \
    *(ptr) = (value); \
    __sync_synchronize(); \
} while (0)

/**
 * @brief 原子交换
 */
#define atomic_exchange(ptr, value) __sync_lock_test_and_set(ptr, value)

/**
 * @brief 比较并交换（CAS）
 */
#define atomic_compare_exchange_strong(ptr, expected, desired) \
    __sync_bool_compare_and_swap(ptr, *(expected), desired) ? \
    (*(expected) = *(expected), 1) : (*(expected) = *(ptr), 0)

/* ========== 整数原子操作 ========== */

/**
 * @brief 原子获取并加
 */
#define atomic_fetch_add(ptr, value) __sync_fetch_and_add(ptr, value)

/**
 * @brief 原子获取并减
 */
#define atomic_fetch_sub(ptr, value) __sync_fetch_and_sub(ptr, value)

/**
 * @brief 原子获取并或
 */
#define atomic_fetch_or(ptr, value) __sync_fetch_and_or(ptr, value)

/**
 * @brief 原子获取并与
 */
#define atomic_fetch_and(ptr, value) __sync_fetch_and_and(ptr, value)

/**
 * @brief 原子获取并异或
 */
#define atomic_fetch_xor(ptr, value) __sync_fetch_and_xor(ptr, value)

/* ========== 返回新值的原子操作 ========== */

/**
 * @brief 原子加并获取
 */
#define atomic_add_fetch(ptr, value) __sync_add_and_fetch(ptr, value)

/**
 * @brief 原子减并获取
 */
#define atomic_sub_fetch(ptr, value) __sync_sub_and_fetch(ptr, value)

/**
 * @brief 原子或并获取
 */
#define atomic_or_fetch(ptr, value) __sync_or_and_fetch(ptr, value)

/**
 * @brief 原子与并获取
 */
#define atomic_and_fetch(ptr, value) __sync_and_and_fetch(ptr, value)

/**
 * @brief 原子异或并获取
 */
#define atomic_xor_fetch(ptr, value) __sync_xor_and_fetch(ptr, value)

/* ========== 位操作原子操作 ========== */

/**
 * @brief 原子位测试并设置
 */
#define atomic_bit_test_and_set(ptr, bit) \
    __sync_fetch_and_or(ptr, (1U << (bit)))

/**
 * @brief 原子位测试并清除
 */
#define atomic_bit_test_and_clear(ptr, bit) \
    __sync_fetch_and_and(ptr, ~(1U << (bit)))

/**
 * @brief 原子位测试并翻转
 */
#define atomic_bit_test_and_toggle(ptr, bit) \
    __sync_fetch_and_xor(ptr, (1U << (bit)))

/* ========== 内存屏障 ========== */

/**
 * @brief 编译器内存屏障
 */
#define compiler_barrier() asm volatile("" ::: "memory")

/**
 * @brief 完整内存屏障
 */
#define memory_barrier() __sync_synchronize()

/**
 * @brief 读内存屏障（获取屏障）
 */
#define read_barrier() __sync_synchronize()

/**
 * @brief 写内存屏障（释放屏障）
 */
#define write_barrier() __sync_synchronize()

/* ========== 高级操作 ========== */

/**
 * @brief 原子自增并返回旧值
 */
#define atomic_fetch_inc(ptr) atomic_fetch_add(ptr, 1)

/**
 * @brief 原子自减并返回旧值
 */
#define atomic_fetch_dec(ptr) atomic_fetch_sub(ptr, 1)

/**
 * @brief 原子自增并返回新值
 */
#define atomic_inc_fetch(ptr) atomic_add_fetch(ptr, 1)

/**
 * @brief 原子自减并返回新值
 */
#define atomic_dec_fetch(ptr) atomic_sub_fetch(ptr, 1)

/**
 * @brief 如果值为0则设置为1（类似尝试获取锁）
 */
static inline int atomic_try_acquire(atomic_int *ptr) {
    return __sync_bool_compare_and_swap(ptr, 0, 1);
}

/**
 * @brief 释放（设置为0）
 */
static inline void atomic_release(atomic_int *ptr) {
    __sync_lock_release(ptr);
}

#endif /* _ATOMIC_H_ */
