/**  
 * @file unzip_utils.c  
 * @brief 基于zlib的minizip接口实现的解压缩工具  
 */  
  
#include "zip_utils.h"  
#include "unzip.h"
#include "zip.h"
#include <dirent.h>
  

/**  
 * @brief 获取压缩文件中的文件信息  
 *  
 * @param zip_file 压缩文件路径  
 * @param files_info 输出参数，存放文件信息的结构体数组  
 * @return 压缩文件中的文件数量，错误返回0  
 */  
int unzip_get_files_info(const char *zip_file, struct unzip_files_info **files_info)  
{  
    unzFile uf = NULL;  
    unz_global_info gi;  
    unz_file_info file_info;  
    char filename_inzip[256];  
    int err;  
    struct unzip_files_info *info = NULL;  
    int i = 0;  
  
    /* 参数检查 */  
    if (!zip_file || !files_info) {  
        LogError("Invalid parameters");  
        return 0;  
    }  
  
    /* 打开zip文件 */  
    uf = unzOpen(zip_file);  
    if (!uf) {  
        LogError("Cannot open %s", zip_file);  
        return 0;  
    }  
  
    /* 获取全局信息 */  
    err = unzGetGlobalInfo(uf, &gi);  
    if (err != UNZ_OK) {  
        LogError("Error %d with zipfile in unzGetGlobalInfo", err);  
        goto err_close;  
    }  
  
    /* 分配内存 */  
    info = (struct unzip_files_info *)malloc(sizeof(struct unzip_files_info) * gi.number_entry);  
    if (!info) {  
        LogError("Failed to allocate memory");  
        goto err_close;  
    }  
    memset(info, 0, sizeof(struct unzip_files_info) * gi.number_entry);  
  
    /* 遍历所有文件 */  
    err = unzGoToFirstFile(uf);  
    if (err != UNZ_OK) {  
        LogError("Error %d with zipfile in unzGoToFirstFile", err);  
        goto err_free;  
    }  
  
    for (i = 0; i < gi.number_entry; i++) {  
        /* 获取当前文件信息 */  
        err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip),  
                                     NULL, 0, NULL, 0);  
        if (err != UNZ_OK) {  
            LogError("Error %d with zipfile in unzGetCurrentFileInfo", err);  
            goto err_free;  
        }  
  
        /* 填充文件信息 */  
        strncpy(info[i].filename, filename_inzip, sizeof(info[i].filename) - 1);  
        info[i].uncompressed_size = file_info.uncompressed_size;  
          
        /* 判断是否为目录 */  
        size_t len = strlen(filename_inzip);  
        info[i].is_dir = (len > 0 && (filename_inzip[len-1] == '/' || filename_inzip[len-1] == '\\'));  
  
        /* 移动到下一个文件 */  
        if ((i + 1) < gi.number_entry) {  
            err = unzGoToNextFile(uf);  
            if (err != UNZ_OK) {  
                LogError("Error %d with zipfile in unzGoToNextFile", err);  
                goto err_free;  
            }  
        }  
    }  
  
    /* 关闭zip文件 */  
    unzClose(uf);  
      
    *files_info = info;  
    return gi.number_entry;  
  
err_free:  
    free(info);  
err_close:  
    unzClose(uf);  
    return 0;  
}  
  
/**  
 * @brief 解压指定文件到内存  
 *  
 * @param zip_file 压缩文件路径  
 * @param fname 要解压的文件名  
 * @param outb 输出缓冲区  
 * @param len 输出缓冲区长度  
 * @return 成功返回解压后的数据长度，失败返回错误码  
 */  
