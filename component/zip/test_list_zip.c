/**
 * @file list_zip.c
 * @brief Test program to list contents of a zip file
 */

 #include "zip_util.h"
 #include <stdio.h>
 #include <time.h>
 #include <string.h>
 
 /**
  * @brief Print usage information
  */
 static void print_usage(const char *progname)
 {
     fprintf(stderr, "Usage: %s <zipfile>\n", progname);
     fprintf(stderr, "Lists contents of a zip archive\n");
 }
 
 /**
  * @brief Format time for display
  * 
  * @param t Time to format
  * @param buf Buffer to store result (must be at least 20 bytes)
  */
 static void format_time(time_t t, char *buf)
 {
     struct tm *tm;
 
     if (t == 0) {
         strcpy(buf, "unknown");
         return;
     }
 
     tm = localtime(&t);
     if (!tm) {
         strcpy(buf, "invalid");
         return;
     }
 
     strftime(buf, 20, "%Y-%m-%d %H:%M:%S", tm);
 }
 
 int main(int argc, char **argv)
 {
     zip_util_archive_t *archive;
     enum zip_util_error err;
     struct zip_util_file_info *files;
     size_t count;
     size_t i;
     char time_buf[20];
     size_t total_compressed = 0;
     size_t total_uncompressed = 0;
     int num_files = 0;
     int num_dirs = 0;
 
     if (argc != 2) {
         print_usage(argv[0]);
         return 1;
     }
 
     /* Open the zip archive */
     archive = zip_util_open(argv[1], &err);
     if (!archive) {
         fprintf(stderr, "Error opening archive '%s': %s\n",
                 argv[1], zip_util_error_message(err));
         return 1;
     }
 
     /* Get list of files */
     files = zip_util_list_files(archive, &count, &err);
     if (!files) {
         fprintf(stderr, "Error listing archive contents: %s\n",
                 zip_util_error_message(err));
         zip_util_close(archive);
         return 1;
     }
 
     /* Print header */
     printf("Archive: %s\n", argv[1]);
     printf("------------------------------------------------\n");
     printf("  Length     Size    Date                Name\n");
     printf("---------  --------  -------------------  --------\n");
 
     /* Print each file's information */
     for (i = 0; i < count; i++) {
         format_time(files[i].mtime, time_buf);
 
         if (files[i].is_dir) {
             printf("%9s  %8s  %19s  %s\n",
                    "-", "-", time_buf, files[i].name);
             num_dirs++;
         } else {
             printf("%9zu  %8zu  %19s  %s\n",
                    files[i].uncompressed_size,
                    files[i].compressed_size,
                    time_buf, files[i].name);
             total_compressed += files[i].compressed_size;
             total_uncompressed += files[i].uncompressed_size;
             num_files++;
         }
     }
 
     /* Print footer with totals */
     printf("---------  --------  -------------------  --------\n");
     printf("%9zu  %8zu                    %d %s, %d %s\n",
            total_uncompressed, total_compressed,
            num_files, (num_files == 1) ? "file" : "files",
            num_dirs, (num_dirs == 1) ? "directory" : "directories");
 
     /* Clean up */
     zip_util_free_file_info(files, count);
     zip_util_close(archive);
 
     return 0;
 }