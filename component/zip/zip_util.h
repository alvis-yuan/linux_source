/**
 * @file zip_util.h
 * @brief Zip compression/decompression utility
 * 
 * This component provides an object-oriented interface for working with ZIP files
 * using libzip library. It supports compression, decompression, and inspection
 * of ZIP archives.
 */

 #ifndef ZIP_UTIL_H
 #define ZIP_UTIL_H
 
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <zip.h>
 #include <zlib.h>
 
 /**
  * @brief Error codes returned by zip operations
  */
 enum zip_util_error {
     ZIP_UTIL_OK = 0,             /**< Operation completed successfully */
     ZIP_UTIL_ERR_OPEN,            /**< Failed to open zip archive */
     ZIP_UTIL_ERR_CREATE,          /**< Failed to create zip archive */
     ZIP_UTIL_ERR_ADD_FILE,        /**< Failed to add file to archive */
     ZIP_UTIL_ERR_EXTRACT_FILE,    /**< Failed to extract file from archive */
     ZIP_UTIL_ERR_NO_ENTRY,        /**< Requested entry not found in archive */
     ZIP_UTIL_ERR_READ,            /**< Error reading from archive */
     ZIP_UTIL_ERR_WRITE,           /**< Error writing to archive */
     ZIP_UTIL_ERR_MEMORY,          /**< Memory allocation error */
     ZIP_UTIL_ERR_INVALID_ARG,     /**< Invalid argument provided */
     ZIP_UTIL_ERR_UNKNOWN          /**< Unknown error occurred */
 };
 
 /**
  * @brief Compression method to use
  */
 enum zip_util_compression_method {
     ZIP_UTIL_COMP_STORE = 0,      /**< Store (no compression) */
     ZIP_UTIL_COMP_DEFLATE         /**< Deflate compression */
 };
 
 /**
  * @brief Opaque handle for zip archive
  */
 typedef struct zip_util_archive zip_util_archive_t;
 
 /**
  * @brief Information about a file in the archive
  */
 struct zip_util_file_info {
     const char *name;             /**< File name */
     size_t compressed_size;       /**< Compressed size in bytes */
     size_t uncompressed_size;     /**< Uncompressed size in bytes */
     time_t mtime;                 /**< Modification time */
     int is_dir;                   /**< Is this a directory? */
 };
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 /**
  * @brief Open an existing zip archive
  * 
  * @param path Path to the zip archive
  * @param err Pointer to store error code (optional)
  * @return zip_util_archive_t* Handle to the opened archive, or NULL on failure
  */
 zip_util_archive_t *zip_util_open(const char *path, enum zip_util_error *err);
 
 /**
  * @brief Create a new zip archive
  * 
  * @param path Path to the new zip archive
  * @param err Pointer to store error code (optional)
  * @return zip_util_archive_t* Handle to the created archive, or NULL on failure
  */
 zip_util_archive_t *zip_util_create(const char *path, enum zip_util_error *err);
 
 /**
  * @brief Close a zip archive and free resources
  * 
  * @param archive Archive handle to close
  */
 void zip_util_close(zip_util_archive_t *archive);
 
 /**
  * @brief Add a file to the zip archive
  * 
  * @param archive Archive handle
  * @param file_path Path to the file to add
  * @param archive_path Path within the archive (NULL for same as file_path)
  * @param method Compression method to use
  * @param err Pointer to store error code (optional)
  * @return int 0 on success, non-zero on failure
  */
 int zip_util_add_file(zip_util_archive_t *archive, const char *file_path,
                       const char *archive_path, 
                       enum zip_util_compression_method method,
                       enum zip_util_error *err);
 
 /**
  * @brief Extract a file from the zip archive
  * 
  * @param archive Archive handle
  * @param file_path Path within the archive
  * @param dest_path Destination path to extract to
  * @param err Pointer to store error code (optional)
  * @return int 0 on success, non-zero on failure
  */
 int zip_util_extract_file(zip_util_archive_t *archive, const char *file_path,
                           const char *dest_path, enum zip_util_error *err);
 
 /**
  * @brief Get information about all files in the archive
  * 
  * @param archive Archive handle
  * @param count Pointer to store number of files (output)
  * @param err Pointer to store error code (optional)
  * @return struct zip_util_file_info* Array of file info (must be freed by caller)
  */
 struct zip_util_file_info *zip_util_list_files(zip_util_archive_t *archive,
                                               size_t *count,
                                               enum zip_util_error *err);
 
 /**
  * @brief Free file info array returned by zip_util_list_files
  * 
  * @param info Array to free
  * @param count Number of elements in array
  */
 void zip_util_free_file_info(struct zip_util_file_info *info, size_t count);
 
 /**
  * @brief Get error message for an error code
  * 
  * @param err Error code
  * @return const char* Error message
  */
 const char *zip_util_error_message(enum zip_util_error err);
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif /* ZIP_UTIL_H */