/**
 * @file unittest.h
 * @brief 简单单元测试框架头文件
 */
#ifndef _UNITTEST_H
#define _UNITTEST_H

#include "vos.h"

/**
 * @def TEST_ASSERT
 * @brief 断言宏，检查条件是否为真
 * @param condition 要检查的条件表达式
 */
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            LogError("Assertion FAIL: %s", #condition); \
            exit(EXIT_FAILURE); \
        } else { \
            LogInfo("Assertion PASS: %s", #condition); \
        } \
    } while (0)

/**
 * @def TEST_ASSERT_EQUAL
 * @brief 断言两个值相等
 * @param expected 期望值
 * @param actual 实际值
 */
#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            LogError("Expected %ld, got %ld", (long)(expected), (long)(actual)); \
            exit(EXIT_FAILURE); \
        } else { \
            LogInfo("Assertion PASS: %s equal %s", #expected, #actual); \
        } \
    } while (0)

/**
 * @def TEST_ASSERT_STR_EQUAL
 * @brief 断言两个字符串相等
 * @param expected 期望字符串
 * @param actual 实际字符串
 */
#define TEST_ASSERT_STR_EQUAL(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            LogError("Expected '%s', got '%s'", (expected), (actual)); \
            exit(EXIT_FAILURE); \
        } else { \
            LogInfo("Assertion PASS: %s equal %s", expected, actual); \
        } \
    } while (0)

/**
 * @def TEST_RUN
 * @brief 运行测试用例
 * @param test_func 测试函数指针
 */
#define TEST_RUN(test_func) \
    do { \
        LogInfo("Running test: %s", #test_func); \
        int ret = test_func(); \
        if (ret == 0) { \
            LogInfo("Test %s PASSED", #test_func); \
            passed_count++; \
        } else { \
            LogError("Test %s FAILED", #test_func); \
            failed_count++; \
        } \
        total_count++; \
    } while (0)

/**
 * @def TEST_SUITE_BEGIN
 * @brief 测试套件开始宏
 */
#define TEST_SUITE_BEGIN() \
    int main(void) \
    { \
        int passed_count = 0; \
        int failed_count = 0; \
        int total_count = 0; \
        LogInfo("Test suite started");

/**
 * @def TEST_SUITE_END
 * @brief 测试套件结束宏
 */
#define TEST_SUITE_END() \
        LogInfo("Test suite completed: %d passed, %d failed, %d total", \
               passed_count, failed_count, total_count); \
        if (failed_count > 0) { \
            LogError("Some tests failed!"); \
            return EXIT_FAILURE; \
        } \
        LogInfo("All tests passed!"); \
        return EXIT_SUCCESS; \
    }

#endif /* _UNITTEST_H */