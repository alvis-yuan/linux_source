/**
 * @file stream_extract.c
 * @brief Test program for streaming extraction of large files from zip archives
 */

 #include <zip.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <errno.h>
 #include <sys/stat.h>
 
 #define BUFFER_SIZE 8192
 
 /**
  * @brief Print usage information
  */
 static void print_usage(const char *progname)
 {
     fprintf(stderr, "Usage: %s <zipfile> <file_to_extract> [output_file]\n", progname);
     fprintf(stderr, "Stream-extracts a large file from a zip archive\n");
     fprintf(stderr, "If output_file is not specified, uses the original filename\n");
 }
 
 /**
  * @brief Create directory hierarchy for a path
  */
 static int create_path(const char *path)
 {
     char *path_copy = strdup(path);
     char *dir = path_copy;
     char *slash;
     int ret = 0;
     struct stat st;
 
     if (!path_copy) {
         return -1;
     }
 
     while ((slash = strchr(dir + 1, '/'))) {
         *slash = '\0';
         if (stat(path_copy, &st) != 0) {
             if (mkdir(path_copy, 0755)) {
                 ret = -1;
                 break;
             }
         }
         *slash = '/';
         dir = slash;
     }
 
     free(path_copy);
     return ret;
 }
 
 int main(int argc, char **argv)
 {
     zip_t *za = NULL;
     zip_file_t *zf = NULL;
     FILE *out = NULL;
     const char *output_path = NULL;
     char buffer[BUFFER_SIZE];
     zip_int64_t n;
     int err;
     struct stat st;
 
     if (argc < 3 || argc > 4) {
         print_usage(argv[0]);
         return 1;
     }
 
     /* Open zip archive */
     if ((za = zip_open(argv[1], 0, &err)) == NULL) {
         zip_error_t error;
         zip_error_init_with_code(&error, err);
         fprintf(stderr, "Error opening archive '%s': %s\n",
                 argv[1], zip_error_strerror(&error));
         zip_error_fini(&error);
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
 
     /* Create directory hierarchy if needed */
     char *path_copy = strdup(output_path);
     char *last_slash = strrchr(path_copy, '/');
     if (last_slash) {
         *last_slash = '\0';
         if (create_path(path_copy)) {
             fprintf(stderr, "Error creating directory hierarchy for '%s': %s\n",
                     output_path, strerror(errno));
             free(path_copy);
             zip_close(za);
             return 1;
         }
     }
     free(path_copy);
 
     printf("Stream-extracting '%s' from '%s' to '%s'...\n", 
            argv[2], argv[1], output_path);
 
     /* Open file in zip archive */
     if ((zf = zip_fopen(za, argv[2], 0)) == NULL) {
         fprintf(stderr, "Error opening '%s' in archive: %s\n",
                 argv[2], zip_strerror(za));
         zip_close(za);
         return 1;
     }
 
     /* Open output file */
     if ((out = fopen(output_path, "wb")) == NULL) {
         fprintf(stderr, "Error opening output file '%s': %s\n",
                 output_path, strerror(errno));
         zip_fclose(zf);
         zip_close(za);
         return 1;
     }
 
     /* Stream data from zip to output file */
     while ((n = zip_fread(zf, buffer, sizeof(buffer))) > 0) {
         if (fwrite(buffer, 1, n, out) != (size_t)n) {
             fprintf(stderr, "Error writing to output file: %s\n",
                     strerror(errno));
             fclose(out);
             zip_fclose(zf);
             zip_close(za);
             return 1;
         }
         printf(".");  /* Progress indicator */
         fflush(stdout);
     }
 
     printf("\n");
 
     if (n < 0) {
         fprintf(stderr, "Error reading from zip file: %s\n",
                 zip_file_strerror(zf));
         fclose(out);
         zip_fclose(zf);
         zip_close(za);
         return 1;
     }
 
     /* Clean up */
     fclose(out);
     zip_fclose(zf);
     zip_close(za);
 
     /* Verify extracted file size */
     if (stat(output_path, &st) == 0) {
         printf("Successfully extracted '%s' (%ld bytes)\n",
                output_path, (long)st.st_size);
     } else {
         printf("Successfully extracted '%s'\n", output_path);
     }
 
     return 0;
 }