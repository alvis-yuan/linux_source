/**
 * @file test_atomic.c
 * @brief atomic.h接口单元测试
 */
#include "vos.h"

/**
 * @brief 测试atomic_read和atomic_set
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_read_set(void)
{
    atomic_t var;

    atomic_set(&var, 100);
    TEST_ASSERT_EQUAL(100, atomic_read(&var));

    atomic_set(&var, -50);
    TEST_ASSERT_EQUAL(-50, atomic_read(&var));

    atomic_set(&var, 0);
    TEST_ASSERT_EQUAL(0, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_add和atomic_sub
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_add_sub(void)
{
    atomic_t var = ATOMIC_INIT(10);

    atomic_add(5, &var);
    TEST_ASSERT_EQUAL(15, atomic_read(&var));

    atomic_sub(8, &var);
    TEST_ASSERT_EQUAL(7, atomic_read(&var));

    atomic_add(-3, &var);
    TEST_ASSERT_EQUAL(4, atomic_read(&var));

    atomic_sub(-6, &var);
    TEST_ASSERT_EQUAL(10, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_inc和atomic_dec
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_inc_dec(void)
{
    atomic_t var = ATOMIC_INIT(5);

    atomic_inc(&var);
    TEST_ASSERT_EQUAL(6, atomic_read(&var));

    atomic_dec(&var);
    TEST_ASSERT_EQUAL(5, atomic_read(&var));

    for (int i = 0; i < 10; i++) {
        atomic_inc(&var);
    }
    TEST_ASSERT_EQUAL(15, atomic_read(&var));

    for (int i = 0; i < 5; i++) {
        atomic_dec(&var);
    }
    TEST_ASSERT_EQUAL(10, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_add_return和atomic_sub_return
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_add_sub_return(void)
{
    atomic_t var = ATOMIC_INIT(20);
    int result;

    result = atomic_add_return(10, &var);
    TEST_ASSERT_EQUAL(30, result);
    TEST_ASSERT_EQUAL(30, atomic_read(&var));

    result = atomic_sub_return(15, &var);
    TEST_ASSERT_EQUAL(15, result);
    TEST_ASSERT_EQUAL(15, atomic_read(&var));

    result = atomic_add_return(-5, &var);
    TEST_ASSERT_EQUAL(10, result);
    TEST_ASSERT_EQUAL(10, atomic_read(&var));

    result = atomic_sub_return(-8, &var);
    TEST_ASSERT_EQUAL(18, result);
    TEST_ASSERT_EQUAL(18, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_inc_return和atomic_dec_return
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_inc_dec_return(void)
{
    atomic_t var = ATOMIC_INIT(0);
    int result;

    result = atomic_inc_return(&var);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL(1, atomic_read(&var));

    result = atomic_dec_return(&var);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(0, atomic_read(&var));

    result = atomic_inc_return(&var);
    result = atomic_inc_return(&var);
    result = atomic_inc_return(&var);
    TEST_ASSERT_EQUAL(3, result);
    TEST_ASSERT_EQUAL(3, atomic_read(&var));

    result = atomic_dec_return(&var);
    result = atomic_dec_return(&var);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL(1, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_fetch_add和atomic_fetch_sub
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_fetch_add_sub(void)
{
    atomic_t var = ATOMIC_INIT(100);
    int old_val;

    old_val = atomic_fetch_add(20, &var);
    TEST_ASSERT_EQUAL(100, old_val);
    TEST_ASSERT_EQUAL(120, atomic_read(&var));

    old_val = atomic_fetch_sub(30, &var);
    TEST_ASSERT_EQUAL(120, old_val);
    TEST_ASSERT_EQUAL(90, atomic_read(&var));

    old_val = atomic_fetch_add(-10, &var);
    TEST_ASSERT_EQUAL(90, old_val);
    TEST_ASSERT_EQUAL(80, atomic_read(&var));

    old_val = atomic_fetch_sub(-15, &var);
    TEST_ASSERT_EQUAL(80, old_val);
    TEST_ASSERT_EQUAL(95, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_fetch_inc和atomic_fetch_dec
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_fetch_inc_dec(void)
{
    atomic_t var = ATOMIC_INIT(5);
    int old_val;

    old_val = atomic_fetch_inc(&var);
    TEST_ASSERT_EQUAL(5, old_val);
    TEST_ASSERT_EQUAL(6, atomic_read(&var));

    old_val = atomic_fetch_dec(&var);
    TEST_ASSERT_EQUAL(6, old_val);
    TEST_ASSERT_EQUAL(5, atomic_read(&var));

    old_val = atomic_fetch_inc(&var);
    old_val = atomic_fetch_inc(&var);
    TEST_ASSERT_EQUAL(6, old_val);
    TEST_ASSERT_EQUAL(7, atomic_read(&var));

    old_val = atomic_fetch_dec(&var);
    old_val = atomic_fetch_dec(&var);
    TEST_ASSERT_EQUAL(6, old_val);
    TEST_ASSERT_EQUAL(5, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_and和atomic_or
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_and_or(void)
{
    atomic_t var = ATOMIC_INIT(0xFF);

    atomic_and(0x0F, &var);
    TEST_ASSERT_EQUAL(0x0F, atomic_read(&var));

    atomic_or(0xF0, &var);
    TEST_ASSERT_EQUAL(0xFF, atomic_read(&var));

    atomic_and(0xAA, &var);
    TEST_ASSERT_EQUAL(0xAA, atomic_read(&var));

    atomic_or(0x55, &var);
    TEST_ASSERT_EQUAL(0xFF, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_xor
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_xor(void)
{
    atomic_t var = ATOMIC_INIT(0x55);

    atomic_xor(0xFF, &var);
    TEST_ASSERT_EQUAL(0xAA, atomic_read(&var));

    atomic_xor(0xAA, &var);
    TEST_ASSERT_EQUAL(0x00, atomic_read(&var));

    atomic_xor(0xFF, &var);
    TEST_ASSERT_EQUAL(0xFF, atomic_read(&var));

    atomic_xor(0x0F, &var);
    TEST_ASSERT_EQUAL(0xF0, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_fetch_and和atomic_fetch_or
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_fetch_and_or(void)
{
    atomic_t var = ATOMIC_INIT(0xFF);
    int old_val;

    old_val = atomic_fetch_and(0x0F, &var);
    TEST_ASSERT_EQUAL(0xFF, old_val);
    TEST_ASSERT_EQUAL(0x0F, atomic_read(&var));

    old_val = atomic_fetch_or(0xF0, &var);
    TEST_ASSERT_EQUAL(0x0F, old_val);
    TEST_ASSERT_EQUAL(0xFF, atomic_read(&var));

    old_val = atomic_fetch_and(0x33, &var);
    TEST_ASSERT_EQUAL(0xFF, old_val);
    TEST_ASSERT_EQUAL(0x33, atomic_read(&var));

    old_val = atomic_fetch_or(0xCC, &var);
    TEST_ASSERT_EQUAL(0x33, old_val);
    TEST_ASSERT_EQUAL(0xFF, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_fetch_xor
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_fetch_xor(void)
{
    atomic_t var = ATOMIC_INIT(0x55);
    int old_val;

    old_val = atomic_fetch_xor(0xFF, &var);
    TEST_ASSERT_EQUAL(0x55, old_val);
    TEST_ASSERT_EQUAL(0xAA, atomic_read(&var));

    old_val = atomic_fetch_xor(0xAA, &var);
    TEST_ASSERT_EQUAL(0xAA, old_val);
    TEST_ASSERT_EQUAL(0x00, atomic_read(&var));

    old_val = atomic_fetch_xor(0xFF, &var);
    TEST_ASSERT_EQUAL(0x00, old_val);
    TEST_ASSERT_EQUAL(0xFF, atomic_read(&var));

    old_val = atomic_fetch_xor(0x0F, &var);
    TEST_ASSERT_EQUAL(0xFF, old_val);
    TEST_ASSERT_EQUAL(0xF0, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_cmpxchg
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_cmpxchg(void)
{
    atomic_t var = ATOMIC_INIT(100);
    int expected;
    bool success;

    expected = 100;
    success = atomic_cmpxchg(&var, expected, 200);
    TEST_ASSERT(success);
    TEST_ASSERT_EQUAL(200, atomic_read(&var));

    expected = 300;
    success = atomic_cmpxchg(&var, expected, 400);
    TEST_ASSERT(!success);
    TEST_ASSERT_EQUAL(200, atomic_read(&var));

    expected = 200;
    success = atomic_cmpxchg(&var, expected, 0);
    TEST_ASSERT(success);
    TEST_ASSERT_EQUAL(0, atomic_read(&var));

    expected = -1;
    success = atomic_cmpxchg(&var, expected, 500);
    TEST_ASSERT(!success);
    TEST_ASSERT_EQUAL(0, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_xchg
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_xchg(void)
{
    atomic_t var = ATOMIC_INIT(50);
    int old_val;

    old_val = atomic_xchg(&var, 100);
    TEST_ASSERT_EQUAL(50, old_val);
    TEST_ASSERT_EQUAL(100, atomic_read(&var));

    old_val = atomic_xchg(&var, -25);
    TEST_ASSERT_EQUAL(100, old_val);
    TEST_ASSERT_EQUAL(-25, atomic_read(&var));

    old_val = atomic_xchg(&var, 0);
    TEST_ASSERT_EQUAL(-25, old_val);
    TEST_ASSERT_EQUAL(0, atomic_read(&var));

    old_val = atomic_xchg(&var, 999);
    TEST_ASSERT_EQUAL(0, old_val);
    TEST_ASSERT_EQUAL(999, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_dec_and_test
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_dec_and_test(void)
{
    atomic_t var;
    bool result;

    atomic_set(&var, 1);
    result = atomic_dec_and_test(&var);
    TEST_ASSERT(result);
    TEST_ASSERT_EQUAL(0, atomic_read(&var));

    atomic_set(&var, 2);
    result = atomic_dec_and_test(&var);
    TEST_ASSERT(!result);
    TEST_ASSERT_EQUAL(1, atomic_read(&var));

    atomic_set(&var, 0);
    result = atomic_dec_and_test(&var);
    TEST_ASSERT(!result);
    TEST_ASSERT_EQUAL(-1, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_inc_and_test
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_inc_and_test(void)
{
    atomic_t var;
    bool result;

    atomic_set(&var, -1);
    result = atomic_inc_and_test(&var);
    TEST_ASSERT(result);
    TEST_ASSERT_EQUAL(0, atomic_read(&var));

    atomic_set(&var, 0);
    result = atomic_inc_and_test(&var);
    TEST_ASSERT(!result);
    TEST_ASSERT_EQUAL(1, atomic_read(&var));

    atomic_set(&var, 5);
    result = atomic_inc_and_test(&var);
    TEST_ASSERT(!result);
    TEST_ASSERT_EQUAL(6, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_sub_and_test
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_sub_and_test(void)
{
    atomic_t var;
    bool result;

    atomic_set(&var, 5);
    result = atomic_sub_and_test(5, &var);
    TEST_ASSERT(result);
    TEST_ASSERT_EQUAL(0, atomic_read(&var));

    atomic_set(&var, 10);
    result = atomic_sub_and_test(5, &var);
    TEST_ASSERT(!result);
    TEST_ASSERT_EQUAL(5, atomic_read(&var));

    atomic_set(&var, 3);
    result = atomic_sub_and_test(10, &var);
    TEST_ASSERT(!result);
    TEST_ASSERT_EQUAL(-7, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试atomic_test_and_set
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_test_and_set(void)
{
    atomic_t var;
    bool result;

    atomic_set(&var, 0);
    result = atomic_test_and_set(&var);
    TEST_ASSERT(!result);

    atomic_set(&var, 1);
    result = atomic_test_and_set(&var);
    TEST_ASSERT(result);

    atomic_set(&var, -1);
    result = atomic_test_and_set(&var);
    TEST_ASSERT(result);

    atomic_set(&var, 100);
    result = atomic_test_and_set(&var);
    TEST_ASSERT(result);

    return 0;
}

/**
 * @brief 测试边界值和极端情况
 * @return 成功返回0，失败返回-1
 */
