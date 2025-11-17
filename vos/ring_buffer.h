#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief 泛型环形缓冲区结构体
 * 
 * 该结构体应该对用户透明，可以嵌入到更大的结构中
 */
typedef struct {
    void **buffer;          /**< 存储void*指针的数组 */
    size_t capacity;        /**< 队列容量 */
    size_t head;            /**< 队列头指针 */
    size_t tail;            /**< 队列尾指针 */
    size_t count;           /**< 当前元素数量 */
    size_t element_size;    /**< 每个元素的大小（字节） */
    pthread_mutex_t mutex;  /**< 互斥锁 */
    pthread_cond_t not_empty; /**< 条件变量：队列不为空 */
    pthread_cond_t not_full;  /**< 条件变量：队列不为满 */
} rb_t;

/**
 * @brief 初始化环形缓冲区
 * 
 * @param rb 环形缓冲区指针
 * @param capacity 缓冲区容量
 * @param element_size 每个元素的大小
 * @return int 0成功，-1失败
 */
int rb_init(rb_t *rb, size_t capacity, size_t element_size);

/**
 * @brief 销毁环形缓冲区，释放所有资源
 * 
 * @param rb 环形缓冲区指针
 */
void rb_destroy(rb_t *rb);

/**
 * @brief 判断缓冲区是否为空
 * 
 * @param rb 环形缓冲区指针
 * @return true 为空
 * @return false 不为空
 */
bool rb_is_empty(rb_t *rb);

/**
 * @brief 判断缓冲区是否已满
 * 
 * @param rb 环形缓冲区指针
 * @return true 已满
 * @return false 未满
 */
bool rb_is_full(rb_t *rb);

/**
 * @brief 获取缓冲区当前元素数量
 * 
 * @param rb 环形缓冲区指针
 * @return size_t 元素数量
 */
size_t rb_size(rb_t *rb);

/**
 * @brief 获取缓冲区容量
 * 
 * @param rb 环形缓冲区指针
 * @return size_t 容量
 */
size_t rb_capacity(rb_t *rb);

/**
 * @brief 阻塞式推送数据
 * 
 * @param rb 环形缓冲区指针
 * @param data 要推送的数据
 * @return int 1成功，0队列满，-1失败
 */
int rb_push_blocking(rb_t *rb, const void *data);

/**
 * @brief 非阻塞式推送数据
 * 
 * @param rb 环形缓冲区指针
 * @param data 要推送的数据
 * @return int 1成功，0队列满，-1失败
 */
int rb_push_nonblocking(rb_t *rb, const void *data);

/**
 * @brief 阻塞式弹出数据
 * 
 * @param rb 环形缓冲区指针
 * @param output 输出缓冲区
 * @return int 1成功，0队列空，-1失败
 */
int rb_pop_blocking(rb_t *rb, void *output);

/**
 * @brief 非阻塞式弹出数据
 * 
 * @param rb 环形缓冲区指针
 * @param output 输出缓冲区
 * @return int 1成功，0队列空，-1失败
 */
int rb_pop_nonblocking(rb_t *rb, void *output);

/**
 * @brief 查看队首元素（不弹出）
 * 
 * @param rb 环形缓冲区指针
 * @param output 输出缓冲区
 * @return int 1成功，0队列空，-1失败
 */
int rb_peek(rb_t *rb, void *output);

#endif /* RING_BUFFER_H */