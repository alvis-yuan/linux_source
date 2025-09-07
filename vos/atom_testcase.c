#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include "atomic.h"

#define TEST_ASSERT(expr, msg) \
    do { \
        if (!(expr)) { \
            printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            exit(1); \
        } else { \
            printf("PASS: %s\n", msg); \
        } \
    } while (0)

/* 测试共享变量 */
static atomic_int g_counter = 0;
static atomic_uint g_uint_counter = 0;
static atomic_int32 g_int32_counter = 0;
static atomic_uint32 g_uint32_counter = 0;
static atomic_int64 g_int64_counter = 0;
static atomic_uint64 g_uint64_counter = 0;
static atomic_ptr g_ptr = NULL;

/* 多线程测试辅助结构 */
#define NUM_THREADS 10
#define OPERATIONS_PER_THREAD 10000

static void* test_add_thread(void* arg) {
    for (int i = 0; i < OPERATIONS_PER_THREAD; i++) {
        atomic_fetch_add(&g_counter, 1);
        atomic_fetch_add(&g_uint_counter, 1);
        atomic_fetch_add(&g_int32_counter, 1);
        atomic_fetch_add(&g_uint32_counter, 1);
        atomic_fetch_add((atomic_int64*)&g_int64_counter, 1);
        atomic_fetch_add((atomic_uint64*)&g_uint64_counter, 1);
    }
    return NULL;
}

static void* test_bitops_thread(void* arg) {
    for (int i = 0; i < OPERATIONS_PER_THREAD; i++) {
        atomic_fetch_or(&g_uint32_counter, 0x1);
        atomic_fetch_and(&g_uint32_counter, ~0x2);
        atomic_fetch_xor(&g_uint32_counter, 0x4);
    }
    return NULL;
}

/* 测试基本原子操作 */
void test_basic_operations() {
    printf("=== Testing Basic Operations ===\n");
    
    atomic_int a = 0;
    atomic_uint b = 0;
    
    /* 测试 atomic_store 和 atomic_load */
    atomic_store(&a, 42);
    TEST_ASSERT(atomic_load(&a) == 42, "atomic_store/load int");
    
    atomic_store(&b, 100);
    TEST_ASSERT(atomic_load(&b) == 100, "atomic_store/load uint");
    
    /* 测试 atomic_exchange */
    int old = atomic_exchange(&a, 99);
    TEST_ASSERT(old == 42 && a == 99, "atomic_exchange");
    
    /* 测试 atomic_compare_exchange_strong */
    int expected = 99;
    int success = atomic_compare_exchange_strong(&a, &expected, 123);
    TEST_ASSERT(success && a == 123, "CAS success case");
    
    expected = 999; // Wrong expected value
    success = atomic_compare_exchange_strong(&a, &expected, 456);
    TEST_ASSERT(!success && expected == 123 && a == 123, "CAS fail case");
}

/* 测试算术操作 */
void test_arithmetic_operations() {
    printf("\n=== Testing Arithmetic Operations ===\n");
    
    atomic_int a = 10;
    atomic_uint b = 10;
    
    /* 测试 fetch_and_add/sub */
    TEST_ASSERT(atomic_fetch_add(&a, 5) == 10 && a == 15, "atomic_fetch_add");
    TEST_ASSERT(atomic_fetch_sub(&a, 3) == 15 && a == 12, "atomic_fetch_sub");
    
    TEST_ASSERT(atomic_fetch_add(&b, 5) == 10 && b == 15, "atomic_fetch_add uint");
    TEST_ASSERT(atomic_fetch_sub(&b, 3) == 15 && b == 12, "atomic_fetch_sub uint");
    
    /* 测试 add/sub_fetch */
    TEST_ASSERT(atomic_add_fetch(&a, 3) == 15, "atomic_add_fetch");
    TEST_ASSERT(atomic_sub_fetch(&a, 5) == 10, "atomic_sub_fetch");
    
    /* 测试 inc/dec 操作 */
    TEST_ASSERT(atomic_fetch_inc(&a) == 10 && a == 11, "atomic_fetch_inc");
    TEST_ASSERT(atomic_fetch_dec(&a) == 11 && a == 10, "atomic_fetch_dec");
    
    TEST_ASSERT(atomic_inc_fetch(&a) == 11, "atomic_inc_fetch");
    TEST_ASSERT(atomic_dec_fetch(&a) == 10, "atomic_dec_fetch");
}

/* 测试位操作 */
void test_bit_operations() {
    printf("\n=== Testing Bit Operations ===\n");
    
    atomic_uint value = 0;
    
    /* 测试位操作 */
    TEST_ASSERT(atomic_fetch_or(&value, 0x3) == 0 && value == 0x3, "atomic_fetch_or");
    TEST_ASSERT(atomic_fetch_and(&value, 0x1) == 0x3 && value == 0x1, "atomic_fetch_and");
    TEST_ASSERT(atomic_fetch_xor(&value, 0x3) == 0x1 && value == 0x2, "atomic_fetch_xor");
    
    /* 测试 or/and/xor_fetch */
    TEST_ASSERT(atomic_or_fetch(&value, 0x1) == 0x3, "atomic_or_fetch");
    TEST_ASSERT(atomic_and_fetch(&value, 0x1) == 0x1, "atomic_and_fetch");
    TEST_ASSERT(atomic_xor_fetch(&value, 0x3) == 0x2, "atomic_xor_fetch");
    
    /* 测试位测试和设置操作 */
    value = 0;
    TEST_ASSERT(atomic_bit_test_and_set(&value, 3) == 0 && value == 0x8, "atomic_bit_test_and_set");
    TEST_ASSERT(atomic_bit_test_and_clear(&value, 3) == 0x8 && value == 0, "atomic_bit_test_and_clear");
    TEST_ASSERT(atomic_bit_test_and_toggle(&value, 2) == 0 && value == 0x4, "atomic_bit_test_and_toggle");
    TEST_ASSERT(atomic_bit_test_and_toggle(&value, 2) == 0x4 && value == 0, "atomic_bit_test_and_toggle");
}