int unzip_extract_to_buf(const char *zip_file, char *fname, char *outb, int len)  
{  
    unzFile uf = NULL;  
    int err;  
    int read_len = 0;  
    int total_read = 0;  
  
    /* 参数检查 */  
    if (!zip_file || !fname || !outb || len <= 0) {  
        LogError("Invalid parameters");  
        return UNZIP_BADPARAM;  
    }  
  
    /* 打开zip文件 */  
    uf = unzOpen(zip_file);  
    if (!uf) {  
        LogError("Cannot open %s", zip_file);  
        return UNZIP_ERRNO;  
    }  
  
    /* 定位到指定文件 */  
    err = unzLocateFile(uf, fname, 0);  
    if (err != UNZ_OK) {  
        LogError("File %s not found in the zipfile", fname);  
        unzClose(uf);  
        return UNZIP_NOTFOUND;  
    }  
  
    /* 打开当前文件 */  
    err = unzOpenCurrentFile(uf);  
    if (err != UNZ_OK) {  
        LogError("Error %d with zipfile in unzOpenCurrentFile", err);  
        unzClose(uf);  
        return UNZIP_INTERNAL;  
    }  
  
    /* 读取文件内容 */  
    while (total_read < len) {  
        read_len = unzReadCurrentFile(uf, outb + total_read, len - total_read);  
        if (read_len < 0) {  
            LogError("Error %d with zipfile in unzReadCurrentFile", read_len);  
            unzCloseCurrentFile(uf);  
            unzClose(uf);  
            return UNZIP_INTERNAL;  
        }  
        if (read_len == 0)  
            break; /* 文件结束 */  
  
        total_read += read_len;  
    }  
  
    /* 关闭当前文件和zip文件 */  
    unzCloseCurrentFile(uf);  
    unzClose(uf);  
  
    return total_read;  
}  
  
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
                          const char *file_to_extract, const char *out_file)  
{  
    unzFile uf = NULL;  
    FILE *fout = NULL;  
    char buffer[UNZIP_BUFFER_SIZE];  
    char output_filename[512];  
    char *filename_withoutpath = NULL;  
    char *p = NULL;  
    int err;  
    int read_len;  
    struct stat st;  
  
    /* 参数检查 */  
    if (!zip_file || !output_path || !file_to_extract) {  
        LogError("Invalid parameters");  
        return UNZIP_BADPARAM;  
    }  
  
    /* 检查输出目录是否存在 */  
    if (stat(output_path, &st) != 0 || !S_ISDIR(st.st_mode)) {  
        LogError("Output path %s does not exist or is not a directory", output_path);  
        return UNZIP_ERRNO;  
    }  
  
    /* 打开zip文件 */  
    uf = unzOpen(zip_file);  
    if (!uf) {  
        LogError("Cannot open %s", zip_file);  
        return UNZIP_ERRNO;  
    }  
  
    /* 定位到指定文件 */  
    err = unzLocateFile(uf, file_to_extract, 0);  
    if (err != UNZ_OK) {  
        LogError("File %s not found in the zipfile", file_to_extract);  
        unzClose(uf);  
        return UNZIP_NOTFOUND;  
    }  
  
    /* 打开当前文件 */  
    err = unzOpenCurrentFile(uf);  
    if (err != UNZ_OK) {  
        LogError("Error %d with zipfile in unzOpenCurrentFile", err);  
        unzClose(uf);  
        return UNZIP_INTERNAL;  
    }  
  
    /* 确定输出文件名 */  
    if (out_file) {  
        snprintf(output_filename, sizeof(output_filename), "%s/%s", output_path, out_file);  
    } else {  
        /* 提取文件名部分 */  
        filename_withoutpath = (char *)file_to_extract;  
        for (p = (char *)file_to_extract; *p; p++) {  
            if (*p == '/' || *p == '\\')  
                filename_withoutpath = p + 1;  
        }  
        snprintf(output_filename, sizeof(output_filename), "%s/%s", output_path, filename_withoutpath);  
    }  
  
    /* 创建输出文件 */  
    fout = fopen(output_filename, "wb");  
    if (!fout) {  
        LogError("Error opening %s for writing", output_filename);  
        unzCloseCurrentFile(uf);  
        unzClose(uf);  
        return UNZIP_ERRNO;  
    }  
  
    /* 读取并写入文件内容 */  
    LogInfo("Extracting %s to %s", file_to_extract, output_filename);  
    do {  
        read_len = unzReadCurrentFile(uf, buffer, sizeof(buffer));  
        if (read_len < 0) { 
            LogError("Error %d with zipfile in unzReadCurrentFile", read_len);  
            fclose(fout);  
            unzCloseCurrentFile(uf);  
            unzClose(uf);  
            return UNZIP_INTERNAL;  
        }  
          
        if (read_len == 0)  
            break; /* 文件结束 */  
              
        if (fwrite(buffer, read_len, 1, fout) != 1) {  
            LogError("Error writing to file %s", output_filename);  
            fclose(fout);  
            unzCloseCurrentFile(uf);  
            unzClose(uf);  
            return UNZIP_ERRNO;  
        }  
    } while (read_len > 0);  
  
    /* 关闭文件 */  
    fclose(fout);  
      
    /* 关闭当前文件和zip文件 */  
    err = unzCloseCurrentFile(uf);  
    if (err != UNZ_OK) {  
        LogError("Error %d with zipfile in unzCloseCurrentFile", err);  
        unzClose(uf);  
        return UNZIP_INTERNAL;  
    }  
      
    unzClose(uf);  
    LogInfo("Successfully extracted %s", file_to_extract);  
    return UNZIP_OK;  
}


