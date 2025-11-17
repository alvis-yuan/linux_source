/**
 * @file test_refcount.c
 * @brief refcount.h接口单元测试
 */
#include "vos.h"

/**
 * @brief 测试refcount_set和refcount_read
 * @return 成功返回0，失败返回-1
 */
static int test_refcount_basic(void)
{
    refcount_t ref;
    unsigned int val;

    refcount_set(&ref, 10);
    val = refcount_read(&ref);
    TEST_ASSERT_EQUAL(10, val);

    refcount_set(&ref, 0);
    val = refcount_read(&ref);
    TEST_ASSERT_EQUAL(0, val);

    refcount_set(&ref, UINT_MAX);
    val = refcount_read(&ref);
    TEST_ASSERT_EQUAL(UINT_MAX, val);

    return 0;
}

/**
 * @brief 测试refcount_inc_not_zero_checked
 * @return 成功返回0，失败返回-1
 */
static int test_refcount_inc_not_zero(void)
{
    refcount_t ref;
    bool result;

    refcount_set(&ref, 0);
    result = refcount_inc_not_zero_checked(&ref);
    TEST_ASSERT(!result);

    refcount_set(&ref, 1);
    result = refcount_inc_not_zero_checked(&ref);
    TEST_ASSERT(result);
    TEST_ASSERT_EQUAL(2, refcount_read(&ref));

    refcount_set(&ref, UINT_MAX);
    result = refcount_inc_not_zero_checked(&ref);
    TEST_ASSERT(result);
    TEST_ASSERT_EQUAL(UINT_MAX, refcount_read(&ref));

    return 0;
}

/**
 * @brief 测试refcount_inc_checked
 * @return 成功返回0，失败返回-1
 */
static int test_refcount_inc(void)
{
    refcount_t ref;

    refcount_set(&ref, 5);
    refcount_inc_checked(&ref);
    TEST_ASSERT_EQUAL(6, refcount_read(&ref));

    refcount_inc_checked(&ref);
    TEST_ASSERT_EQUAL(7, refcount_read(&ref));

    return 0;
}

/**
 * @brief 测试refcount_add_not_zero_checked
 * @return 成功返回0，失败返回-1
 */
static int test_refcount_add_not_zero(void)
{
    refcount_t ref;
    bool result;

    refcount_set(&ref, 0);
    result = refcount_add_not_zero_checked(5, &ref);
    TEST_ASSERT(!result);

    refcount_set(&ref, 10);
    result = refcount_add_not_zero_checked(5, &ref);
    TEST_ASSERT(result);
    TEST_ASSERT_EQUAL(15, refcount_read(&ref));

    refcount_set(&ref, UINT_MAX - 5);
    result = refcount_add_not_zero_checked(10, &ref);
    TEST_ASSERT(result);
    TEST_ASSERT_EQUAL(UINT_MAX, refcount_read(&ref));

    return 0;
}

/**
 * @brief 测试refcount_add_checked
 * @return 成功返回0，失败返回-1
 */
static int test_refcount_add(void)
{
    refcount_t ref;

    refcount_set(&ref, 20);
    refcount_add_checked(10, &ref);
    TEST_ASSERT_EQUAL(30, refcount_read(&ref));

    refcount_add_checked(5, &ref);
    TEST_ASSERT_EQUAL(35, refcount_read(&ref));

    return 0;
}

/**
 * @brief 测试refcount_sub_and_test_checked
 * @return 成功返回0，失败返回-1
 */
static int test_refcount_sub_and_test(void)
{
    refcount_t ref;
    bool result;

    refcount_set(&ref, 5);
    result = refcount_sub_and_test_checked(3, &ref);
    TEST_ASSERT(!result);
    TEST_ASSERT_EQUAL(2, refcount_read(&ref));

    result = refcount_sub_and_test_checked(2, &ref);
    TEST_ASSERT(result);
    TEST_ASSERT_EQUAL(0, refcount_read(&ref));

    refcount_set(&ref, UINT_MAX);
    result = refcount_sub_and_test_checked(1, &ref);
    TEST_ASSERT(!result);

    return 0;
}

