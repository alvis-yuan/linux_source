#define _GNU_SOURCE  // 启用 GNU 扩展
#include <stdio.h>
#include <printf.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

// 自定义 printf 处理器：处理 %b
static int printf_binary(FILE *stream, const struct printf_info *info, const void *const *args) {
    unsigned long long num;

    // 根据参数大小读取正确的类型
    if (info->is_long_double) {
        num = *(const unsigned long long *)args[0];
    } else if (info->is_long) {
        num = *(const unsigned long *)args[0];
    } else if (info->is_char) {
        num = *(const unsigned char *)args[0];
    } else if (info->is_short) {
        num = *(const unsigned short *)args[0];
    } else {
        num = *(const unsigned int *)args[0];
    }

    // 处理 # 标志（添加 0b 前缀）
    if (info->alt) {
        fprintf(stream, "0b");
    }

    // 打印二进制
    bool leading_zero = true;
    for (int i = sizeof(num) * 8 - 1; i >= 0; i--) {
        int bit = (num >> i) & 1;
        if (bit || !leading_zero || i == 0) {
            leading_zero = false;
            fprintf(stream, "%d", bit);
        }
    }

    return 0;
}

// 参数信息函数（告诉 printf 如何解析参数）
static int printf_binary_arginfo(const struct printf_info *info, size_t n, int *argtypes, int *size) {
    if (n > 0) {
        argtypes[0] = PA_INT;  // 参数类型是整数
    }
    return 1;
}

// 自定义 printf 处理器：处理%B
static int printf_bool(FILE *stream, const struct printf_info *info, const void *const *args) {
	bool value = *(const bool *)args[0];
    fprintf(stream, value ? "true" : "false");
    return 0;
}

// 参数信息函数（告诉 printf 如何解析参数）
static int printf_bool_arginfo(const struct printf_info *info, size_t n, int *argtypes, int *size) {
    if (n > 0) {
        argtypes[0] = PA_POINTER;  // 参数类型是字符串
    }
    return 1;
}

// 处理 %M（MAC 地址打印）
static int printf_upper_mac(FILE *stream, const struct printf_info *info, const void *const *args) {
    const uint8_t *mac = *(const uint8_t **)args[0];  // 假设传入的是 uint8_t[6]
    
	fprintf(stream, "%02X:%02X:%02X:%02X:%02X:%02X",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return 0;
}

// 参数信息函数（告诉 printf 如何解析参数）
static int printf_mac_arginfo(const struct printf_info *info, size_t n, int *argtypes, int *size) {
    if (n > 0) {
        argtypes[0] = PA_POINTER;  // MAC 地址通过指针传递（uint8_t*）
    }
    return 1;
}

// 处理 %*N（十六进制内存打印）
static int printf_hex_memory(FILE *stream, const struct printf_info *info, const void *const *args) {
    int length = *(const int *)args[0];      // 第一个参数：长度
    const uint8_t *addr = *(const uint8_t **)args[1];  // 第二个参数：地址

    for (int i = 0; i < length; i++) {
        fprintf(stream, "0x%02X ", addr[i]);   // 每字节两位十六进制，空格分隔
		
		if ((i+1)%16 == 0) {
			fprintf(stream, "\n");
		}
    }
    return 0;
}

// 参数信息函数（告诉 printf 如何解析参数）
static int printf_hex_memory_arginfo(const struct printf_info *info, size_t n, int *argtypes, int *size) {
    if (n >= 2) {
        argtypes[0] = PA_INT;      // 第一个参数是 int（长度）
        argtypes[1] = PA_POINTER;  // 第二个参数是指针（地址）
    }
    return 2;  // 声明需要 2 个参数
}

__attribute__((constructor))
static void register_custom_specifiers(void)
{
    // 注册 %b
    register_printf_specifier('b', printf_binary, printf_binary_arginfo);
    // 注册 %B
    register_printf_specifier('B', printf_bool, printf_bool_arginfo);
	// 注册 %M
    register_printf_specifier('M', printf_upper_mac, printf_mac_arginfo);
	// 注册 %N
	register_printf_specifier('N', printf_hex_memory, printf_hex_memory_arginfo);
}

static int custom_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);
    
    return ret;
}

int main() {

    unsigned int num = 42;

    // 测试
    custom_printf("Default: %b\n", num);       // 输出: 101010
    custom_printf("With prefix: %#b\n", num);  // 输出: 0b101010

	// 测试 %B（bool）
    bool flag1 = true;
    bool flag2 = false;
    custom_printf("Bool (true): %B\n", flag1);  // true
    custom_printf("Bool (false): %B\n", flag2); // false

	printf("----------%d-----\n", __LINE__);

	uint8_t data[34] = {};
	for (int i = 0; i < 34; i++) {
		data[i] = 0xa0 + i;
	}
    custom_printf("Memory: \n%N\n", 34, data);  // 输出: 00 11 22 33 44 55 66 77

	// 测试
    uint8_t mac[6] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    custom_printf("MAC Address: %M\n", mac);  // 输出: 00:1A:2B:3C:4D:5E

    return 0;
}
