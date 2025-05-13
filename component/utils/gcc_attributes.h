#ifndef GCC_ATTRIBUTES_H
#define GCC_ATTRIBUTES_H

// 仅在GCC或Clang编译器下启用属性扩展
#if defined(__GNUC__) || defined(__clang__)

/*****************************
 * 函数属性（Function Attributes）
 *****************************/

/**
 * @brief 标记函数不会返回（如exit()或无限循环）。
 * @example NORETURN void fatal_error() { exit(1); }
 */
#define NORETURN __attribute__((noreturn))

/**
 * @brief 强制函数内联，忽略编译器的优化决策。
 * @example ALWAYS_INLINE int add(int a, int b) { return a + b; }
 */
#define ALWAYS_INLINE __attribute__((always_inline))

/**
 * @brief 禁止函数内联，用于调试或避免优化干扰。
 * @example NOINLINE void debug_func() { /* 复杂逻辑 */ }
 */
#define NOINLINE __attribute__((noinline))

/**
 * @brief 标记函数为"冷"（很少执行），优化其代码布局以减少缓存占用。
 * @example COLD void error_handler() { /* 罕见错误处理 */ }
 */
#define COLD __attribute__((cold))

/**
 * @brief 标记函数为"热"（频繁执行），优化其性能和缓存局部性。
 * @example HOT void critical_loop() { /* 高频代码 */ }
 */
#define HOT __attribute__((hot))

/**
 * @brief 指定函数的调用约定（如`stdcall`）。
 * @example CALL_CONV("stdcall") void api_func();
 */
#define CALL_CONV(convention) __attribute__((stdcall))

/**
 * @brief 标记函数为弱符号，允许被其他同名函数覆盖。
 * @example WEAK void default_impl() { /* 可被覆盖的实现 */ }
 */
#define WEAK __attribute__((weak))

/**
 * @brief 函数参数为格式化字符串，启用编译器格式检查（如printf风格）。
 * @param fmt_pos 格式化字符串参数位置（从1开始）。
 * @param va_pos 可变参数起始位置。
 * @example FORMAT(printf, 1, 2) void log(const char* fmt, ...);
 */
#define FORMAT(type, fmt_pos, va_pos) __attribute__((format(type, fmt_pos, va_pos)))

/**
 * @brief 清理函数：当变量离开作用域时自动调用指定函数（类似RAII）。
 * @param cleanup_fn 清理函数名（需接受一个指向变量的指针）。
 * @example CLEANUP(free_ptr) void* ptr = malloc(100);
 */
#define CLEANUP(cleanup_fn) __attribute__((cleanup(cleanup_fn)))

/**
 * @brief 标记函数为构造函数（在main()前执行）。
 * @param priority 可选优先级（数字越小越早执行，默认无优先级）。
 * @example CONSTRUCTOR void init() { ... }
 */
#define CONSTRUCTOR(priority) __attribute__((constructor(priority)))

/**
 * @brief 标记函数为析构函数（在main()后执行）。
 * @param priority 可选优先级（数字越小越晚执行，默认无优先级）。
 * @example DESTRUCTOR void cleanup() { ... }
 */
#define DESTRUCTOR(priority) __attribute__((destructor(priority)))

/**
 * @brief 标记函数为导出函数（可被其他模块调用）。 
 * @example API_EXPORT void api_function() { ... }
 */
#define API_EXPORT __attribute__((visibility("default")))

/*****************************
 * 变量属性（Variable Attributes）
 *****************************/

/**
 * @brief 指定变量或结构体字段的对齐方式（字节数）。
 * @example ALIGNED(16) float vector[4]; // 对齐到16字节
 */
#define ALIGNED(bytes) __attribute__((aligned(bytes)))

/**
 * @brief 取消结构体/联合体的填充（节省内存，但可能降低性能）。
 * @example PACKED struct SensorData { char id; int value; };
 */
#define PACKED __attribute__((packed))

/**
 * @brief 将变量放入指定section（如自定义内存段）。
 * @example SECTION(".boot_data") uint32_t boot_flags;
 */
#define SECTION(section_name) __attribute__((section(section_name)))

/**
 * @brief 标记变量为未初始化（避免编译器清零初始化）。
 * @example UNINIT int fast_buffer[1024];
 */
#define UNINIT __attribute__((section(".uninit")))

/**
 * @brief 线程局部存储（TLS），每个线程拥有独立的变量实例。
 * @example THREAD_LOCAL int thread_id;
 */
#define THREAD_LOCAL __attribute__((thread))

/*****************************
 * 类型属性（Type Attributes）
 *****************************/

/**
 * @brief 标记枚举类型为位掩码（允许按位操作）。
 * @example BITMASK_ENUM enum Flags { A = 1 << 0, B = 1 << 1 };
 */
#define BITMASK_ENUM __attribute__((flag_enum))

/*****************************
 * 编译器优化控制
 *****************************/

/**
 * @brief 分支预测优化：提示条件大概率成立。
 * @example if (LIKELY(x > 0)) { /* 快速路径 */ }
 */
#define LIKELY(x) __builtin_expect(!!(x), 1)

/**
 * @brief 分支预测优化：提示条件大概率不成立。
 * @example if (UNLIKELY(error)) { /* 错误处理 */ }
 */
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

/**
 * @brief 禁止尾调用优化（用于调试或特定场景）。
 * @example NO_TAIL_CALL void recursive_func();
 */
#define NO_TAIL_CALL __attribute__((disable_tail_calls))

/*****************************
 * 其他实用属性
 *****************************/

/**
 * @brief 禁用警告（用于特定代码段）。
 * @example SUPPRESS_WARNING("-Wunused") int unused_var;
 */
#define SUPPRESS_WARNING(warning) __attribute__((diagnostic_ignore(warning)))

#else
// 非GCC/Clang编译器时定义为空宏
#define NORETURN
#define ALWAYS_INLINE
#define NOINLINE
#define COLD
#define HOT
#define CALL_CONV(convention)
#define WEAK
#define FORMAT(type, fmt_pos, va_pos)
#define CLEANUP(cleanup_fn)
#define ALIGNED(bytes)
#define PACKED
#define SECTION(section_name)
#define UNINIT
#define THREAD_LOCAL
#define BITMASK_ENUM
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#define NO_TAIL_CALL
#define SUPPRESS_WARNING(warning)
#define CONSTRUCTOR(priority)
#define DESTRUCTOR(priority)


#endif // __GNUC__ || __clang__

#endif // GCC_ATTRIBUTES_H