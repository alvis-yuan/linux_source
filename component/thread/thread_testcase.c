#if 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "thread.h"

/** 测试用例计数器 */
static int test_count = 0;
static int passed_count = 0;

#define MULTI_THREAD_DEADLOCK_TEST

/**
 * @brief 断言宏，用于测试验证
 */
#define TEST_ASSERT(condition, message) \
    do { \
        test_count++; \
        if (condition) { \
            printf("✓ TEST %d PASSED: %s\n", test_count, message); \
            passed_count++; \
        } else { \
            printf("✗ TEST %d FAILED: %s\n", test_count, message); \
        } \
    } while(0)

/**
 * @brief 测试用例1: 正常锁操作流程
 */
static void test_normal_lock_operations(void)
{
    printf("\n=== 测试1: 正常锁操作流程 ===\n");
    
    int ret;
    mutex_ctx_t *mutex = lock_init();
    
    // 初始化锁
    TEST_ASSERT(mutex != NULL, "锁初始化成功");
    
    // 获取锁
    ret = LOCK_ACQUIRE(mutex);
    TEST_ASSERT(ret == 0, "锁获取成功");
    
    // 查看锁状态
    lock_stat(mutex);
    
    // 释放锁
    ret = LOCK_RELEASE(mutex);
    TEST_ASSERT(ret == 0, "锁释放成功");
    
    // 销毁锁
    ret = lock_destroy(mutex);
    TEST_ASSERT(ret == 0, "锁销毁成功");
}


/**
 * @brief 测试用例4: 销毁还在持有的锁
 */
static void test_destroy_locked_mutex(void)
{
    printf("\n=== 测试2: 销毁还在持有的锁 ===\n");
    
    mutex_ctx_t *mutex = NULL;
    int ret = -1;
    
    // 初始化锁
    mutex = lock_init();
    TEST_ASSERT(mutex != NULL, "锁初始化成功");
    
    // 获取锁但不释放
    ret = LOCK_ACQUIRE(mutex);
    TEST_ASSERT(ret == 0, "锁获取成功");
    
    // 尝试销毁还在持有的锁
    ret = lock_destroy(mutex);
    TEST_ASSERT(ret == -1, "销毁持有的锁应失败");
    
    // 释放锁
    ret = LOCK_RELEASE(mutex);
    TEST_ASSERT(ret == 0, "锁释放成功");
    
    // 再次尝试销毁
    ret = lock_destroy(mutex);
    TEST_ASSERT(ret == 0, "锁销毁成功");
}

/**
 * @brief 测试用例5: 重入锁测试（递归锁）
 */
static void test_recursive_lock(void)
{
    printf("\n=== 测试3: 重入锁测试 ===\n");
    
    int ret = -1;
    mutex_ctx_t *mutex = lock_init();
    
    // 初始化锁
    TEST_ASSERT(mutex != NULL, "锁初始化成功");
    
    // 第一次获取锁
    ret = LOCK_ACQUIRE(mutex);
    TEST_ASSERT(ret == 0, "第一次锁获取成功");
    
#ifdef MUTEX_DEBUG
    // 第二次获取锁（重入）
    ret = LOCK_ACQUIRE(mutex);
    TEST_ASSERT(ret == -1, "第二次锁获取失败（重入）");
#endif
    
    // 查看锁状态
    lock_stat(mutex);
    
    // 第一次释放锁
    ret = LOCK_RELEASE(mutex);
    TEST_ASSERT(ret == 0, "第一次锁释放成功");
    
#ifdef MUTEX_DEBUG
    // 第二次释放锁
    ret = LOCK_RELEASE(mutex);
    TEST_ASSERT(ret == -1, "第二次锁释放失败");
#endif
    
    // 销毁锁
    ret = lock_destroy(mutex);
    TEST_ASSERT(ret == 0, "锁销毁成功");
}

/**
 * @brief 测试用例6: 多线程死锁场景
 */
typedef struct {
    mutex_ctx_t *mutex1;
    mutex_ctx_t *mutex2;
    int thread_id;
} thread_args_t;

