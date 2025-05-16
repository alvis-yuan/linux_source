/**  
 * @file extract_zip.c  
 * @brief 测试程序，用于从ZIP文件中提取指定文件  
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
    printf("Usage: %s <zip_file> <file_to_extract> [output_path] [output_filename]\n\n", program_name);  
    printf("Arguments:\n");  
    printf("  zip_file         - 要处理的ZIP文件路径\n");  
    printf("  file_to_extract  - 要提取的文件名(ZIP内的路径)\n");  
    printf("  output_path      - 输出目录路径（可选，默认为当前目录）\n");  
    printf("  output_filename  - 输出文件名（可选，默认使用原文件名）\n");  
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
    const char *output_path = ".";  /* 默认为当前目录 */  
    const char *output_filename = NULL;  /* 默认使用原文件名 */  
    int ret;  
      
    /* 检查命令行参数 */  
    if (argc < 3 || argc > 5) {  
        show_usage(argv[0]);  
        return 1;  
    }  
      
    zip_file = argv[1];  
    file_to_extract = argv[2];  
      
    /* 处理可选参数 */  
    if (argc >= 4) {  
        output_path = argv[3];  
    }  
      
    if (argc == 5) {  
        output_filename = argv[4];  
    }  
      
    /* 提取文件 */  
    printf("Extracting '%s' from '%s' to '%s'", file_to_extract, zip_file, output_path);  
    if (output_filename) {  
        printf(" as '%s'", output_filename);  
    }  
    printf("\n");  
      
    ret = unzip_extract_to_file(zip_file, output_path, file_to_extract, output_filename);  
      
    /* 处理返回结果 */  
    switch (ret) {  
    case UNZIP_OK:  
        printf("File extracted successfully.\n");  
        break;  
    case UNZIP_BADPARAM:  
        fprintf(stderr, "Error: Invalid parameters.\n");  
        return 1;  
    case UNZIP_ERRNO:  
        fprintf(stderr, "Error: File system error.\n");  
        return 1;  
    case UNZIP_NOTFOUND:  
        fprintf(stderr, "Error: File '%s' not found in the ZIP archive.\n", file_to_extract);  
        return 1;  
    case UNZIP_INTERNAL:  
        fprintf(stderr, "Error: Internal error during extraction.\n");  
        return 1;  
    default:  
        fprintf(stderr, "Error: Unknown error (code: %d).\n", ret);  
        return 1;  
    }  
      
    return 0;  
}