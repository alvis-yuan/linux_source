#ifndef _VOS_H_
#define _VOS_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>

/**
 * @def LogError
 * @brief 错误日志输出宏
 */
#define LogError(fmt, arg...) \
    fprintf(stderr, "[ERROR %s-%d] " fmt "\n", __func__, __LINE__, ##arg)

/**
 * @def LogInfo
 * @brief 信息日志输出宏
 */
#define LogInfo(fmt, arg...) \
    fprintf(stderr, "[INFO %s-%d] " fmt "\n", __func__, __LINE__, ##arg)

#define container_of(ptr, type, member) ({ \
    const typeof( ((type *)0)->member ) *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type,member) ); \
})

#define BUG() do { \
	LogError("BUG: failure\n"); \
} while (0)

#define BUG_ON(condition) do { if (condition) BUG(); } while (0)

#include "atomic.h"
#include "refcount.h"
#include "uref.h"
#include "list.h"
#include "voslock.h"
#include "unittest.h"

#endif /* _VOS_H_ */