static void* deadlock_thread_func(void *arg)
{
    thread_args_t *args = (thread_args_t *)arg;
    
#ifdef MULTI_THREAD_DEADLOCK_TEST
    if (args->thread_id == 1) {
        // 线程1: 先锁mutex1，再锁mutex2
        LOCK_ACQUIRE(args->mutex1);
        printf("线程1获取了mutex1\n");
        sleep(1);  // 让线程2有机会获取mutex2
        LOCK_ACQUIRE(args->mutex2);  // 这里会死锁
        printf("线程1获取了mutex2\n");
        LOCK_RELEASE(args->mutex2);
        LOCK_RELEASE(args->mutex1);
    } else {
        // 线程2: 先锁mutex2，再锁mutex1
        LOCK_ACQUIRE(args->mutex2);
        printf("线程2获取了mutex2\n");
        sleep(1);  // 让线程1有机会获取mutex1
        LOCK_ACQUIRE(args->mutex1);  // 这里会死锁
        printf("线程2获取了mutex1\n");
        LOCK_RELEASE(args->mutex1);
        LOCK_RELEASE(args->mutex2);
    }
#else
    LOCK_ACQUIRE(args->mutex1);
    printf("线程%d获取了mutex1\n", args->thread_id);
    sleep(1);  // 让线程2有机会获取mutex2
    LOCK_ACQUIRE(args->mutex2);  // 这里会死锁
    printf("线程%d获取了mutex2\n", args->thread_id);
    LOCK_RELEASE(args->mutex2);
    LOCK_RELEASE(args->mutex1);
#endif
    
    return NULL;
}

static void test_deadlock_scenario(void)
{
    printf("\n=== 测试4: 多线程死锁场景 ===\n");
    
    mutex_ctx_t *mutex1 = NULL;
    mutex_ctx_t *mutex2 = NULL;
    pthread_t thread1, thread2;
    thread_args_t args1, args2;
    int ret;
    
    // 初始化两个锁
    mutex1 = lock_init();
    TEST_ASSERT(mutex1 != NULL, "mutex1初始化成功");
    
    mutex2 = lock_init();
    TEST_ASSERT(mutex2 != NULL, "mutex2初始化成功");
    
    // 设置线程参数
    args1.mutex1 = mutex1;
    args1.mutex2 = mutex2;
    args1.thread_id = 1;
    
    args2.mutex1 = mutex1;
    args2.mutex2 = mutex2;
    args2.thread_id = 2;
    
    printf("创建两个线程模拟死锁场景（需要手动终止）\n");
    
    // 创建线程
    #if 0
    pthread_create(&thread1, NULL, deadlock_thread_func, &args1);
    pthread_create(&thread2, NULL, deadlock_thread_func, &args2);
    #else
    ret = THREAD_CREATE(&thread1, NULL, deadlock_thread_func, &args1);
    TEST_ASSERT(ret == 0, "线程1创建成功");
    
    ret = THREAD_CREATE(&thread2, NULL, deadlock_thread_func, &args2);
    TEST_ASSERT(ret == 0, "线程2创建成功");
    #endif
    
#ifdef MULTI_THREAD_DEADLOCK_TEST
    // 等待一段时间让死锁发生
    sleep(3);
    
    // 尝试join线程（会超时）
    printf("检测到可能的死锁，测试结束\n");
    
    // 强制终止线程（在实际测试中可能需要）
    pthread_cancel(thread1);
    pthread_cancel(thread2);
#else
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
#endif

    // 清理
    lock_destroy(mutex1);
    lock_destroy(mutex2);
}

/**
 * @brief 测试用例7: 参数有效性检查
 */
static void test_parameter_validation(void)
{
    printf("\n=== 测试5: 参数有效性检查 ===\n");
    
    int ret;
    
    // 测试NULL参数
    ret = lock_acquire(NULL, __FILE__, __func__, __LINE__);
    TEST_ASSERT(ret == -1, "NULL参数加锁应失败");
    
    ret = lock_release(NULL, __FILE__, __func__, __LINE__);
    TEST_ASSERT(ret == -1, "NULL参数解锁应失败");
    
    ret = lock_destroy(NULL);
    TEST_ASSERT(ret == -1, "NULL参数销毁应失败");
    
    lock_stat(NULL);  // 应该输出错误信息但不崩溃
}

/**
 * @brief 测试用例8: 锁状态查询
 */
static void test_lock_status(void)
{
    printf("\n=== 测试6: 锁状态查询 ===\n");
    
    mutex_ctx_t *mutex = NULL;
    int ret;
    
    // 初始化锁
    mutex = lock_init();
    TEST_ASSERT(mutex != NULL, "锁初始化成功");
    
    // 查看未锁定的状态
    printf("未锁定状态:\n");
    lock_stat(mutex);
    
    // 锁定后查看状态
    LOCK_ACQUIRE(mutex);
    printf("锁定后状态:\n");
    lock_stat(mutex);
    
    // 释放后查看状态
    LOCK_RELEASE(mutex);
    printf("释放后状态:\n");
    lock_stat(mutex);
    
    // 销毁锁
    ret = lock_destroy(mutex);
    TEST_ASSERT(ret == 0, "锁销毁成功");
}

