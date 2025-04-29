/**
 * @file output_channel_file.c
 * @brief 文件输出通道实现
 */

 #include "channel.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <time.h>
 #include <sys/stat.h>
 #include <unistd.h>
 #include <libgen.h>
 #include <errno.h>
 #include <zlib.h>
 
 #define LOG_DIR "/data/logdir"
 #define CURRENT_LOG "current.log"

 static struct output_channel *file_ch = NULL;

 
 struct file_priv {
	 FILE *file;
	 time_t create_time;
 };
 
 static int ensure_log_dir(void)
 {
	 struct stat st;
	 
	 if (stat(LOG_DIR, &st) == 0) {
		 if (S_ISDIR(st.st_mode))
			 return 0;
		 return -1;
	 }
 
	 if (mkdir(LOG_DIR, 0755) < 0 && errno != EEXIST)
		 return -1;
 
	 return 0;
 }
 
 static int rotate_log(time_t create_time)
 {
	 char old_path[256];
	 char new_path[256];
	 FILE *src/*, *dst*/;
	 gzFile gz;
	 char buf[4096];
	 size_t len;
	 struct tm *tm_info;
 
	 tm_info = localtime(&create_time);
	 if (!tm_info)
		 return -1;
 
	 snprintf(old_path, sizeof(old_path), "%s/" CURRENT_LOG, LOG_DIR);
	 snprintf(new_path, sizeof(new_path), "%s/%04d%02d%02d_%02d%02d%02d.log.gz",
			  LOG_DIR,
			  tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
			  tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
 
	 src = fopen(old_path, "rb");
	 if (!src)
		 return -1;
 
	 gz = gzopen(new_path, "wb");
	 if (!gz) {
		 fclose(src);
		 return -1;
	 }
 
	 while ((len = fread(buf, 1, sizeof(buf), src)) > 0) {
		 if (gzwrite(gz, buf, (unsigned)len) != (int)len) {
			 gzclose(gz);
			 fclose(src);
			 unlink(new_path);
			 return -1;
		 }
	 }
 
	 gzclose(gz);
	 fclose(src);
 
	 if (unlink(old_path) < 0) {
		 unlink(new_path);
		 return -1;
	 }
 
	 return 0;
 }
 
 static int file_write(void *priv, const log_msg_t *msg)
 {
	 struct file_priv *f = priv;
	 time_t now;
	 struct tm *tm_info;
	 char time_buf[20];
 
	 now = time(NULL);
	 tm_info = localtime(&now);
	 strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
 
	 fprintf(f->file, "[%s] [%s] [PID:%d] %s\n",
			 time_buf, level_to_str(msg->level), msg->pid, msg->data);
	 fflush(f->file);
	 
	 return 0;
 }
 
 static int file_rotate(void *priv)
 {
	 struct file_priv *f = priv;
	 time_t now = time(NULL);
	 
	 if (now - f->create_time >= 3600) {
		 if (rotate_log(f->create_time) == 0) {
			 fclose(f->file);
			 f->file = fopen(LOG_DIR "/" CURRENT_LOG, "a");
			 if (!f->file)
				 return -1;
			 f->create_time = now;
		 }
	 }
	 
	 return 0;
 }
 
 static void file_destroy(void *priv)
 {
	 struct file_priv *f = priv;
	 if (f->file)
		 fclose(f->file);
	 free(f);
 }
 
 static const struct output_channel_ops file_ops = {
	 .write = file_write,
	 .rotate = file_rotate,
	 .destroy = file_destroy,
 };
 
 static struct output_channel *output_channel_file_create(void)
 {
	 struct file_priv *priv;
	 struct output_channel *channel;
	 char path[256];
 
	 if (ensure_log_dir() < 0)
		 return NULL;
 
	 snprintf(path, sizeof(path), "%s/" CURRENT_LOG, LOG_DIR);
 
	 priv = malloc(sizeof(*priv));
	 if (!priv)
		 return NULL;
 
	 priv->file = fopen(path, "a");
	 if (!priv->file) {
		 free(priv);
		 return NULL;
	 }
 
	 priv->create_time = time(NULL);
 
	 channel = malloc(sizeof(*channel));
	 if (!channel) {
		 fclose(priv->file);
		 free(priv);
		 return NULL;
	 }
 
	 channel->ops = &file_ops;
	 channel->priv = priv;
	 channel->next = NULL;
 
	 return channel;
 }

 static __attribute__((constructor)) void init(void)
 {
	 /* 创建输出通道 */
	 file_ch = output_channel_file_create();
	 if (file_ch) {
		 output_channel_add(file_ch);
	 } else {
		 fprintf(stderr, "Failed to create file output channel\n");
		 exit(EXIT_FAILURE);
	 }
 }

 static __attribute__((destructor)) void cleanup(void)
 {
	 if (file_ch) {
		 output_channel_remove(file_ch);
		 free(file_ch);
		 file_ch = NULL;
	 }
 }