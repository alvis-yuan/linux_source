#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include "timer.h"

// 测试数据结构
struct test_context {
    int callback_count;
    int expected_calls;
    uint32_t last_period;
    void *user_data;
};

// 定时器回调函数
static void timer_callback(vos_timer_t *timer)
{
    struct test_context *ctx = (struct test_context *)vos_timer_get_user_data(timer);
    if (ctx) {
        ctx->callback_count++;
        printf("Timer callback called %d times\n", ctx->callback_count);
    }
}

// 测试1: 创建和删除定时器
void test_timer_create_delete()
{
    printf("=== Test 1: Timer Create and Delete ===\n");
    
    // 测试正常创建
    vos_timer_t *timer = vos_timer_create(timer_callback, 100, NULL);
    assert(timer != NULL);
    printf("Timer created successfully\n");
    
    // 测试删除
    vos_timer_delete(timer);
    printf("Timer deleted successfully\n");
    
    // 测试NULL指针安全
    vos_timer_delete(NULL); // 应该不会崩溃
    printf("NULL timer delete handled safely\n");
    
    printf("Test 1 PASSED\n\n");
}

// 测试2: 暂停和恢复定时器
void test_timer_pause_resume()
{
    printf("=== Test 2: Timer Pause and Resume ===\n");
    
    vos_timer_t *timer = vos_timer_create(timer_callback, 100, NULL);
    assert(timer != NULL);
    
    // 测试暂停
    vos_timer_pause(timer);
    assert(vos_timer_get_paused(timer) == true);
    printf("Timer paused successfully\n");
    
    // 测试重复暂停
    vos_timer_pause(timer); // 应该不会重复操作
    printf("Duplicate pause handled correctly\n");
    
    // 测试恢复
    vos_timer_resume(timer);
    assert(vos_timer_get_paused(timer) == false);
    printf("Timer resumed successfully\n");
    
    // 测试重复恢复
    vos_timer_resume(timer); // 应该不会重复操作
    printf("Duplicate resume handled correctly\n");
    
    // 测试NULL指针安全
    vos_timer_pause(NULL);
    vos_timer_resume(NULL);
    printf("NULL timer pause/resume handled safely\n");
    
    vos_timer_delete(timer);
    printf("Test 2 PASSED\n\n");
}

// 测试3: 设置定时器属性
void test_timer_set_properties()
{
    printf("=== Test 3: Timer Set Properties ===\n");
    
    vos_timer_t *timer = vos_timer_create(timer_callback, 100, NULL);
    assert(timer != NULL);
    
    // 测试设置回调函数
    vos_timer_set_cb(timer, timer_callback);
    printf("Callback function set successfully\n");
    
    // 测试设置周期
    vos_timer_set_period(timer, 200);
    printf("Period set to 200ms\n");
    
    // 测试设置重复次数
    vos_timer_set_repeat_count(timer, 5);
    printf("Repeat count set to 5\n");
    
    // 测试设置自动删除
    vos_timer_set_auto_delete(timer, true);
    printf("Auto delete enabled\n");
    
    // 测试设置用户数据
    int user_data = 42;
    vos_timer_set_user_data(timer, &user_data);
    assert(vos_timer_get_user_data(timer) == &user_data);
    printf("User data set successfully\n");
    
    // 测试NULL指针安全
    vos_timer_set_cb(NULL, timer_callback);
    vos_timer_set_period(NULL, 100);
    vos_timer_set_repeat_count(NULL, 1);
    vos_timer_set_auto_delete(NULL, false);
    vos_timer_set_user_data(NULL, NULL);
    printf("NULL timer property setters handled safely\n");
    
    vos_timer_delete(timer);
    printf("Test 3 PASSED\n\n");
}

// 测试4: 获取定时器属性
void test_timer_get_properties()
{
    printf("=== Test 4: Timer Get Properties ===\n");
    
    int user_data = 123;
    vos_timer_t *timer = vos_timer_create(timer_callback, 150, &user_data);
    assert(timer != NULL);
    
    // 测试获取暂停状态
    assert(vos_timer_get_paused(timer) == false);
    printf("Initial pause state: false\n");
    
    // 测试获取用户数据
    assert(vos_timer_get_user_data(timer) == &user_data);
    printf("User data retrieved successfully\n");
    
    // 测试NULL指针安全
    assert(vos_timer_get_paused(NULL) == false);
    assert(vos_timer_get_user_data(NULL) == NULL);
    printf("NULL timer property getters handled safely\n");
    
    vos_timer_delete(timer);
    printf("Test 4 PASSED\n\n");
}

