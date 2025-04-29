/**
 * @file log_api.c
 * @brief 日志客户端API实现
 */

 #include "log_api.h"
 #include <sys/socket.h>
 #include <sys/un.h>
 #include <unistd.h>
 #include <stdlib.h>
 #include <stdarg.h>
 #include <string.h>
 #include <errno.h>
 #include <stdarg.h>
 #include <stdio.h>
 
 static int sock_fd = -1;
 
 static __attribute__((constructor)) int log_init(void)
 {
	 struct sockaddr_un addr;
	 
	 sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	 if (sock_fd < 0) {
		 fprintf(stderr, "%s() socket error(%d:%s)\n", __func__, errno, strerror(errno));
		 return -1;
	}
	 
	 memset(&addr, 0, sizeof(addr));
	 addr.sun_family = AF_UNIX;
	 strncpy(addr.sun_path, "/tmp/log_server.sock", sizeof(addr.sun_path) - 1);
	 
	 if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		 fprintf(stderr, "%s() connect error(%d:%s)\n", __func__, errno, strerror(errno));
		 close(sock_fd);
		 sock_fd = -1;
		 return -1;
	 }
	 
	 return 0;
 }
 
void log_write(char loglevel,const char *file,int line,const char *func,const char *fmt,...)
 {
	va_list arg;
	char buf[2048],*p;
	log_msg_t *msg = (log_msg_t *)buf;
	int ret;

	p = strrchr(file,'/');
	p = p?p+1:(char *)file;
	msg->pid = getpid();
	msg->level = loglevel;

	char *pos = msg->data;
	pos += snprintf(pos,sizeof(buf)+buf-pos,"%s|%d|%s() ",p,line,func);
	va_start(arg, fmt);
	if( pos-buf<sizeof(buf) )
		pos += vsnprintf(pos,sizeof(buf)+buf-pos,fmt,arg);
	va_end(arg);

	if( pos-buf>sizeof(buf) )
		pos = buf+sizeof(buf)-1;

	while(pos>=msg->data && *pos=='\n') *pos--=0;

	if( sock_fd>=0) {
		ret = send(sock_fd,buf,pos-buf+1,0);
		if (ret<0) {
			fprintf(stderr,"%s() send error(%d:%s)\n",__func__,errno,strerror(errno));
			close(sock_fd);
			sock_fd = -1;
		}
		return;
	}
	else {
		fprintf(stderr,"logserver NOT run!\n");
		if( sock_fd>=0 ) close(sock_fd);
		sock_fd = -1;
		LocalDbg("logserver NOT run!");
		return;
	}
 }
 
 static __attribute__((destructor))void log_cleanup(void)
 {
	 if (sock_fd >= 0) {
		 close(sock_fd);
		 sock_fd = -1;
	 }
 }