/**
 * @brief 测试用例9: 加锁未初始化锁
 */
static void test_acquire_uninitialized_mutex(void)
{
    printf("\n=== 测试7: 加锁未初始化锁 ===\n");
    
    mutex_ctx_t *mutex = NULL;
    int ret;

    mutex = lock_init();
    
    // 尝试加锁未初始化的锁
    ret = lock_acquire(mutex, __FILE__, __func__, __LINE__);
    TEST_ASSERT(ret == -1, "未初始化锁加锁应失败");
}

/**
 * @brief 测试用例10: 解锁未初始化锁
 */
static void test_release_uninitialized_mutex(void)
{
    printf("\n=== 测试8: 解锁未初始化锁 ===\n");
    
    mutex_ctx_t *mutex = NULL;
    int ret;

    mutex = lock_init();
    
    // 尝试解锁未初始化的锁
    ret = lock_release(mutex, __FILE__, __func__, __LINE__);
    TEST_ASSERT(ret == -1, "未初始化锁解锁应失败");
}

/**
 * @brief 主测试函数
 */
int main(void)
{
    printf("开始线程操作接口测试...\n");
    printf("编译配置: MUTEX_DEBUG %s\n", 
#ifdef MUTEX_DEBUG
           "已启用"
#else
           "未启用"
#endif
    );
    
    // 运行所有测试用例
    #if 1
    test_normal_lock_operations();
    test_destroy_locked_mutex();
    test_recursive_lock();
    test_deadlock_scenario();
    test_parameter_validation();
    test_lock_status();
    #else
    test_acquire_uninitialized_mutex();
    test_release_uninitialized_mutex();
    #endif
    
    // 输出测试结果摘要
    printf("\n=== 测试结果摘要 ===\n");
    printf("总测试数: %d\n", test_count);
    printf("通过数: %d\n", passed_count);
    printf("失败数: %d\n", test_count - passed_count);
    printf("通过率: %.1f%%\n", (float)passed_count / test_count * 100);
    
    if (passed_count == test_count) {
        printf("🎉 所有测试通过！\n");
        return 0;
    } else {
        printf("❌ 有测试失败，请检查实现\n");
        return 1;
    }
}
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "thread.h"

/** 测试用例计数器 */
static int test_count = 0;
static int passed_count = 0;

/**
 * @brief 断言宏，用于测试验证
 */
#define TEST_ASSERT(condition, message) \
    do { \
        test_count++; \
        if (condition) { \
            printf("✓ TEST %d PASSED: %s\n", test_count, message); \
            passed_count++; \
        } else { \
            printf("✗ TEST %d FAILED: %s\n", test_count, message); \
        } \
    } while(0)

/** 全局测试变量 */
static cond_ctx_t *g_test_cond = NULL;
static int g_shared_data = 0;
static int g_threads_ready = 0;
static int g_threads_completed = 0;

/**
 * @brief 测试用例1: 正常条件变量初始化
 */
void test_cond_var_init_normal(void)
{
    printf("\n=== 测试1: 正常条件变量初始化 ===\n");
    
    cond_ctx_t *cond = cond_var_init();
    TEST_ASSERT(cond != NULL, "条件变量初始化成功");
    
    if (cond != NULL) {
        cond_var_destroy(cond);
    }
}

/**
 * @brief 测试用例2: 条件变量内存分配失败
 */
void test_cond_var_init_malloc_fail(void)
{
    printf("\n=== 测试2: 条件变量内存分配失败 ===\n");
    
    // 这个测试较难模拟，主要是验证错误处理逻辑
    // 在实际测试中可以通过内存限制工具来测试
    printf("注意: 内存分配失败测试需要特殊环境\n");
}

/**
 * @brief 测试用例3: 等待线程函数
 */
static void* wait_thread_func(void *arg)
{
    cond_ctx_t *cond = (cond_ctx_t *)arg;
    
    __sync_fetch_and_add(&g_threads_ready, 1);
    
    printf("等待线程[%ld] 开始等待条件变量\n", syscall(SYS_gettid));
    int ret = cond_var_wait(cond);
    if (ret == 0) {
        printf("等待线程[%ld] 被唤醒\n", syscall(SYS_gettid));
        __sync_fetch_and_add(&g_threads_completed, 1);
    } else {
        printf("等待线程[%ld] 等待失败: %d\n", syscall(SYS_gettid), ret);
    }
    
    return NULL;
}

