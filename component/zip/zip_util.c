/**
 * @file zip_util.c
 * @brief Implementation of zip compression/decompression utility
 */

 #include "zip_util.h"
 #include <sys/stat.h>
 #include <time.h>
 #include <errno.h>
 
 /**
  * @brief Internal structure for zip archive
  */
 struct zip_util_archive {
     zip_t *za;                    /**< libzip archive handle */
     int is_open;                  /**< Is archive open? */
 };
 
 /**
  * @brief Convert libzip error to zip_util_error
  * 
  * @param ze libzip error code
  * @return enum zip_util_error Corresponding zip_util_error code
  */
 static enum zip_util_error zip_error_to_util_error(int ze)
 {
     switch (ze) {
     case ZIP_ER_OK:
         return ZIP_UTIL_OK;
     case ZIP_ER_MEMORY:
         return ZIP_UTIL_ERR_MEMORY;
     case ZIP_ER_NOENT:
         return ZIP_UTIL_ERR_NO_ENTRY;
     case ZIP_ER_READ:
         return ZIP_UTIL_ERR_READ;
     case ZIP_ER_WRITE:
         return ZIP_UTIL_ERR_WRITE;
     default:
         return ZIP_UTIL_ERR_UNKNOWN;
     }
 }
 
 zip_util_archive_t *zip_util_open(const char *path, enum zip_util_error *err)
 {
     int ze;
     zip_t *za;
     zip_util_archive_t *archive;
     
     if (!path) {
         if (err) *err = ZIP_UTIL_ERR_INVALID_ARG;
         return NULL;
     }
 
     za = zip_open(path, 0, &ze);
     if (!za) {
         if (err) *err = zip_error_to_util_error(ze);
         return NULL;
     }
 
     archive = malloc(sizeof(*archive));
     if (!archive) {
         zip_close(za);
         if (err) *err = ZIP_UTIL_ERR_MEMORY;
         return NULL;
     }
 
     archive->za = za;
     archive->is_open = 1;
 
     if (err) *err = ZIP_UTIL_OK;
     return archive;
 }
 
 zip_util_archive_t *zip_util_create(const char *path, enum zip_util_error *err)
 {
     zip_t *za;
     zip_util_archive_t *archive;
 
     if (!path) {
         if (err) *err = ZIP_UTIL_ERR_INVALID_ARG;
         return NULL;
     }
 
     za = zip_open(path, ZIP_CREATE | ZIP_TRUNCATE, NULL);
     if (!za) {
         if (err) *err = ZIP_UTIL_ERR_CREATE;
         return NULL;
     }
 
     archive = malloc(sizeof(*archive));
     if (!archive) {
         zip_close(za);
         if (err) *err = ZIP_UTIL_ERR_MEMORY;
         return NULL;
     }
 
     archive->za = za;
     archive->is_open = 1;
 
     if (err) *err = ZIP_UTIL_OK;
     return archive;
 }
 
 void zip_util_close(zip_util_archive_t *archive)
 {
     if (!archive) return;
     
     if (archive->is_open && archive->za) {
         zip_close(archive->za);
         archive->is_open = 0;
     }
     
     free(archive);
 }
 
 int zip_util_add_file(zip_util_archive_t *archive, const char *file_path,
                      const char *archive_path, 
                      enum zip_util_compression_method method,
                      enum zip_util_error *err)
 {
     zip_source_t *zs;
     struct stat st;
     zip_int64_t index;
     const char *name_in_archive;
     int compression_method;
 
     if (!archive || !file_path) {
         if (err) *err = ZIP_UTIL_ERR_INVALID_ARG;
         return -1;
     }
 
     if (stat(file_path, &st) != 0) {
         if (err) *err = ZIP_UTIL_ERR_NO_ENTRY;
         return -1;
     }
 
     name_in_archive = archive_path ? archive_path : file_path;
 
     zs = zip_source_file(archive->za, file_path, 0, 0);
     if (!zs) {
         if (err) *err = ZIP_UTIL_ERR_ADD_FILE;
         return -1;
     }
 
     compression_method = (method == ZIP_UTIL_COMP_DEFLATE) ? ZIP_CM_DEFLATE : ZIP_CM_STORE;
 
     index = zip_file_add(archive->za, name_in_archive, zs, ZIP_FL_ENC_UTF_8);
     if (index < 0) {
         zip_source_free(zs);
         if (err) *err = ZIP_UTIL_ERR_ADD_FILE;
         return -1;
     }
 
     if (zip_set_file_compression(archive->za, index, compression_method, 0) != 0) {
         if (err) *err = ZIP_UTIL_ERR_ADD_FILE;
         return -1;
     }
 
     if (err) *err = ZIP_UTIL_OK;
     return 0;
 }
 
 int zip_util_extract_file(zip_util_archive_t *archive, const char *file_path,
                          const char *dest_path, enum zip_util_error *err)
 {
     zip_stat_t sb;
     zip_file_t *zf;
     FILE *out;
     char buf[4096];
     zip_int64_t n;
     zip_int64_t index;
 
     if (!archive || !file_path || !dest_path) {
         if (err) *err = ZIP_UTIL_ERR_INVALID_ARG;
         return -1;
     }
 
     index = zip_name_locate(archive->za, file_path, 0);
     if (index < 0) {
         if (err) *err = ZIP_UTIL_ERR_NO_ENTRY;
         return -1;
     }
 
     if (zip_stat_index(archive->za, index, 0, &sb) != 0) {
         if (err) *err = ZIP_UTIL_ERR_NO_ENTRY;
         return -1;
     }
 
     zf = zip_fopen_index(archive->za, index, 0);
     if (!zf) {
         if (err) *err = ZIP_UTIL_ERR_EXTRACT_FILE;
         return -1;
     }
 
     out = fopen(dest_path, "wb");
     if (!out) {
         zip_fclose(zf);
         if (err) *err = ZIP_UTIL_ERR_EXTRACT_FILE;
         return -1;
     }
 
     while ((n = zip_fread(zf, buf, sizeof(buf))) > 0) {
         if (fwrite(buf, 1, n, out) != (size_t)n) {
             fclose(out);
             zip_fclose(zf);
             if (err) *err = ZIP_UTIL_ERR_EXTRACT_FILE;
             return -1;
         }
     }
 
     fclose(out);
     zip_fclose(zf);
 
     if (n < 0) {
         if (err) *err = ZIP_UTIL_ERR_EXTRACT_FILE;
         return -1;
     }
 
     if (err) *err = ZIP_UTIL_OK;
     return 0;
 }
 
 struct zip_util_file_info *zip_util_list_files(zip_util_archive_t *archive,
                                              size_t *count,
                                              enum zip_util_error *err)
 {
     zip_int64_t num_entries;
     struct zip_util_file_info *files;
     zip_int64_t i;
 
     if (!archive || !count) {
         if (err) *err = ZIP_UTIL_ERR_INVALID_ARG;
         return NULL;
     }
 
     num_entries = zip_get_num_entries(archive->za, 0);
     if (num_entries < 0) {
         if (err) *err = ZIP_UTIL_ERR_READ;
         return NULL;
     }
 
     files = malloc(num_entries * sizeof(*files));
     if (!files) {
         if (err) *err = ZIP_UTIL_ERR_MEMORY;
         return NULL;
     }
 
     for (i = 0; i < num_entries; i++) {
         const char *name = zip_get_name(archive->za, i, 0);
         zip_stat_t sb;
 
         if (!name) {
             free(files);
             if (err) *err = ZIP_UTIL_ERR_READ;
             return NULL;
         }
 
         if (zip_stat_index(archive->za, i, 0, &sb) != 0) {
             free(files);
             if (err) *err = ZIP_UTIL_ERR_READ;
             return NULL;
         }
 
         files[i].name = strdup(name);
         if (!files[i].name) {
             while (i-- > 0) {
                 free((void *)files[i].name);
             }
             free(files);
             if (err) *err = ZIP_UTIL_ERR_MEMORY;
             return NULL;
         }
 
         files[i].compressed_size = sb.comp_size;
         files[i].uncompressed_size = sb.size;
         files[i].mtime = sb.mtime;
         files[i].is_dir = (name[strlen(name)-1] == '/');
     }
 
     *count = num_entries;
     if (err) *err = ZIP_UTIL_OK;
     return files;
 }
 
 void zip_util_free_file_info(struct zip_util_file_info *info, size_t count)
 {
     size_t i;
 
     if (!info) return;
 
     for (i = 0; i < count; i++) {
         free((void *)info[i].name);
     }
 
     free(info);
 }
 
 const char *zip_util_error_message(enum zip_util_error err)
 {
     static const char *messages[] = {
         "Success",
         "Failed to open zip archive",
         "Failed to create zip archive",
         "Failed to add file to archive",
         "Failed to extract file from archive",
         "Requested entry not found in archive",
         "Error reading from archive",
         "Error writing to archive",
         "Memory allocation error",
         "Invalid argument provided",
         "Unknown error occurred"
     };
 
     if (err < 0 || err >= (int)(sizeof(messages)/sizeof(messages[0]))) {
         return "Invalid error code";
     }
 
     return messages[err];
 }