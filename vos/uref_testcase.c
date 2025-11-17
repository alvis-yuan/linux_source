/**
 * @file test_uref.c
 * @brief uref.h接口单元测试
 */

#include "vos.h"

/**
 * @brief 测试对象结构体
 */
struct test_object {
    struct uref ref;
    int data;
    bool released;
};

/**
 * @brief 测试对象的释放函数
 * @param uref uref对象
 */
static void test_release(struct uref *uref)
{
    struct test_object *obj;
    
    obj = container_of(uref, struct test_object, ref);
    obj->released = true;
    LogInfo("Object released, data=%d", obj->data);
}

/**
 * @brief 测试uref_init和uref_read
 * @return 成功返回0，失败返回-1
 */
static int test_uref_init_read(void)
{
    struct test_object obj;
    unsigned int count;

    uref_init(&obj.ref);
    count = uref_read(&obj.ref);
    TEST_ASSERT_EQUAL(1, count);

    obj.data = 100;
    obj.released = false;

    return 0;
}

/**
 * @brief 测试uref_get
 * @return 成功返回0，失败返回-1
 */
static int test_uref_get(void)
{
    struct test_object obj;
    unsigned int count;

    uref_init(&obj.ref);
    
    uref_get(&obj.ref);
    count = uref_read(&obj.ref);
    TEST_ASSERT_EQUAL(2, count);

    uref_get(&obj.ref);
    count = uref_read(&obj.ref);
    TEST_ASSERT_EQUAL(3, count);

    obj.data = 200;
    obj.released = false;

    return 0;
}

/**
 * @brief 测试uref_put基本功能
 * @return 成功返回0，失败返回-1
 */
static int test_uref_put_basic(void)
{
    struct test_object obj;
    int result;

    uref_init(&obj.ref);
    obj.data = 300;
    obj.released = false;

    result = uref_put(&obj.ref, test_release);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT(obj.released);

    return 0;
}

/**
 * @brief 测试uref_put多次引用
 * @return 成功返回0，失败返回-1
 */
static int test_uref_put_multiple(void)
{
    struct test_object obj;
    int result;

    uref_init(&obj.ref);
    obj.data = 400;
    obj.released = false;

    /* 增加引用计数 */
    uref_get(&obj.ref);
    uref_get(&obj.ref);
    TEST_ASSERT_EQUAL(3, uref_read(&obj.ref));

    /* 第一次put，不应该释放 */
    result = uref_put(&obj.ref, test_release);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT(!obj.released);
    TEST_ASSERT_EQUAL(2, uref_read(&obj.ref));

    /* 第二次put，不应该释放 */
    result = uref_put(&obj.ref, test_release);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT(!obj.released);
    TEST_ASSERT_EQUAL(1, uref_read(&obj.ref));

    /* 第三次put，应该释放 */
    result = uref_put(&obj.ref, test_release);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT(obj.released);

    return 0;
}

/**
 * @brief 测试uref_get_unless_zero成功情况
 * @return 成功返回0，失败返回-1
 */
static int test_uref_get_unless_zero_success(void)
{
    struct test_object obj;
    int result;
    unsigned int count;

    uref_init(&obj.ref);
    obj.data = 500;
    obj.released = false;

    result = uref_get_unless_zero(&obj.ref);
    TEST_ASSERT(result != 0);
    
    count = uref_read(&obj.ref);
    TEST_ASSERT_EQUAL(2, count);

    /* 再次测试 */
    result = uref_get_unless_zero(&obj.ref);
    TEST_ASSERT(result != 0);
    count = uref_read(&obj.ref);
    TEST_ASSERT_EQUAL(3, count);

    return 0;
}

/**
 * @brief 测试uref_get_unless_zero失败情况
 * @return 成功返回0，失败返回-1
 */
static int test_uref_get_unless_zero_failure(void)
{
    struct test_object obj;
    int result;

    uref_init(&obj.ref);
    obj.data = 600;
    obj.released = false;

    /* 释放对象，使引用计数为0 */
    uref_put(&obj.ref, test_release);
    TEST_ASSERT(obj.released);

    /* 尝试在已释放的对象上增加引用 */
    result = uref_get_unless_zero(&obj.ref);
    TEST_ASSERT_EQUAL(0, result);

    return 0;
}

/**
 * @brief 测试UREF_INIT宏
 * @return 成功返回0，失败返回-1
 */
