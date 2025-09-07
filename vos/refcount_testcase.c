/**
 * @file test_refcount.c
 * @brief 引用计数接口测试用例
 */

#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include "refcount.h"

/**
 * @brief 测试refcount_set和refcount_read
 */
void test_set_and_read(void)
{
    printf("=== 测试 refcount_set 和 refcount_read ===\n");
    
    refcount_t rc;
    refcount_set(&rc, 5);
    assert(refcount_read(&rc) == 5);
    printf("设置值5，读取值: %u ✓\n", refcount_read(&rc));
    
    refcount_set(&rc, 0);
    assert(refcount_read(&rc) == 0);
    printf("设置值0，读取值: %u ✓\n", refcount_read(&rc));
    
    refcount_set(&rc, REFCOUNT_MAX);
    assert(refcount_read(&rc) == REFCOUNT_MAX);
    printf("设置最大值，读取值: %u ✓\n", refcount_read(&rc));
    
    printf("测试通过！\n\n");
}

/**
 * @brief 测试refcount_add_not_zero
 */
void test_add_not_zero(void)
{
    printf("=== 测试 refcount_add_not_zero ===\n");
    
    refcount_t rc;
    
    // 测试从0开始添加
    refcount_set(&rc, 0);
    bool result = refcount_add_not_zero(1, &rc);
    assert(result == false);
    assert(refcount_read(&rc) == 0);
    printf("从0开始添加1: 失败 ✓\n");
    
    // 测试正常添加
    refcount_set(&rc, 1);
    result = refcount_add_not_zero(2, &rc);
    assert(result == true);
    assert(refcount_read(&rc) == 3);
    printf("从1开始添加2: 成功，值=%u ✓\n", refcount_read(&rc));
    
    // 测试饱和
    refcount_set(&rc, REFCOUNT_MAX - 1);
    result = refcount_add_not_zero(2, &rc);
    assert(result == true);
    assert(refcount_read(&rc) == REFCOUNT_SATURATED);
    printf("接近最大值添加2: 饱和 ✓\n");
    
    printf("测试通过！\n\n");
}

/**
 * @brief 测试refcount_add
 */
void test_add(void)
{
    printf("=== 测试 refcount_add ===\n");
    
    refcount_t rc;
    
    // 测试正常加法
    refcount_set(&rc, 10);
    refcount_add(5, &rc);
    assert(refcount_read(&rc) == 15);
    printf("10 + 5 = %u ✓\n", refcount_read(&rc));
    
    // 测试在0上加法（应该警告）
    refcount_set(&rc, 0);
    printf("current rc: %d\n", refcount_read(&rc));
    //refcount_add(1, &rc);
    //atomic_fetch_add(&rc.refs, 1);
    __refcount_add(1, &rc, NULL);
    printf("current rc: %d\n", refcount_read(&rc));
    printf("current rc: %d\n", atomic_load(&rc.refs));
    assert(refcount_read(&rc) == 1); // 仍然会加，但会警告
    printf("0 + 1 = %u (有警告) ✓\n", refcount_read(&rc));
    
    // 测试饱和
    refcount_set(&rc, REFCOUNT_MAX - 1);
    refcount_add(2, &rc);
    assert(refcount_read(&rc) == REFCOUNT_SATURATED);
    printf("接近最大值+2: 饱和 ✓\n");
    
    printf("测试通过！\n\n");
}

/**
 * @brief 测试refcount_inc_not_zero
 */
void test_inc_not_zero(void)
{
    printf("=== 测试 refcount_inc_not_zero ===\n");
    
    refcount_t rc;
    
    // 测试从0开始增加
    refcount_set(&rc, 0);
    bool result = refcount_inc_not_zero(&rc);
    assert(result == false);
    assert(refcount_read(&rc) == 0);
    printf("从0开始增加: 失败 ✓\n");
    
    // 测试正常增加
    refcount_set(&rc, 5);
    result = refcount_inc_not_zero(&rc);
    assert(result == true);
    assert(refcount_read(&rc) == 6);
    printf("5 + 1 = %u ✓\n", refcount_read(&rc));
    
    // 测试饱和
    refcount_set(&rc, REFCOUNT_MAX);
    result = refcount_inc_not_zero(&rc);
    assert(result == true); // 仍然返回true，但会饱和
    assert(refcount_read(&rc) == REFCOUNT_SATURATED);
    printf("最大值+1: 饱和 ✓\n");
    
    printf("测试通过！\n\n");
}

/**
 * @brief 测试refcount_inc
 */
