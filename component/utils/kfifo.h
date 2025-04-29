#ifndef _USERMODE_KFIFO_H_
#define _USERMODE_KFIFO_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/* 内存屏障（用户态简化版） */
#if defined(__x86_64__)
#define smp_mb()    __asm__ __volatile__("mfence" ::: "memory")
#define smp_rmb()   __asm__ __volatile__("lfence" ::: "memory")
#define smp_wmb()   __asm__ __volatile__("sfence" ::: "memory")
#else
#define smp_mb()    __sync_synchronize()
#define smp_rmb()   __sync_synchronize()
#define smp_wmb()   __sync_synchronize()
#endif

#define min(a, b) ((a) < (b) ? (a) : (b))

/* 判断是否为2的幂次方 */
static inline bool is_power_of_2(uint32_t n) {
    return n && !(n & (n - 1));
}

/* 向上取整到2的幂次方 */
static inline uint32_t roundup_pow_of_two(uint32_t x) {
    if (x == 0) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

/* 环形队列结构体 */
struct kfifo {
    uint8_t *buffer;     /* 数据缓冲区 */
    uint32_t size;       /* 缓冲区大小（必须为2的幂） */
    uint32_t in;         /* 写入位置（自动溢出） */
    uint32_t out;        /* 读取位置（自动溢出） */
};

/* 初始化队列 */
static inline void kfifo_init(struct kfifo *fifo, uint8_t *buffer, uint32_t size) {
    fifo->buffer = buffer;
    fifo->size = size;
    fifo->in = fifo->out = 0;
}

/* 动态分配队列 */
static inline struct kfifo *kfifo_alloc(uint32_t size) {
    struct kfifo *fifo = malloc(sizeof(struct kfifo));
    if (!fifo) return NULL;

    size = roundup_pow_of_two(size);
    fifo->buffer = malloc(size);
    if (!fifo->buffer) {
        free(fifo);
        return NULL;
    }

    fifo->size = size;
    fifo->in = fifo->out = 0;
    return fifo;
}

/* 释放队列 */
static inline void kfifo_free(struct kfifo *fifo) {
    if (fifo) {
        free(fifo->buffer);
        free(fifo);
    }
}

/* 队列剩余空间 */
static inline uint32_t kfifo_avail(const struct kfifo *fifo) {
    return fifo->size - (fifo->in - fifo->out);
}

/* 队列已用空间 */
static inline uint32_t kfifo_len(const struct kfifo *fifo) {
    return fifo->in - fifo->out;
}

/* 入队（单生产者无需锁） */
static inline uint32_t kfifo_in(struct kfifo *fifo, const void *buf, uint32_t len) {
    uint32_t l;
    len = min(len, kfifo_avail(fifo));

    smp_mb(); /* 确保读取out值前完成所有内存操作 */

    /* 分段拷贝：从in位置到缓冲区末尾 */
    l = min(len, fifo->size - (fifo->in & (fifo->size - 1)));
    memcpy(fifo->buffer + (fifo->in & (fifo->size - 1)), buf, l);

    /* 剩余数据拷贝到缓冲区头部 */
    memcpy(fifo->buffer, (uint8_t *)buf + l, len - l);

    smp_wmb(); /* 确保数据写入后再更新in */
    fifo->in += len;
    return len;
}

/* 出队（单消费者无需锁） */
static inline uint32_t kfifo_out(struct kfifo *fifo, void *buf, uint32_t len) {
    uint32_t l;
    len = min(len, kfifo_len(fifo));

    smp_rmb(); /* 确保读取in值前完成所有内存操作 */

    /* 分段拷贝：从out位置到缓冲区末尾 */
    l = min(len, fifo->size - (fifo->out & (fifo->size - 1)));
    memcpy(buf, fifo->buffer + (fifo->out & (fifo->size - 1)), l);

    /* 剩余数据从缓冲区头部拷贝 */
    memcpy((uint8_t *)buf + l, fifo->buffer, len - l);

    smp_mb(); /* 确保数据读取后再更新out */
    fifo->out += len;
    return len;
}

#endif /* _USERMODE_KFIFO_H_ */