/**  
 * @brief 压缩文件到ZIP文件  
 *  
 * @param zip_file 目标ZIP文件路径  
 * @param src_file 要压缩的源文件路径  
 * @param file_name_in_zip ZIP中的文件名，如果为NULL则使用src_file的文件名  
 * @return 成功返回UNZIP_OK，失败返回错误码  
 */  
int zip_file(const char *zip_file, const char *src_file, const char *file_name_in_zip)  
{  
    zipFile zf = NULL;  
    FILE *fin = NULL;  
    char buffer[UNZIP_BUFFER_SIZE];  
    int err = ZIP_OK;  
    int size_read = 0;  
    char *filename_inzip = NULL;  
    char *p = NULL;  
    struct stat file_stat;  
      
    /* 参数检查 */  
    if (!zip_file || !src_file) {  
        LogError("Invalid parameters");  
        return UNZIP_BADPARAM;  
    }  
      
    /* 检查源文件是否存在 */  
    if (stat(src_file, &file_stat) != 0) {  
        LogError("Source file %s does not exist", src_file);  
        return UNZIP_ERRNO;  
    }  
      
    /* 确定ZIP中的文件名 */  
    if (file_name_in_zip) {  
        filename_inzip = (char *)file_name_in_zip;  
    } else {  
        /* 提取文件名部分 */  
        filename_inzip = (char *)src_file;  
        for (p = (char *)src_file; *p; p++) {  
            if (*p == '/' || *p == '\\')  
                filename_inzip = p + 1;  
        }  
    }  
      
    /* 打开源文件 */  
    fin = fopen(src_file, "rb");  
    if (!fin) {  
        LogError("Cannot open source file %s", src_file);  
        return UNZIP_ERRNO;  
    }  
      
    /* 创建或打开ZIP文件 */  
    zf = zipOpen(zip_file, APPEND_STATUS_CREATE);  
    if (!zf) {  
        LogError("Cannot create or open zip file %s", zip_file);  
        fclose(fin);  
        return UNZIP_ERRNO;  
    }  
      
    /* 在ZIP中创建新文件 */  
    err = zipOpenNewFileInZip(zf, filename_inzip, NULL,  
                                 NULL, 0, NULL, 0, NULL,  
                                 Z_DEFLATED, Z_DEFAULT_COMPRESSION);  
    if (err != ZIP_OK) {  
        LogError("Error %d in zipOpenNewFileInZip", err);  
        zipClose(zf, NULL);  
        fclose(fin);  
        return UNZIP_INTERNAL;  
    }  
      
    /* 读取源文件并写入ZIP */  
    LogInfo("Compressing %s to %s", src_file, zip_file);  
    do {  
        size_read = fread(buffer, 1, sizeof(buffer), fin);  
        if (size_read < 0) {  
            LogError("Error reading from %s", src_file);  
            zipCloseFileInZip(zf);  
            zipClose(zf, NULL);  
            fclose(fin);  
            return UNZIP_ERRNO;  
        }  
          
        if (size_read > 0) {  
            err = zipWriteInFileInZip(zf, buffer, size_read);  
            if (err != ZIP_OK) {  
                LogError("Error %d in zipWriteInFileInZip", err);  
                zipCloseFileInZip(zf);  
                zipClose(zf, NULL);  
                fclose(fin);  
                return UNZIP_INTERNAL;  
            }  
        }  
    } while (size_read > 0);  
      
    /* 关闭ZIP中的文件 */  
    err = zipCloseFileInZip(zf);  
    if (err != ZIP_OK) {  
        LogError("Error %d in zipCloseFileInZip", err);  
        zipClose(zf, NULL);  
        fclose(fin);  
        return UNZIP_INTERNAL;  
    }  
      
    /* 关闭源文件和ZIP文件 */  
    fclose(fin);  
    err = zipClose(zf, NULL);  
    if (err != ZIP_OK) {  
        LogError("Error %d in zipClose", err);  
        return UNZIP_INTERNAL;  
    }  
      
    LogInfo("Successfully compressed %s", src_file);  
    return UNZIP_OK;  
}  
  
/**  
 * @brief 压缩内存数据到ZIP文件  
 *  
 * @param zip_file 目标ZIP文件路径  
 * @param buf 要压缩的数据缓冲区  
 * @param len 数据长度  
 * @param file_name_in_zip ZIP中的文件名  
 * @return 成功返回UNZIP_OK，失败返回错误码  
 */  
