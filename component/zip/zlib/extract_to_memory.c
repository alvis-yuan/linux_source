/**  
 * @file extract_to_memory.c  
 * @brief 测试程序，用于从ZIP文件中提取指定文件到内存  
 */  
  
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include "zip_utils.h"  
  
/**  
 * @brief 显示使用帮助  
 *  
 * @param program_name 程序名称  
 */  
static void show_usage(const char *program_name)  
{  
    printf("Usage: %s <zip_file> <file_to_extract>\n\n", program_name);  
    printf("Arguments:\n");  
    printf("  zip_file         - 要处理的ZIP文件路径\n");  
    printf("  file_to_extract  - 要提取的文件名（ZIP内的路径）\n");  
}  
  
/**  
 * @brief 主函数  
 *  
 * @param argc 命令行参数数量  
 * @param argv 命令行参数数组  
 * @return 程序退出码  
 */  
int main(int argc, char *argv[])  
{  
    const char *zip_file;  
    const char *file_to_extract;  
    char *buffer = NULL;  
    int buffer_size = UNZIP_BUFFER_SIZE;  /* 初始缓冲区大小 */  
    int result;  
    int i;  
      
    /* 检查命令行参数 */  
    if (argc != 3) {  
        show_usage(argv[0]);  
        return 1;  
    }  
      
    zip_file = argv[1];  
    file_to_extract = argv[2];  
      
    printf("Extracting '%s' from '%s' to memory\n", file_to_extract, zip_file);  
      
    /* 分配初始缓冲区 */  
    buffer = (char *)malloc(buffer_size);  
    if (!buffer) {  
        fprintf(stderr, "Error: Failed to allocate memory.\n");  
        return 1;  
    }  
      
    /* 提取文件到内存 */  
    result = unzip_extract_to_buf(zip_file, (char *)file_to_extract, buffer, buffer_size);  
      
    /* 处理返回结果 */  
    if (result < 0) {  
        switch (result) {  
        case UNZIP_BADPARAM:  
            fprintf(stderr, "Error: Invalid parameters.\n");  
            break;  
        case UNZIP_ERRNO:  
            fprintf(stderr, "Error: File system error.\n");  
            break;  
        case UNZIP_NOTFOUND:  
            fprintf(stderr, "Error: File '%s' not found in the ZIP archive.\n", file_to_extract);  
            break;  
        case UNZIP_INTERNAL:  
            fprintf(stderr, "Error: Internal error during extraction.\n");  
            break;  
        default:  
            fprintf(stderr, "Error: Unknown error (code: %d).\n", result);  
        }  
        free(buffer);  
        return 1;  
    }  
      
    /* 如果缓冲区太小，重新分配并再次尝试 */  
    if (result == buffer_size) {  
        /* 可能缓冲区太小，尝试更大的缓冲区 */  
        char *new_buffer;  
        buffer_size *= 2;  /* 扩大缓冲区 */  
          
        printf("Buffer might be too small, trying with larger buffer (%d bytes)...\n", buffer_size);  
          
        new_buffer = (char *)realloc(buffer, buffer_size);  
        if (!new_buffer) {  
            fprintf(stderr, "Error: Failed to reallocate memory.\n");  
            free(buffer);  
            return 1;  
        }  
          
        buffer = new_buffer;  
        result = unzip_extract_to_buf(zip_file, (char *)file_to_extract, buffer, buffer_size);  
          
        if (result < 0) {  
            fprintf(stderr, "Error: Failed to extract file (code: %d).\n", result);  
            free(buffer);  
            return 1;  
        }  
    }  
      
    /* 显示提取结果 */  
    printf("Successfully extracted %d bytes.\n\n", result);  
      
    /* 显示文件内容（如果是文本文件） */  
    printf("File content preview (first 100 bytes or less):\n");  
    printf("----------------------------------------\n");  
      
    /* 检查是否为文本文件 */  
    int is_text = 1;  
    for (i = 0; i < result && i < 100; i++) {  
        if (buffer[i] == 0 || (unsigned char)buffer[i] > 127) {  
            is_text = 0;  
            break;  
        }  
    }  
      
    if (is_text) {  
        /* 显示文本内容 */  
        int preview_size = (result < 100) ? result : 100;  
        printf("%.*s\n", preview_size, buffer);  
    } else {  
        /* 显示十六进制内容 */  
        printf("Binary content (hex dump):\n");  
        for (i = 0; i < result && i < 50; i++) {  
            printf("%02x ", (unsigned char)buffer[i]);  
            if ((i + 1) % 16 == 0)  
                printf("\n");  
        }  
        printf("\n");  
    }  
      
    printf("----------------------------------------\n");  
      
    /* 释放内存 */  
    free(buffer);  
      
    return 0;  
}