static int test_uref_init_macro(void)
{
    struct test_object obj = {
        .ref = UREF_INIT(5),
        .data = 700,
        .released = false
    };

    TEST_ASSERT_EQUAL(5, uref_read(&obj.ref));
    TEST_ASSERT_EQUAL(700, obj.data);
    TEST_ASSERT(!obj.released);

    return 0;
}

/**
 * @brief 测试复杂引用场景
 * @return 成功返回0，失败返回-1
 */
static int test_uref_complex_scenario(void)
{
    struct test_object obj;
    int result;
    unsigned int count;

    /* 初始化 */
    uref_init(&obj.ref);
    obj.data = 800;
    obj.released = false;

    /* 模拟多个用户获取引用 */
    for (int i = 0; i < 10; i++) {
        uref_get(&obj.ref);
    }
    count = uref_read(&obj.ref);
    TEST_ASSERT_EQUAL(11, count);

    /* 模拟多个用户释放引用 */
    for (int i = 0; i < 10; i++) {
        result = uref_put(&obj.ref, test_release);
        TEST_ASSERT_EQUAL(0, result);
        TEST_ASSERT(!obj.released);
    }

    /* 检查最终计数 */
    count = uref_read(&obj.ref);
    TEST_ASSERT_EQUAL(1, count);

    /* 最后释放 */
    result = uref_put(&obj.ref, test_release);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT(obj.released);

    return 0;
}

/**
 * @brief 测试边界条件
 * @return 成功返回0，失败返回-1
 */
static int test_uref_edge_cases(void)
{
    struct test_object obj;
    int result;

    /* 测试初始引用为0的情况 */
    refcount_set(&obj.ref.refcount, 0);
    obj.data = 900;
    obj.released = false;

    result = uref_get_unless_zero(&obj.ref);
    TEST_ASSERT_EQUAL(0, result);

    /* 测试高引用计数 */
    refcount_set(&obj.ref.refcount, 1000);
    result = uref_get_unless_zero(&obj.ref);
    TEST_ASSERT(result != 0);
    TEST_ASSERT_EQUAL(1001, uref_read(&obj.ref));

    /* 清理 */
    refcount_set(&obj.ref.refcount, 1);
    uref_put(&obj.ref, test_release);

    return 0;
}

/**
 * @brief 测试容器宏功能
 * @return 成功返回0，失败返回-1
 */
static int test_uref_container_macro(void)
{
    struct test_object obj;
    struct uref *uref_ptr;
    struct test_object *obj_ptr;

    uref_init(&obj.ref);
    obj.data = 1000;
    obj.released = false;

    /* 测试通过uref指针找到父对象 */
    uref_ptr = &obj.ref;
    obj_ptr = container_of(uref_ptr, struct test_object, ref);
    
    TEST_ASSERT_EQUAL(1000, obj_ptr->data);
    TEST_ASSERT(!obj_ptr->released);

    /* 释放对象 */
    uref_put(&obj.ref, test_release);

    return 0;
}

/**
 * @brief 测试多次初始化
 * @return 成功返回0，失败返回-1
 */
static int test_uref_reinit(void)
{
    struct test_object obj;
    int result;

    /* 第一次初始化和使用 */
    uref_init(&obj.ref);
    obj.data = 1100;
    obj.released = false;

    uref_get(&obj.ref);
    result = uref_put(&obj.ref, test_release);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT(!obj.released);

    result = uref_put(&obj.ref, test_release);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT(obj.released);

    /* 重新初始化并使用 */
    obj.released = false;
    uref_init(&obj.ref);
    
    TEST_ASSERT_EQUAL(1, uref_read(&obj.ref));
    TEST_ASSERT(!obj.released);

    result = uref_put(&obj.ref, test_release);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT(obj.released);

    return 0;
}

/**
 * @brief 主测试套件
 */
TEST_SUITE_BEGIN()

    TEST_RUN(test_uref_init_read);
    TEST_RUN(test_uref_get);
    TEST_RUN(test_uref_put_basic);
    TEST_RUN(test_uref_put_multiple);
    TEST_RUN(test_uref_get_unless_zero_success);
    TEST_RUN(test_uref_get_unless_zero_failure);
    TEST_RUN(test_uref_init_macro);
    TEST_RUN(test_uref_complex_scenario);
    TEST_RUN(test_uref_edge_cases);
    TEST_RUN(test_uref_container_macro);
    TEST_RUN(test_uref_reinit);

TEST_SUITE_END()