/* 测试内存屏障 */
void test_memory_barriers() {
    printf("\n=== Testing Memory Barriers ===\n");
    
    int x = 0, y = 0;
    
    /* 编译器屏障测试 */
    x = 1;
    compiler_barrier();
    y = 2;
    
    /* 完整内存屏障 */
    memory_barrier();
    
    /* 读写屏障 */
    read_barrier();
    write_barrier();
    
    TEST_ASSERT(1, "memory barriers completed");
}

/* 测试指针操作 */
void test_pointer_operations() {
    printf("\n=== Testing Pointer Operations ===\n");
    
    int data1 = 100, data2 = 200;
    atomic_ptr ptr = NULL;
    
    atomic_store(&ptr, &data1);
    TEST_ASSERT(atomic_load(&ptr) == &data1, "atomic pointer store/load");
    
    void* old_ptr = atomic_exchange(&ptr, &data2);
    TEST_ASSERT(old_ptr == &data1 && atomic_load(&ptr) == &data2, "atomic pointer exchange");
    
    void* expected = &data2;
    int success = atomic_compare_exchange_strong(&ptr, &expected, &data1);
    TEST_ASSERT(success && atomic_load(&ptr) == &data1, "atomic pointer CAS");
}

/* 测试高级操作 */
void test_advanced_operations() {
    printf("\n=== Testing Advanced Operations ===\n");
    
    atomic_int lock = 0;
    
    /* 测试尝试获取 */
    TEST_ASSERT(atomic_try_acquire(&lock), "atomic_try_acquire first attempt");
    TEST_ASSERT(!atomic_try_acquire(&lock), "atomic_try_acquire second attempt (should fail)");
    
    /* 测试释放 */
    atomic_release(&lock);
    TEST_ASSERT(atomic_load(&lock) == 0, "atomic_release");
    TEST_ASSERT(atomic_try_acquire(&lock), "atomic_try_acquire after release");
    
    atomic_release(&lock);
}

/* 测试多线程安全性 */
void test_thread_safety() {
    printf("\n=== Testing Thread Safety ===\n");
    
    pthread_t threads[NUM_THREADS];
    
    /* 重置计数器 */
    g_counter = 0;
    g_uint_counter = 0;
    g_int32_counter = 0;
    g_uint32_counter = 0;
    g_int64_counter = 0;
    g_uint64_counter = 0;
    
    /* 创建线程进行加法操作 */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, test_add_thread, NULL);
    }
    
    /* 等待所有线程完成 */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* 验证结果 */
    int expected = NUM_THREADS * OPERATIONS_PER_THREAD;
    TEST_ASSERT(atomic_load(&g_counter) == expected, "thread-safe int counter");
    TEST_ASSERT(atomic_load(&g_uint_counter) == (unsigned int)expected, "thread-safe uint counter");
    TEST_ASSERT(atomic_load(&g_int32_counter) == expected, "thread-safe int32 counter");
    TEST_ASSERT(atomic_load(&g_uint32_counter) == (uint32_t)expected, "thread-safe uint32 counter");
    TEST_ASSERT(atomic_load((atomic_int64*)&g_int64_counter) == (int64_t)expected, "thread-safe int64 counter");
    TEST_ASSERT(atomic_load((atomic_uint64*)&g_uint64_counter) == (uint64_t)expected, "thread-safe uint64 counter");
    
    printf("Final counter values: %d (expected: %d)\n", atomic_load(&g_counter), expected);
}

/* 测试边界条件 */
void test_edge_cases() {
    printf("\n=== Testing Edge Cases ===\n");
    
    /* 测试最大值边界 */
    atomic_int max_val = INT_MAX;
    atomic_fetch_inc(&max_val);
    TEST_ASSERT(max_val == INT_MIN, "integer overflow handling");
    
    /* 测试零值 */
    atomic_int zero = 0;
    TEST_ASSERT(atomic_fetch_dec(&zero) == 0 && zero == -1, "decrement from zero");
    
    /* 测试负值 */
    atomic_int neg = -5;
    TEST_ASSERT(atomic_fetch_add(&neg, 10) == -5 && neg == 5, "add to negative value");
    
    /* 测试所有位操作 */
    atomic_uint all_ones = ~0U;
    atomic_fetch_and(&all_ones, 0);
    TEST_ASSERT(all_ones == 0, "clear all bits");
    
    atomic_fetch_or(&all_ones, ~0U);
    TEST_ASSERT(all_ones == ~0U, "set all bits");
}

int main() {
    printf("Starting atomic operations tests...\n");

    atomic_int rc = 15;
    atomic_store(&rc, 0);
    atomic_fetch_add(&rc, 1);
    printf("rc: %d\n", atomic_load(&rc));
    
    test_basic_operations();
    test_arithmetic_operations();
    test_bit_operations();
    test_memory_barriers();
    test_pointer_operations();
    test_advanced_operations();
    test_edge_cases();
    test_thread_safety();
    
    printf("\n=== All Tests Passed! ===\n");
    return 0;
}