void test_inc(void)
{
    printf("=== 测试 refcount_inc ===\n");
    
    refcount_t rc;
    
    // 测试正常增加
    refcount_set(&rc, 8);
    refcount_inc(&rc);
    assert(refcount_read(&rc) == 9);
    printf("8 + 1 = %u ✓\n", refcount_read(&rc));
    
    // 测试在0上增加（应该警告）
    refcount_set(&rc, 0);
    refcount_inc(&rc);
    assert(refcount_read(&rc) == 1); // 仍然会增加，但会警告
    printf("0 + 1 = %u (有警告) ✓\n", refcount_read(&rc));
    
    // 测试饱和
    refcount_set(&rc, REFCOUNT_MAX);
    refcount_inc(&rc);
    assert(refcount_read(&rc) == REFCOUNT_SATURATED);
    printf("最大值+1: 饱和 ✓\n");
    
    printf("测试通过！\n\n");
}

/**
 * @brief 测试refcount_sub_and_test
 */
void test_sub_and_test(void)
{
    printf("=== 测试 refcount_sub_and_test ===\n");
    
    refcount_t rc;
    
    // 测试减法后不为0
    refcount_set(&rc, 5);
    bool result = refcount_sub_and_test(2, &rc);
    assert(result == false);
    assert(refcount_read(&rc) == 3);
    printf("5 - 2 = %u, 不为0 ✓\n", refcount_read(&rc));
    
    // 测试减法后为0
    refcount_set(&rc, 3);
    result = refcount_sub_and_test(3, &rc);
    assert(result == true);
    assert(refcount_read(&rc) == 0);
    printf("3 - 3 = 0 ✓\n");
    
    // 测试下溢（应该警告）
    refcount_set(&rc, 1);
    result = refcount_sub_and_test(2, &rc);
    assert(result == false); // 不会为0，因为会饱和
    assert(refcount_read(&rc) == REFCOUNT_SATURATED);
    printf("1 - 2: 下溢，饱和 ✓\n");
    
    printf("测试通过！\n\n");
}

/**
 * @brief 测试refcount_dec_and_test
 */
void test_dec_and_test(void)
{
    printf("=== 测试 refcount_dec_and_test ===\n");
    
    refcount_t rc;
    
    // 测试减少后不为0
    refcount_set(&rc, 3);
    bool result = refcount_dec_and_test(&rc);
    assert(result == false);
    assert(refcount_read(&rc) == 2);
    printf("3 - 1 = %u, 不为0 ✓\n", refcount_read(&rc));
    
    // 测试减少后为0
    refcount_set(&rc, 1);
    result = refcount_dec_and_test(&rc);
    assert(result == true);
    assert(refcount_read(&rc) == 0);
    printf("1 - 1 = 0 ✓\n");
    
    // 测试下溢（应该警告）
    refcount_set(&rc, 0);
    result = refcount_dec_and_test(&rc);
    assert(result == false); // 不会为0，因为会饱和
    assert(refcount_read(&rc) == REFCOUNT_SATURATED);
    printf("0 - 1: 下溢，饱和 ✓\n");
    
    printf("测试通过！\n\n");
}

/**
 * @brief 测试refcount_dec
 */
void test_dec(void)
{
    printf("=== 测试 refcount_dec ===\n");
    
    refcount_t rc;
    
    // 测试正常减少
    refcount_set(&rc, 4);
    refcount_dec(&rc);
    assert(refcount_read(&rc) == 3);
    printf("4 - 1 = %u ✓\n", refcount_read(&rc));
    
    // 测试减少到0（应该警告）
    refcount_set(&rc, 1);
    refcount_dec(&rc);
    assert(refcount_read(&rc) == 0);
    printf("1 - 1 = 0 (有警告) ✓\n");
    
    // 测试下溢（应该警告）
    refcount_set(&rc, 0);
    refcount_dec(&rc);
    assert(refcount_read(&rc) == REFCOUNT_SATURATED);
    printf("0 - 1: 下溢，饱和 ✓\n");
    
    printf("测试通过！\n\n");
}

/**
 * @brief 测试refcount_dec_if_one
 */
void test_dec_if_one(void)
{
    printf("=== 测试 refcount_dec_if_one ===\n");
    
    refcount_t rc;
    
    // 测试值为1时减少
    refcount_set(&rc, 1);
    bool result = refcount_dec_if_one(&rc);
    assert(result == true);
    assert(refcount_read(&rc) == 0);
    printf("1 -> 0: 成功 ✓\n");
    
    // 测试值不为1时
    refcount_set(&rc, 2);
    result = refcount_dec_if_one(&rc);
    assert(result == false);
    assert(refcount_read(&rc) == 2);
    printf("2 -> 2: 失败 ✓\n");
    
    // 测试值为0时
    refcount_set(&rc, 0);
    result = refcount_dec_if_one(&rc);
    assert(result == false);
    assert(refcount_read(&rc) == 0);
    printf("0 -> 0: 失败 ✓\n");
    
    printf("测试通过！\n\n");
}

