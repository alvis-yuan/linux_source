#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {  
    uint32_t crc;  
} CRC32_CTX;

/* CRC32 标准多项式 */  
#define CRC32_POLY 0xEDB88320UL  
  
/* CRC32 表 - 用于优化计算 */  
static uint32_t crc32_table[256];  
static int crc32_table_initialized = 0;  
  
/* 初始化 CRC32 表 */  
static void init_crc32_table(void)  
{  
    uint32_t c;  
    int i, j;  
  
    for (i = 0; i < 256; i++) {  
        c = (uint32_t)i;  
        for (j = 0; j < 8; j++) {  
            c = (c & 1) ? (CRC32_POLY ^ (c >> 1)) : (c >> 1);  
        }  
        crc32_table[i] = c;  
    }  
    crc32_table_initialized = 1;  
}  
  
int CRC32_Init(CRC32_CTX *ctx)  
{  
    if (ctx == NULL)  
        return 0;  
      
    if (!crc32_table_initialized)  
        init_crc32_table();  
      
    ctx->crc = 0xFFFFFFFFUL;  
    return 1;  
}  
  
int CRC32_Update(CRC32_CTX *ctx, const void *data, size_t len)  
{  
    const unsigned char *buf = (const unsigned char *)data;  
    size_t i;  
      
    if (ctx == NULL || data == NULL)  
        return 0;  
      
    for (i = 0; i < len; i++) {  
        ctx->crc = crc32_table[(ctx->crc ^ buf[i]) & 0xFF] ^ (ctx->crc >> 8);  
    }  
      
    return 1;  
}  
  
int CRC32_Final(unsigned char *md, CRC32_CTX *ctx)  
{  
    uint32_t crc;  
      
    if (md == NULL || ctx == NULL)  
        return 0;  
      
    crc = ctx->crc ^ 0xFFFFFFFFUL;  
      
    /* 以小端字节序存储结果 */  
    md[0] = (unsigned char)(crc & 0xFF);  
    md[1] = (unsigned char)((crc >> 8) & 0xFF);  
    md[2] = (unsigned char)((crc >> 16) & 0xFF);  
    md[3] = (unsigned char)((crc >> 24) & 0xFF);  
      
    return 1;  
}  
  
unsigned int CRC32(const void *data, size_t len)  
{  
    CRC32_CTX ctx;  
    unsigned char md[4];  
    uint32_t crc;  
      
    CRC32_Init(&ctx);  
    CRC32_Update(&ctx, data, len);  
    CRC32_Final(md, &ctx);  
      
    crc = ((uint32_t)md[3] << 24) |   
          ((uint32_t)md[2] << 16) |   
          ((uint32_t)md[1] << 8) |   
          (uint32_t)md[0];  
      
    return crc;  
}


/* 已知的测试向量 */  
struct test_vector {  
    const char *data;  
    uint32_t expected;  
};  
  
static struct test_vector test_vectors[] = {  
    {"", 0x00000000},  
    {"a", 0xE8B7BE43},  
    {"abc", 0x352441C2},  
    {"message digest", 0x20159D7F},  
    {"abcdefghijklmnopqrstuvwxyz", 0x4C2750BD},  
    {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 0x1FC2E6D2},  
    {"12345678901234567890123456789012345678901234567890123456789012345678901234567890", 0x7CA94A72},  
    {NULL, 0}  
};  
  
/* 测试单步计算 */  
static int test_crc32_one_shot(void)  
{  
    struct test_vector *tv;  
    uint32_t result;  
    int failed = 0;  
  
    printf("Testing CRC32 one-shot calculation:\n");  
      
    for (tv = test_vectors; tv->data != NULL; tv++) {  
        result = CRC32(tv->data, strlen(tv->data));  
          
        printf("  Input: \"%s\"\n", tv->data);  
        printf("  Expected: 0x%08X, Got: 0x%08X ... %s\n",   
               tv->expected, result,   
               (result == tv->expected) ? "OK" : "FAILED");  
          
        if (result != tv->expected)  
            failed++;  
    }  
      
    return failed;  
}  
  
/* 测试分步计算 */  
static int test_crc32_incremental(void)  
{  
    struct test_vector *tv;  
    CRC32_CTX ctx;  
    unsigned char md[4];  
    uint32_t result;  
    int failed = 0;  
    size_t i, len;  
  
    printf("\nTesting CRC32 incremental calculation:\n");  
      
    for (tv = test_vectors; tv->data != NULL; tv++) {  
        CRC32_Init(&ctx);  
          
        /* 每次处理一个字符 */  
        len = strlen(tv->data);  
        for (i = 0; i < len; i++) {  
            CRC32_Update(&ctx, &tv->data[i], 1);  
        }  
          
        CRC32_Final(md, &ctx);  
        result = ((uint32_t)md[3] << 24) |   
                 ((uint32_t)md[2] << 16) |   
                 ((uint32_t)md[1] << 8) |   
                 (uint32_t)md[0];  
          
        printf("  Input: \"%s\" (incremental)\n", tv->data);  
        printf("  Expected: 0x%08X, Got: 0x%08X ... %s\n",   
               tv->expected, result,   
               (result == tv->expected) ? "OK" : "FAILED");  
          
        if (result != tv->expected)  
            failed++;  
    }  
      
    return failed;  
}  
  
/* 测试空数据和大数据 */  
static int test_crc32_edge_cases(void)  
{  
    CRC32_CTX ctx;  
    unsigned char md[4];  
    uint32_t result;  
    int failed = 0;  
    char *large_data;  
    size_t large_size = 1024 * 1024; /* 1MB */  
      
    printf("\nTesting CRC32 edge cases:\n");  
      
    /* 测试空数据 */  
    CRC32_Init(&ctx);  
    CRC32_Final(md, &ctx);  
    result = ((uint32_t)md[3] << 24) |   
             ((uint32_t)md[2] << 16) |   
             ((uint32_t)md[1] << 8) |   
             (uint32_t)md[0];  
      
    printf("  Empty data\n");  
    printf("  Expected: 0x00000000, Got: 0x%08X ... %s\n",   
           result, (result == 0) ? "OK" : "FAILED");  
      
    if (result != 0)  
        failed++;  
      
    /* 测试大数据 */  
    large_data = malloc(large_size);  
    if (large_data == NULL) {  
        printf("  Large data test: SKIPPED (memory allocation failed)\n");  
    } else {  
        /* 用递增的值填充数据 */  
        for (size_t i = 0; i < large_size; i++) {  
            large_data[i] = (char)(i & 0xFF);  
        }  
          
        /* 计算 CRC32 */  
        CRC32_Init(&ctx);  
        CRC32_Update(&ctx, large_data, large_size);  
        CRC32_Final(md, &ctx);  
        result = ((uint32_t)md[3] << 24) |   
                 ((uint32_t)md[2] << 16) |   
                 ((uint32_t)md[1] << 8) |   
                 (uint32_t)md[0];  
          
        printf("  Large data (1MB)\n");  
        printf("  CRC32: 0x%08X\n", result);  
          
        free(large_data);  
    }  
      
    return failed;  
}  
  
int main(void)  
{  
    int failed = 0;  
      
    failed += test_crc32_one_shot();  
    failed += test_crc32_incremental();  
    failed += test_crc32_edge_cases();  
      
    if (failed == 0) {  
        printf("\nAll tests PASSED!\n");  
        return 0;  
    } else {  
        printf("\n%d tests FAILED!\n", failed);  
        return 1;  
    }  
}