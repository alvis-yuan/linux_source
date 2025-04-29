/**
 * @file log_server.c
 * @brief 日志服务器实现
 */

 #include "channel.h"
 #include "filter.h"
 #include <stdlib.h>
 #include <unistd.h>
 
 
 int main(int argc, char *argv[])
 {
 
	 (void)argc;
	 (void)argv;
 
	 /* 主循环 */
	 LocalDbg("log server start");
	channel_main_loop();
	 
	 LocalDbg("log server exit");
	 destroy_channels();
 
	 return 0;
 }