/**
 * @brief 测试refcount_dec_not_one
 */
void test_dec_not_one(void)
{
    printf("=== 测试 refcount_dec_not_one ===\n");
    
    refcount_t rc;
    
    // 测试值大于1时减少
    refcount_set(&rc, 3);
    bool result = refcount_dec_not_one(&rc);
    assert(result == true);
    assert(refcount_read(&rc) == 2);
    printf("3 -> 2: 成功 ✓\n");
    
    // 测试值为1时
    refcount_set(&rc, 1);
    result = refcount_dec_not_one(&rc);
    assert(result == false);
    assert(refcount_read(&rc) == 1);
    printf("1 -> 1: 失败 ✓\n");
    
    // 测试饱和值时
    refcount_set(&rc, REFCOUNT_SATURATED);
    result = refcount_dec_not_one(&rc);
    assert(result == true); // 饱和值总是返回true
    assert(refcount_read(&rc) == REFCOUNT_SATURATED);
    printf("饱和值 -> 饱和值: 成功 ✓\n");
    
    // 测试下溢保护
    refcount_set(&rc, 0);
    result = refcount_dec_not_one(&rc);
    assert(result == true); // 会检测到下溢
    assert(refcount_read(&rc) == REFCOUNT_SATURATED);
    printf("0 -> 饱和值: 下溢保护 ✓\n");
    
    printf("测试通过！\n\n");
}

/**
 * @brief 测试饱和警告功能
 */
void test_saturation_warnings(void)
{
    printf("=== 测试饱和警告功能 ===\n");
    
    refcount_t rc;
    
    printf("测试各种饱和情况（应该有警告输出）:\n");
    
    // 测试加法溢出
    refcount_set(&rc, REFCOUNT_MAX - 1);
    refcount_add(2, &rc);
    
    // 测试在0上加法
    refcount_set(&rc, 0);
    refcount_inc(&rc);
    
    // 测试减法下溢
    refcount_set(&rc, 1);
    refcount_sub_and_test(2, &rc);
    
    // 测试减少下溢
    refcount_set(&rc, 0);
    refcount_dec(&rc);
    
    printf("饱和警告测试完成！\n\n");
}

/**
 * @brief 综合测试：模拟实际使用场景
 */
void test_comprehensive(void)
{
    printf("=== 综合测试：模拟对象生命周期 ===\n");
    
    refcount_t obj_refcount = REFCOUNT_INIT(1); // 对象创建时引用计数为1
    
    // 模拟多个引用
    assert(refcount_inc_not_zero(&obj_refcount) == true);
    assert(refcount_read(&obj_refcount) == 2);
    printf("第一次引用增加: %u ✓\n", refcount_read(&obj_refcount));
    
    assert(refcount_inc_not_zero(&obj_refcount) == true);
    assert(refcount_read(&obj_refcount) == 3);
    printf("第二次引用增加: %u ✓\n", refcount_read(&obj_refcount));
    
    // 模拟引用释放
    assert(refcount_dec_and_test(&obj_refcount) == false);
    assert(refcount_read(&obj_refcount) == 2);
    printf("第一次引用释放: %u ✓\n", refcount_read(&obj_refcount));
    
    assert(refcount_dec_and_test(&obj_refcount) == false);
    assert(refcount_read(&obj_refcount) == 1);
    printf("第二次引用释放: %u ✓\n", refcount_read(&obj_refcount));
    
    // 最后释放（应该返回true表示可以释放对象）
    assert(refcount_dec_and_test(&obj_refcount) == true);
    assert(refcount_read(&obj_refcount) == 0);
    printf("最后释放: 可以安全释放对象 ✓\n");
    
    // 测试后续操作（应该警告）
    refcount_inc(&obj_refcount); // use-after-free 警告
    
    printf("综合测试通过！\n\n");
}

/**
 * @brief 主测试函数
 */
int main(void)
{
    printf("开始引用计数接口测试...\n\n");
    
    test_set_and_read();
    test_add_not_zero();
    test_add();
    test_inc_not_zero();
    test_inc();
    test_sub_and_test();
    test_dec_and_test();
    test_dec();
    test_dec_if_one();
    test_dec_not_one();
    test_saturation_warnings();
    test_comprehensive();
    
    printf("所有测试用例通过！✓\n");
    printf("引用计数实现功能正常！\n");
    
    return 0;
}