/**
 * @brief 测试用例3: 条件变量等待和信号测试
 */
void test_cond_var_wait_signal(void)
{
    printf("\n=== 测试3: 条件变量等待和信号测试 ===\n");
    
    pthread_t wait_thread;
    int ret;
    
    g_test_cond = cond_var_init();
    TEST_ASSERT(g_test_cond != NULL, "条件变量初始化");
    
    g_threads_ready = 0;
    g_threads_completed = 0;
    
    // 创建等待线程
    ret = pthread_create(&wait_thread, NULL, wait_thread_func, g_test_cond);
    TEST_ASSERT(ret == 0, "创建等待线程");
    
    // 等待线程就绪
    while (g_threads_ready < 1) {
        usleep(1000);
    }
    
    sleep(1); // 确保等待线程进入等待状态
    
    printf("主线程发送信号唤醒等待线程\n");
    ret = cond_var_signal(g_test_cond);
    TEST_ASSERT(ret == 0, "发送信号成功");
    
    // 等待线程完成
    pthread_join(wait_thread, NULL);
    
    TEST_ASSERT(g_threads_completed == 1, "等待线程成功被唤醒");
    
    cond_var_destroy(g_test_cond);
    g_test_cond = NULL;
}

/**
 * @brief 测试用例4: 条件变量广播测试
 */
static void* multi_wait_thread_func(void *arg)
{
    int thread_id = *(int*)arg;
    cond_ctx_t *cond = g_test_cond;
    
    __sync_fetch_and_add(&g_threads_ready, 1);
    
    printf("等待线程%d[%ld] 开始等待条件变量\n", thread_id, syscall(SYS_gettid));
    int ret = cond_var_wait(cond);
    if (ret == 0) {
        printf("等待线程%d[%ld] 被广播唤醒\n", thread_id, syscall(SYS_gettid));
        __sync_fetch_and_add(&g_threads_completed, 1);
    } else {
        printf("等待线程%d[%ld] 等待失败: %d\n", thread_id, syscall(SYS_gettid), ret);
    }
    
    return NULL;
}

void test_cond_var_broadcast(void)
{
    printf("\n=== 测试4: 条件变量广播测试 ===\n");
    
    pthread_t threads[3];
    int thread_ids[3] = {1, 2, 3};
    int ret;
    
    g_test_cond = cond_var_init();
    TEST_ASSERT(g_test_cond != NULL, "条件变量初始化");
    
    g_threads_ready = 0;
    g_threads_completed = 0;
    
    // 创建多个等待线程
    for (int i = 0; i < 3; i++) {
        ret = THREAD_CREATE(&threads[i], NULL, multi_wait_thread_func, &thread_ids[i]);
    }
    
    // 等待所有线程就绪
    while (g_threads_ready < 3) {
        usleep(1000);
    }
    
    sleep(1); // 确保所有线程进入等待状态
    
    printf("主线程发送广播唤醒所有等待线程\n");
    ret = cond_var_broadcast(g_test_cond);
    TEST_ASSERT(ret == 0, "发送广播成功");
    
    // 等待所有线程完成
    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }
    
    TEST_ASSERT(g_threads_completed == 3, "所有等待线程成功被唤醒");
    
    cond_var_destroy(g_test_cond);
    g_test_cond = NULL;
}

/**
 * @brief 测试用例5: 生产者消费者模式测试
 */
static void* producer_thread_func(void *arg)
{
    cond_ctx_t *cond = (cond_ctx_t *)arg;
    
    sleep(1); // 模拟生产时间
    
    pthread_mutex_lock(&cond->mutex);
    g_shared_data = 42; // 生产数据
    printf("生产者生产数据: %d\n", g_shared_data);
    pthread_mutex_unlock(&cond->mutex);
    
    // 通知消费者
    cond_var_signal(cond);
    
    return NULL;
}

static void* consumer_thread_func(void *arg)
{
    cond_ctx_t *cond = (cond_ctx_t *)arg;
    
    cond_var_wait(cond); // 等待生产者信号
    
    pthread_mutex_lock(&cond->mutex);
    printf("消费者消费数据: %d\n", g_shared_data);
    int data = g_shared_data;
    pthread_mutex_unlock(&cond->mutex);
    
    TEST_ASSERT(data == 42, "生产者消费者数据传递正确");
    
    return NULL;
}