int zip_buffer(const char *zip_file, const char *buf, int len, const char *file_name_in_zip)  
{  
    zipFile zf = NULL;  
    int err = ZIP_OK;  
      
    /* 参数检查 */  
    if (!zip_file || !buf || len <= 0 || !file_name_in_zip) {  
        LogError("Invalid parameters");  
        return UNZIP_BADPARAM;  
    }  
      
    /* 创建或打开ZIP文件 */  
    zf = zipOpen(zip_file, APPEND_STATUS_CREATE);  
    if (!zf) {  
        LogError("Cannot create or open zip file %s", zip_file);  
        return UNZIP_ERRNO;  
    }  
      
    /* 在ZIP中创建新文件 */  
    err = zipOpenNewFileInZip(zf, file_name_in_zip, NULL,  
                                 NULL, 0, NULL, 0, NULL,  
                                 Z_DEFLATED, Z_DEFAULT_COMPRESSION);  
    if (err != ZIP_OK) {  
        LogError("Error %d in zipOpenNewFileInZip", err);  
        zipClose(zf, NULL);  
        return UNZIP_INTERNAL;  
    }  
      
    /* 写入数据到ZIP */  
    LogInfo("Compressing %d bytes to %s", len, zip_file);  
    err = zipWriteInFileInZip(zf, buf, len);  
    if (err != ZIP_OK) {  
        LogError("Error %d in zipWriteInFileInZip", err);  
        zipCloseFileInZip(zf);  
        zipClose(zf, NULL);  
        return UNZIP_INTERNAL;  
    }  
      
    /* 关闭ZIP中的文件 */  
    err = zipCloseFileInZip(zf);  
    if (err != ZIP_OK) {  
        LogError("Error %d in zipCloseFileInZip", err);  
        zipClose(zf, NULL);  
        return UNZIP_INTERNAL;  
    }  
      
    /* 关闭ZIP文件 */  
    err = zipClose(zf, NULL);  
    if (err != ZIP_OK) {  
        LogError("Error %d in zipClose", err);  
        return UNZIP_INTERNAL;  
    }  
      
    LogInfo("Successfully compressed %d bytes", len);  
    return UNZIP_OK;  
}  
  
/**  
 * @brief 添加文件或目录到ZIP文件  
 *  
 * @param zf 已打开的ZIP文件句柄  
 * @param path 要添加的文件或目录路径  
 * @param zip_path ZIP中的路径  
 * @return 成功返回UNZIP_OK，失败返回错误码  
 */  
static int add_to_zip(zipFile zf, const char *path, const char *zip_path)  
{  
    struct stat file_stat;  
    DIR *dir = NULL;  
    struct dirent *entry = NULL;  
    char full_path[512];  
    char full_zip_path[512];  
    FILE *fin = NULL;  
    char buffer[UNZIP_BUFFER_SIZE];  
    int err = ZIP_OK;  
    int size_read = 0;  
      
    /* 获取文件信息 */  
    if (stat(path, &file_stat) != 0) {  
        LogError("Cannot stat %s", path);  
        return UNZIP_ERRNO;  
    }  
      
    /* 处理目录 */  
    if (S_ISDIR(file_stat.st_mode)) {  
        /* 添加目录条目 */  
        if (zip_path && zip_path[0] != '\0') {  
            char dir_path[512];  
            snprintf(dir_path, sizeof(dir_path), "%s/", zip_path);  
              
            err = zipOpenNewFileInZip(zf, dir_path, NULL,  
                                         NULL, 0, NULL, 0, NULL,  
                                         Z_DEFLATED, Z_DEFAULT_COMPRESSION);  
            if (err != ZIP_OK) {  
                LogError("Error %d in zipOpenNewFileInZip for directory %s", err, dir_path);  
                return UNZIP_INTERNAL;  
            }  
              
            err = zipCloseFileInZip(zf);  
            if (err != ZIP_OK) {  
                LogError("Error %d in zipCloseFileInZip for directory %s", err, dir_path);  
                return UNZIP_INTERNAL;  
            }  
        }  
          
        /* 打开目录 */  
        dir = opendir(path);  
        if (!dir) {  
            LogError("Cannot open directory %s", path);  
            return UNZIP_ERRNO;  
        }  
          
        /* 遍历目录内容 */  
        while ((entry = readdir(dir)) != NULL) {  
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)  
                continue;  
                  
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);  
              
            if (zip_path && zip_path[0] != '\0')  
                snprintf(full_zip_path, sizeof(full_zip_path), "%s/%s", zip_path, entry->d_name);  
            else  
                snprintf(full_zip_path, sizeof(full_zip_path), "%s", entry->d_name);  
                  
            err = add_to_zip(zf, full_path, full_zip_path);  
            if (err != UNZIP_OK) {  
                closedir(dir);  
                return err;  
            }  
        }  
          
        closedir(dir);  
    } else if (S_ISREG(file_stat.st_mode)) {  
        /* 处理普通文件 */  
        fin = fopen(path, "rb");  
        if (!fin) {  
            LogError("Cannot open file %s", path);  
            return UNZIP_ERRNO;  
        }  

        /* 在ZIP中创建新文件 */  
        err = zipOpenNewFileInZip(zf, zip_path, NULL,  
                                     NULL, 0, NULL, 0, NULL,  
                                     Z_DEFLATED, Z_DEFAULT_COMPRESSION);	
        if (err != ZIP_OK) {  
            LogError("Error %d in zipOpenNewFileInZip for file %s", err, zip_path);  
            fclose(fin);  
            return UNZIP_INTERNAL;  
        }  
          
        /* 读取文件内容并写入ZIP */  
        LogInfo("Adding %s to zip", path);  
        do {  
            size_read = fread(buffer, 1, sizeof(buffer), fin);  
            if (size_read < 0) {  
                LogError("Error reading from %s", path);  
                zipCloseFileInZip(zf);  
                fclose(fin);  
                return UNZIP_ERRNO;  
            }  
              
            if (size_read > 0) {  
                err = zipWriteInFileInZip(zf, buffer, size_read);  
                if (err != ZIP_OK) {  
                    LogError("Error %d in zipWriteInFileInZip", err);  
                    zipCloseFileInZip(zf);  
                    fclose(fin);  
                    return UNZIP_INTERNAL;  
                }  
            }  
        } while (size_read > 0);  
          
        /* 关闭文件 */  
        fclose(fin);  
          
        /* 关闭ZIP中的文件 */  
        err = zipCloseFileInZip(zf);  
        if (err != ZIP_OK) {  
            LogError("Error %d in zipCloseFileInZip", err);  
            return UNZIP_INTERNAL;  
        }  
    }  
      
    return UNZIP_OK;  
}  
  
