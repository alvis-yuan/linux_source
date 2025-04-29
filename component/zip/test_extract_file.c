/**
 * @file extract_file.c
 * @brief Test program to extract a specific file from a zip archive
 */

 #include "zip_util.h"
 #include <stdio.h>
 #include <string.h>
 
 /**
  * @brief Print usage information
  */
 static void print_usage(const char *progname)
 {
     fprintf(stderr, "Usage: %s <zipfile> <file_to_extract> [output_file]\n", progname);
     fprintf(stderr, "Extracts a file from a zip archive\n");
     fprintf(stderr, "If output_file is not specified, uses the original filename\n");
 }
 
 int main(int argc, char **argv)
 {
     zip_util_archive_t *archive;
     enum zip_util_error err;
     const char *output_path;
     int ret;
 
     if (argc < 3 || argc > 4) {
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
 
     /* Determine output path */
     if (argc == 4) {
         output_path = argv[3];
     } else {
         /* Extract to current directory with original filename */
         const char *last_slash = strrchr(argv[2], '/');
         if (last_slash) {
             output_path = last_slash + 1;
         } else {
             output_path = argv[2];
         }
     }
 
     printf("Extracting '%s' from '%s' to '%s'...\n", 
            argv[2], argv[1], output_path);
 
     /* Extract the file */
     ret = zip_util_extract_file(archive, argv[2], output_path, &err);
     if (ret != 0) {
         fprintf(stderr, "Error extracting file: %s\n",
                 zip_util_error_message(err));
         zip_util_close(archive);
         return 1;
     }
 
     printf("Successfully extracted '%s'\n", output_path);
 
     zip_util_close(archive);
     return 0;
 }