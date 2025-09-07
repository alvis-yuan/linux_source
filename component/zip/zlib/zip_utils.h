/**  
 * @file zip_utils.h  
 * @brief 基于zlib的minizip接口实现的解压缩工具  
 */  
  
#ifndef _ZIP_UTILS_H_  
#define _ZIP_UTILS_H_  

#define _GNU_SOURCE

#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <sys/stat.h>  
#include <sys/types.h>  
#include <unistd.h>  
#include <errno.h>  
  
/* 日志定义 */  
#define LogError(fmt, arg...) fprintf(stderr, "[%s-%d] " fmt "\n", __func__, __LINE__, ##arg)  
#define LogInfo(fmt, arg...) fprintf(stderr, "[%s-%d] " fmt "\n", __func__, __LINE__, ##arg)  
  
/* 错误码定义 */  
#define UNZIP_OK            0  
#define UNZIP_ERRNO        -1  
#define UNZIP_BADPARAM     -2  
#define UNZIP_NOMEM        -3  
#define UNZIP_NOTFOUND     -4  
#define UNZIP_INTERNAL     -5  
#define UNZIP_BADZIP       -6  
  
/* 缓冲区大小定义 */  
#define UNZIP_BUFFER_SIZE  8192  
  
/* 文件信息结构体 */  
struct unzip_files_info {  
    char filename[256];  
    unsigned long uncompressed_size;  
    int is_dir;  
};  
  
/**  
 * @brief 获取压缩文件中的文件信息  
 *  
 * @param zip_file 压缩文件路径  
 * @param files_info 输出参数，存放文件信息的结构体数组  
 * @return 压缩文件中的文件数量，错误返回0  
 */  
int unzip_get_files_info(const char *zip_file, struct unzip_files_info **files_info);  
  
/**  
 * @brief 解压指定文件到内存  
 *  
 * @param zip_file 压缩文件路径  
 * @param fname 要解压的文件名  
 * @param outb 输出缓冲区  
 * @param len 输出缓冲区长度  
 * @return 成功返回解压后的数据长度，失败返回错误码  
 */  
int unzip_extract_to_buf(const char *zip_file, char *fname, char *outb, int len);  
  
/**  
 * @brief 解压指定文件到文件系统  
 *  
 * @param zip_file 压缩文件路径  
 * @param output_path 输出目录路径  
 * @param file_to_extract 要解压的文件名  
 * @param out_file 解压后的文件名，如果为NULL，则使用file_to_extract  
 * @return 成功返回UNZIP_OK，失败返回错误码  
 */  
int unzip_extract_to_file(const char *zip_file, const char *output_path,   
                          const char *file_to_extract, const char *out_file); 
						  
/**  
 * @brief 压缩文件到ZIP文件  
 *  
 * @param zip_file 目标ZIP文件路径  
 * @param src_file 要压缩的源文件路径  
 * @param file_name_in_zip ZIP中的文件名，如果为NULL则使用src_file的文件名  
 * @return 成功返回UNZIP_OK，失败返回错误码  
 */  
int zip_file(const char *zip_file, const char *src_file, const char *file_name_in_zip);  
  
/**  
 * @brief 压缩内存数据到ZIP文件  
 *  
 * @param zip_file 目标ZIP文件路径  
 * @param buf 要压缩的数据缓冲区  
 * @param len 数据长度  
 * @param file_name_in_zip ZIP中的文件名  
 * @return 成功返回UNZIP_OK，失败返回错误码  
 */  
int zip_buffer(const char *zip_file, const char *buf, int len, const char *file_name_in_zip);  
  
/**  
 * @brief 压缩目录到ZIP文件  
 *  
 * @param zip_file 目标ZIP文件路径  
 * @param src_dir 要压缩的源目录路径  
 * @param exclude_root 是否排除根目录名  
 * @return 成功返回UNZIP_OK，失败返回错误码  
 */  
int zip_directory(const char *zip_file, const char *src_dir, int exclude_root);
  
#endif /* _ZIP_UTILS_H_ */