/**  
 * @brief 压缩目录到ZIP文件  
 *  
 * @param zip_file 目标ZIP文件路径  
 * @param src_dir 要压缩的源目录路径  
 * @param exclude_root 是否排除根目录名  
 * @return 成功返回UNZIP_OK，失败返回错误码  
 */  
int zip_directory(const char *zip_file, const char *src_dir, int exclude_root)  
{  
    zipFile zf = NULL;  
    int err = ZIP_OK;  
    char *root_name = NULL;  
    char *p = NULL;  
    struct stat dir_stat;  
      
    /* 参数检查 */  
    if (!zip_file || !src_dir) {  
        LogError("Invalid parameters");  
        return UNZIP_BADPARAM;  
    }  
      
    /* 检查源目录是否存在 */  
    if (stat(src_dir, &dir_stat) != 0 || !S_ISDIR(dir_stat.st_mode)) {  
        LogError("Source directory %s does not exist or is not a directory", src_dir);  
        return UNZIP_ERRNO;  
    }  
      
    /* 创建或打开ZIP文件 */  
    zf = zipOpen(zip_file, APPEND_STATUS_CREATE);  
    if (!zf) {  
        LogError("Cannot create or open zip file %s", zip_file);  
        return UNZIP_ERRNO;  
    }  
      
    /* 确定根目录名 */  
    if (exclude_root) {  
        /* 添加目录内容，不包括根目录名 */  
        err = add_to_zip(zf, src_dir, "");  
    } else {  
        /* 提取目录名部分 */  
        root_name = (char *)src_dir;  
        for (p = (char *)src_dir; *p; p++) {  
            if (*p == '/' || *p == '\\')  
                root_name = p + 1;  
        }  
          
        /* 如果路径以斜杠结尾，则使用上一级目录名 */  
        if (*root_name == '\0' && p > (char *)src_dir + 1) {  
            p -= 2;  
            while (p > (char *)src_dir && *p != '/' && *p != '\\')  
                p--;  
            if (p > (char *)src_dir)  
                root_name = p + 1;  
        }  
          
        /* 添加目录内容，包括根目录名 */  
        err = add_to_zip(zf, src_dir, root_name);  
    }  
      
    if (err != UNZIP_OK) {  
        zipClose(zf, NULL);  
        return err;  
    }  
      
    /* 关闭ZIP文件 */  
    err = zipClose(zf, NULL);  
    if (err != ZIP_OK) {  
        LogError("Error %d in zipClose", err);  
        return UNZIP_INTERNAL;  
    }  
      
    LogInfo("Successfully compressed directory %s", src_dir);  
    return UNZIP_OK;  
}									 	