static int test_edge_cases(void)
{
    atomic_t var;
    int result;

    atomic_set(&var, 2147483647);
    atomic_inc(&var);
    TEST_ASSERT_EQUAL(-2147483648, atomic_read(&var));

    atomic_set(&var, -2147483648);
    atomic_dec(&var);
    TEST_ASSERT_EQUAL(2147483647, atomic_read(&var));

    atomic_set(&var, 0);
    for (int i = 0; i < 1000; i++) {
        atomic_inc(&var);
    }
    TEST_ASSERT_EQUAL(1000, atomic_read(&var));

    for (int i = 0; i < 1000; i++) {
        atomic_dec(&var);
    }
    TEST_ASSERT_EQUAL(0, atomic_read(&var));

    return 0;
}

/**
 * @brief 测试ATOMIC_INIT宏
 * @return 成功返回0，失败返回-1
 */
static int test_atomic_init(void)
{
    atomic_t var1 = ATOMIC_INIT(0);
    TEST_ASSERT_EQUAL(0, atomic_read(&var1));

    atomic_t var2 = ATOMIC_INIT(100);
    TEST_ASSERT_EQUAL(100, atomic_read(&var2));

    atomic_t var3 = ATOMIC_INIT(-50);
    TEST_ASSERT_EQUAL(-50, atomic_read(&var3));

    return 0;
}

/**
 * @brief 主测试套件
 */
TEST_SUITE_BEGIN()

    TEST_RUN(test_atomic_read_set);
    TEST_RUN(test_atomic_add_sub);
    TEST_RUN(test_atomic_inc_dec);
    TEST_RUN(test_atomic_add_sub_return);
    TEST_RUN(test_atomic_inc_dec_return);
    TEST_RUN(test_atomic_fetch_add_sub);
    TEST_RUN(test_atomic_fetch_inc_dec);
    TEST_RUN(test_atomic_and_or);
    TEST_RUN(test_atomic_xor);
    TEST_RUN(test_atomic_fetch_and_or);
    TEST_RUN(test_atomic_fetch_xor);
    TEST_RUN(test_atomic_cmpxchg);
    TEST_RUN(test_atomic_xchg);
    TEST_RUN(test_atomic_dec_and_test);
    TEST_RUN(test_atomic_inc_and_test);
    TEST_RUN(test_atomic_sub_and_test);
    TEST_RUN(test_atomic_test_and_set);
    TEST_RUN(test_edge_cases);
    TEST_RUN(test_atomic_init);

TEST_SUITE_END()