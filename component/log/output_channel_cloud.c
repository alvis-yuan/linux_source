/**
 * @file output_channel_cloud.c
 * @brief 云端输出通道实现
 */

 #include "channel.h"
 #include <stdlib.h>
 #include <string.h>
 #include <curl/curl.h>
 
 struct cloud_priv {
	 CURL *curl;
	 char *url;
 };
 
 static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
 {
	 (void)ptr;
	 (void)userdata;
	 return size * nmemb;
 }
 
 static int cloud_write(void *priv, const log_msg_t *msg)
 {
	 struct cloud_priv *c = priv;
	 CURLcode res;
	 char *escaped;
	 char post_data[4096];
	 
	 if (!c->curl)
		 return -1;
 
	 escaped = curl_easy_escape(c->curl, msg->data, 0);
	 if (!escaped)
		 return -1;
 
	 snprintf(post_data, sizeof(post_data),
			  "pid=%d&level=%d&message=%s", msg->pid, msg->level, escaped);
	 
	 curl_easy_setopt(c->curl, CURLOPT_POSTFIELDS, post_data);
	 
	 res = curl_easy_perform(c->curl);
	 curl_free(escaped);
	 
	 return (res == CURLE_OK) ? 0 : -1;
 }
 
 static void cloud_destroy(void *priv)
 {
	 struct cloud_priv *c = priv;
	 if (c->curl)
		 curl_easy_cleanup(c->curl);
	 free(c->url);
	 free(c);
 }
 
 static const struct output_channel_ops cloud_ops = {
	 .write = cloud_write,
	 .destroy = cloud_destroy,
 };
 
 struct output_channel *output_channel_cloud_create(const char *url)
 {
	 struct cloud_priv *priv;
	 struct output_channel *channel;
 
	 priv = malloc(sizeof(*priv));
	 if (!priv)
		 return NULL;
 
	 priv->curl = curl_easy_init();
	 if (!priv->curl) {
		 free(priv);
		 return NULL;
	 }
 
	 priv->url = strdup(url);
	 if (!priv->url) {
		 curl_easy_cleanup(priv->curl);
		 free(priv);
		 return NULL;
	 }
 
	 curl_easy_setopt(priv->curl, CURLOPT_URL, url);
	 curl_easy_setopt(priv->curl, CURLOPT_WRITEFUNCTION, write_callback);
	 curl_easy_setopt(priv->curl, CURLOPT_TIMEOUT, 5L);
 
	 channel = malloc(sizeof(*channel));
	 if (!channel) {
		 curl_easy_cleanup(priv->curl);
		 free(priv->url);
		 free(priv);
		 return NULL;
	 }
 
	 channel->ops = &cloud_ops;
	 channel->priv = priv;
	 channel->next = NULL;
 
	 return channel;
 }