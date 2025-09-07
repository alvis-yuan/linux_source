/**  
 * @file list_zip.c  
 * @brief 测试程序，用于列出ZIP文件中的内容  
 */  
  
#include <stdio.h>  
#include <stdlib.h>  
#include "zip_utils.h"  
  
/**  
 * @brief 将文件大小转换为可读格式  
 *  
 * @param size 文件大小（字节）  
 * @param buf 输出缓冲区  
 * @param buf_size 缓冲区大小  
 */  
static void format_size(unsigned long size, char *buf, size_t buf_size)  
{  
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};  
    int unit_index = 0;  
    double size_d = (double)size;  
      
    while (size_d >= 1024.0 && unit_index < 4) {  
        size_d /= 1024.0;  
        unit_index++;  
    }  
      
    if (unit_index == 0) {  
        snprintf(buf, buf_size, "%lu %s", size, units[unit_index]);  
    } else {  
        snprintf(buf, buf_size, "%.2f %s", size_d, units[unit_index]);  
    }  
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
    struct unzip_files_info *files_info = NULL;  
    int num_files;  
    int i;  
    char size_str[32];  
      
    /* 检查命令行参数 */  
    if (argc != 2) {  
        printf("Usage: %s <zip_file>\n", argv[0]);  
        return 1;  
    }  
      
    zip_file = argv[1];  
      
    /* 获取ZIP文件信息 */  
    num_files = unzip_get_files_info(zip_file, &files_info);  
    if (num_files == 0) {  
        fprintf(stderr, "Failed to get information from %s or the file is empty\n", zip_file);  
        return 1;  
    }  
      
    /* 打印文件信息 */  
    printf("ZIP file: %s\n", zip_file);  
    printf("Total files: %d\n\n", num_files);  
    printf("%-40s %-12s %s\n", "Filename", "Size", "Type");  
    printf("%-40s %-12s %s\n", "--------", "----", "----");  
      
    for (i = 0; i < num_files; i++) {  
        format_size(files_info[i].uncompressed_size, size_str, sizeof(size_str));  
        printf("%-40s %-12s %s\n",   
               files_info[i].filename,  
               size_str,  
               files_info[i].is_dir ? "Directory" : "File");  
    }  
      
    /* 释放内存 */  
    free(files_info);  
      
    return 0;  
}