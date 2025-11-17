/**
 * @file uref.h
 * @brief 用户态引用计数对象处理库
 * 
 * 基于Linux内核kref实现，用于处理通用引用计数对象
 */

#ifndef _UREF_H_
#define _UREF_H_

#include "vos.h"

/**
 * @brief 引用计数结构体
 * 
 * 用于管理对象的引用计数，当最后一个引用被释放时自动清理对象
 */
struct uref {
    refcount_t refcount; /**< 引用计数器 */
};

/** @brief 初始化uref的宏 */
#define UREF_INIT(n)    { .refcount = REFCOUNT_INIT(n), }

/**
 * @brief 初始化uref对象
 * @param uref 要初始化的uref对象
 * 
 * 将引用计数初始化为1
 */
static inline void uref_init(struct uref *uref)
{
    refcount_set(&uref->refcount, 1);
}

/**
 * @brief 读取uref的当前引用计数
 * @param uref uref对象
 * @return 当前的引用计数值
 */
static inline unsigned int uref_read(const struct uref *uref)
{
    return refcount_read(&uref->refcount);
}

/**
 * @brief 增加对象的引用计数
 * @param uref uref对象
 */
static inline void uref_get(struct uref *uref)
{
    refcount_inc(&uref->refcount);
}

/**
 * @brief 减少对象的引用计数，如果为0则释放对象
 * @param uref uref对象
 * @param release 释放对象的函数指针
 * 
 * 减少引用计数，如果引用计数变为0，则调用release()函数清理对象。
 * 注意：release参数是必需的，不能直接传递free函数。
 * 
 * @return 如果对象被移除返回1，否则返回0。
 *         注意：如果返回0，不能保证uref仍然在内存中。
 *         只有在想确认uref是否已经消失时才使用返回值。
 */
static inline int uref_put(struct uref *uref, void (*release)(struct uref *uref))
{
    if (refcount_dec_and_test(&uref->refcount)) {
        release(uref);
        return 1;
    }
    return 0;
}

/**
 * @brief 除非引用计数为0，否则增加引用计数
 * @param uref uref对象
 * 
 * 如果引用计数不为0，则增加引用计数。
 * 这个函数用于简化围绕引用计数的锁定，特别是对于可以从查找结构中查找的对象，
 * 这些对象在对象析构函数中从查找结构中移除。
 * 
 * @return 如果增加成功返回非零值，否则返回0
 */
static inline __attribute__((warn_unused_result)) 
int uref_get_unless_zero(struct uref *uref)
{
    return refcount_inc_not_zero(&uref->refcount);
}

#endif /* _UREF_H_ */