/**
 * @brief 测试refcount_dec_and_test_checked
 * @return 成功返回0，失败返回-1
 */
static int test_refcount_dec_and_test(void)
{
    refcount_t ref;
    bool result;

    refcount_set(&ref, 3);
    result = refcount_dec_and_test_checked(&ref);
    TEST_ASSERT(!result);
    TEST_ASSERT_EQUAL(2, refcount_read(&ref));

    result = refcount_dec_and_test_checked(&ref);
    TEST_ASSERT(!result);
    TEST_ASSERT_EQUAL(1, refcount_read(&ref));

    result = refcount_dec_and_test_checked(&ref);
    TEST_ASSERT(result);
    TEST_ASSERT_EQUAL(0, refcount_read(&ref));

    return 0;
}

/**
 * @brief 测试refcount_dec_checked
 * @return 成功返回0，失败返回-1
 */
static int test_refcount_dec(void)
{
    refcount_t ref;

    refcount_set(&ref, 3);
    refcount_dec_checked(&ref);
    TEST_ASSERT_EQUAL(2, refcount_read(&ref));

    refcount_dec_checked(&ref);
    TEST_ASSERT_EQUAL(1, refcount_read(&ref));

    return 0;
}

/**
 * @brief 测试refcount_dec_if_one
 * @return 成功返回0，失败返回-1
 */
static int test_refcount_dec_if_one(void)
{
    refcount_t ref;
    bool result;

    refcount_set(&ref, 2);
    result = refcount_dec_if_one(&ref);
    TEST_ASSERT(!result);
    TEST_ASSERT_EQUAL(2, refcount_read(&ref));

    refcount_set(&ref, 1);
    result = refcount_dec_if_one(&ref);
    TEST_ASSERT(result);
    TEST_ASSERT_EQUAL(0, refcount_read(&ref));

    return 0;
}

/**
 * @brief 测试refcount_dec_not_one
 * @return 成功返回0，失败返回-1
 */
static int test_refcount_dec_not_one(void)
{
    refcount_t ref;
    bool result;

    refcount_set(&ref, 1);
    result = refcount_dec_not_one(&ref);
    TEST_ASSERT(!result);
    TEST_ASSERT_EQUAL(1, refcount_read(&ref));

    refcount_set(&ref, 3);
    result = refcount_dec_not_one(&ref);
    TEST_ASSERT(result);
    TEST_ASSERT_EQUAL(2, refcount_read(&ref));

    refcount_set(&ref, UINT_MAX);
    result = refcount_dec_not_one(&ref);
    TEST_ASSERT(result);

    return 0;
}

/**
 * @brief 测试边界条件
 * @return 成功返回0，失败返回-1
 */
static int test_refcount_edge_cases(void)
{
    refcount_t ref;
    bool result;

    /* 测试饱和 */
    refcount_set(&ref, UINT_MAX);
    refcount_inc_checked(&ref);
    TEST_ASSERT_EQUAL(UINT_MAX, refcount_read(&ref));

    /* 测试下溢 */
    refcount_set(&ref, 0);
    result = refcount_dec_not_one(&ref);
    TEST_ASSERT(!result);

    /* 测试正常递减 */
    refcount_set(&ref, 10);
    for (int i = 0; i < 10; i++) {
        refcount_dec_checked(&ref);
    }
    TEST_ASSERT_EQUAL(0, refcount_read(&ref));

    return 0;
}

/**
 * @brief 主测试套件
 */
TEST_SUITE_BEGIN()

    TEST_RUN(test_refcount_basic);
    TEST_RUN(test_refcount_inc_not_zero);
    TEST_RUN(test_refcount_inc);
    TEST_RUN(test_refcount_add_not_zero);
    TEST_RUN(test_refcount_add);
    TEST_RUN(test_refcount_sub_and_test);
    TEST_RUN(test_refcount_dec_and_test);
    TEST_RUN(test_refcount_dec);
    TEST_RUN(test_refcount_dec_if_one);
    TEST_RUN(test_refcount_dec_not_one);
    TEST_RUN(test_refcount_edge_cases);

TEST_SUITE_END()