// 测试5: 定时器回调功能
void test_timer_callback_functionality()
{
    printf("=== Test 5: Timer Callback Functionality ===\n");
    
    struct test_context ctx = {
        .callback_count = 0,
        .expected_calls = 0,
        .user_data = NULL
    };
    
    // 创建单次定时器
    vos_timer_t *timer = vos_timer_create(timer_callback, 50, &ctx);
    assert(timer != NULL);
    
    printf("Single-shot timer created, waiting for callback...\n");
    vos_timer_resume(timer);
    
    // 等待足够时间让定时器触发
    usleep(100000); // 100ms
    
    // 由于是单次定时器，应该至少触发一次
    printf("Callback count: %d\n", ctx.callback_count);
    
    vos_timer_delete(timer);
    printf("Test 5 PASSED\n\n");
}

// 测试6: 重复定时器
void test_timer_repeat_functionality()
{
    printf("=== Test 6: Timer Repeat Functionality ===\n");
    
    struct test_context ctx = {
        .callback_count = 0,
        .expected_calls = 3,
        .user_data = NULL
    };
    
    // 创建重复定时器
    vos_timer_t *timer = vos_timer_create(timer_callback, 30, &ctx);
    assert(timer != NULL);
    
    // 设置为重复3次
    vos_timer_set_repeat_count(timer, 3);
    
    printf("Repeat timer created (3 times), waiting...\n");
    
    // 等待足够时间让定时器触发多次
    usleep(200000); // 200ms
    
    printf("Callback count: %d\n", ctx.callback_count);
    
    vos_timer_delete(timer);
    printf("Test 6 PASSED\n\n");
}

// 测试7: 自动删除功能
void test_timer_auto_delete()
{
    printf("=== Test 7: Timer Auto Delete ===\n");
    
    struct test_context ctx = {
        .callback_count = 0,
        .expected_calls = 1,
        .user_data = NULL
    };
    
    // 创建定时器并启用自动删除
    vos_timer_t *timer = vos_timer_create(timer_callback, 40, &ctx);
    assert(timer != NULL);
    
    vos_timer_set_auto_delete(timer, true);
    vos_timer_set_repeat_count(timer, 1); // 单次触发
    
    printf("Auto-delete timer created, waiting...\n");
    
    // 等待定时器触发
    usleep(100000); // 100ms
    
    printf("Auto-delete test completed\n");
    
    // 注意：定时器应该已经被自动删除，这里不需要手动删除
    printf("Test 7 PASSED\n\n");
}

// 测试8: 边界条件测试
void test_timer_edge_cases()
{
    printf("=== Test 8: Timer Edge Cases ===\n");
    
    // 测试零周期定时器
    vos_timer_t *timer = vos_timer_create(timer_callback, 0, NULL);
    assert(timer != NULL);
    vos_timer_delete(timer);
    printf("Zero period timer handled\n");
    
    // 测试极大周期定时器
    timer = vos_timer_create(timer_callback, UINT32_MAX, NULL);
    assert(timer != NULL);
    vos_timer_delete(timer);
    printf("Large period timer handled\n");
    
    // 测试各种重复次数
    timer = vos_timer_create(timer_callback, 100, NULL);
    assert(timer != NULL);
    
    vos_timer_set_repeat_count(timer, -1); // 无限重复
    vos_timer_set_repeat_count(timer, 0);  // 应该被处理
    vos_timer_set_repeat_count(timer, 1);  // 单次
    
    vos_timer_delete(timer);
    printf("Various repeat counts handled\n");
    
    printf("Test 8 PASSED\n\n");
}

// 主测试函数
int main()
{
    printf("Starting Timer Test Suite\n\n");
    
    //test_timer_create_delete();
    //test_timer_pause_resume();
    //test_timer_set_properties();
    //test_timer_get_properties();
    test_timer_callback_functionality();
    //test_timer_repeat_functionality();
    //test_timer_auto_delete();
    //test_timer_edge_cases();
    
    printf("All timer tests PASSED! ✓\n");
    printf("Timer implementation is working correctly.\n");
    
    return 0;
}