void test_producer_consumer_pattern(void)
{
    printf("\n=== 测试5: 生产者消费者模式测试 ===\n");
    
    pthread_t producer, consumer;
    int ret;
    
    cond_ctx_t *cond = cond_var_init();
    TEST_ASSERT(cond != NULL, "条件变量初始化");
    
    g_shared_data = 0;
    
    // 创建消费者线程（先等待）
    ret = pthread_create(&consumer, NULL, consumer_thread_func, cond);
    TEST_ASSERT(ret == 0, "创建消费者线程");
    
    // 创建生产者线程
    ret = pthread_create(&producer, NULL, producer_thread_func, cond);
    TEST_ASSERT(ret == 0, "创建生产者线程");
    
    // 等待线程完成
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    
    cond_var_destroy(cond);
}

/**
 * @brief 测试用例6: 参数有效性检查
 */
void test_parameter_validation(void)
{
    printf("\n=== 测试6: 参数有效性检查 ===\n");
    
    int ret;
    
    // 测试NULL参数
    ret = cond_var_wait(NULL);
    TEST_ASSERT(ret == -1, "NULL参数等待应失败");
    
    ret = cond_var_signal(NULL);
    TEST_ASSERT(ret == -1, "NULL参数信号应失败");
    
    ret = cond_var_broadcast(NULL);
    TEST_ASSERT(ret == -1, "NULL参数广播应失败");
    
    ret = cond_var_destroy(NULL);
    TEST_ASSERT(ret == -1, "NULL参数销毁应失败");
}

/**
 * @brief 测试用例7: 条件变量超时测试（需要扩展接口支持超时）
 */
void test_cond_var_timeout(void)
{
    printf("\n=== 测试7: 条件变量超时测试 ===\n");
    
    printf("注意: 当前接口不支持超时，需要扩展pthread_cond_timedwait\n");
    printf("建议添加cond_var_timedwait接口支持超时等待\n");
}

/**
 * @brief 测试用例8: 多次销毁测试
 */
void test_multiple_destroy(void)
{
    printf("\n=== 测试8: 多次销毁测试 ===\n");
    
    cond_ctx_t *cond = cond_var_init();
    TEST_ASSERT(cond != NULL, "条件变量初始化");
    
    // 第一次销毁
    int ret = cond_var_destroy(cond);
    TEST_ASSERT(ret == 0, "第一次销毁成功");
    
    // 第二次销毁（应该失败或无害）
    // 注意: 这里的行为取决于实现，可能崩溃或返回错误
    printf("注意: 重复销毁测试，可能产生未定义行为\n");
}

/**
 * @brief 测试用例9: 竞争条件测试
 */
static void* race_condition_thread_func(void *arg)
{
    cond_ctx_t *cond = (cond_ctx_t *)arg;
    
    for (int i = 0; i < 10; i++) {
        cond_var_signal(cond);
        usleep(1000); // 微小延迟增加竞争可能性
    }
    
    return NULL;
}

void test_race_condition(void)
{
    printf("\n=== 测试9: 竞争条件测试 ===\n");
    
    pthread_t threads[5];
    int ret;
    
    cond_ctx_t *cond = cond_var_init();
    TEST_ASSERT(cond != NULL, "条件变量初始化");
    
    // 创建多个线程同时发送信号
    for (int i = 0; i < 5; i++) {
        ret = pthread_create(&threads[i], NULL, race_condition_thread_func, cond);
        TEST_ASSERT(ret == 0, "创建竞争线程%d", i+1);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("竞争条件测试完成，无死锁或崩溃\n");
    
    cond_var_destroy(cond);
}

/**
 * @brief 主测试函数
 */
int main(void)
{
    printf("开始条件变量接口测试...\n");
    
    // 运行所有测试用例
    test_cond_var_init_normal();
    test_cond_var_init_malloc_fail();
    test_cond_var_wait_signal();
    test_cond_var_broadcast();
    test_producer_consumer_pattern();
    test_parameter_validation();
    test_cond_var_timeout();
    test_multiple_destroy();
    test_race_condition();
    
    // 输出测试结果摘要
    printf("\n=== 测试结果摘要 ===\n");
    printf("总测试数: %d\n", test_count);
    printf("通过数: %d\n", passed_count);
    printf("失败数: %d\n", test_count - passed_count);
    printf("通过率: %.1f%%\n", (float)passed_count / test_count * 100);
    
    if (passed_count == test_count) {
        printf("🎉 所有测试通过！\n");
        return 0;
    } else {
        printf("❌ 有测试失败，请检查实现\n");
        return 1;
    }
}
#endif