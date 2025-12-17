#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <asm/types.h>	//for videodev2.h
#include <linux/videodev2.h>
#include <netdb.h>  
#include <malloc.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <net/if.h>
#include <sys/un.h>
#include <signal.h> 
#include <dirent.h>
#include <syslog.h>
#include <sys/prctl.h>
#include <stdarg.h>

#include <libcommon.h>

static int g_log_client = -1;

__attribute__((constructor)) void log_api_init(void)
{
	struct sockaddr_un address = {AF_UNIX,SOCK_LOG_SERVER};
	if( g_log_client>=0 ) return;
	g_log_client = socket(AF_UNIX, SOCK_DGRAM, 0);
	if( g_log_client>=0 ){
		if( connect(g_log_client,(struct sockaddr *)&address,sizeof(address))<0 ) {
			//fprintf(stderr,"%s() Connect to logserver(%s) error\n",__func__,SOCK_LOG_SERVER);
			close(g_log_client);
			g_log_client = -1;
		}
	}
}

int LogSendMsg(int pid,int level,const void *data,int datalen)
{
	if( g_log_client<0 ) log_api_init();
	if( g_log_client<0 ) {
		fprintf(stderr,"logserver NOT run!\n");
		return -1;
	}
	char buf[2048];
	log_msg_data_t *msg = (log_msg_data_t *)buf;
	msg->pid = pid;
	msg->level = level;
	int len = sizeof(buf)-sizeof(*msg);
	if( len>datalen ) len = datalen;
	if( len>0 ) memcpy(msg->data,data,len);
	return send(g_log_client,buf,len+sizeof(*msg),0);
}

void LogWrite(char loglevel,const char *file,int line,const char *func,const char *fmt,...)
{
	va_list arg;
	char buf[2048],*p;
	log_msg_data_t *msg = (log_msg_data_t *)buf;

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
	if( loglevel>=LOG_LEVEL_VERBOSE ) {
		fprintf(stderr,"%s\n",msg->data);
		return;
	}

	if( g_log_client>=0 && send(g_log_client,buf,pos-buf+1,0)>0 )
		return;
	else
		fprintf(stderr,"%s\n",msg->data);
}

void LogWithDataFile(char loglevel,const char *datafile,const char *file,int line,const char *func,const char *fmt,...)
{
	va_list arg;
	char buf[2048],*p;
	log_msg_with_datafile_t *fullmsg = (log_msg_with_datafile_t *)buf;

	fullmsg->pid = 0;
	fullmsg->level = -1;

	snprintf(fullmsg->datafile,sizeof(fullmsg->datafile),"%s",datafile);
	log_msg_data_t *msg = fullmsg->msg;
	p = strrchr(file,'/');
	p = p?p+1:(char *)file;
	msg->pid = getpid();
	msg->level = loglevel;

	char *pos = msg->data;
	pos += snprintf(pos,sizeof(buf)+buf-pos,"%s|%d|%s() ",p,line,func);
	va_start(arg, fmt);
	pos += vsnprintf(pos,sizeof(buf)+buf-pos,fmt,arg);
	va_end(arg);

	p = msg->data+strlen(msg->data)-1;
	while(p>=msg->data && *p=='\n') *p--=0;
	if( loglevel>=LOG_LEVEL_VERBOSE ) {
		fprintf(stderr,"%s\n",msg->data);
		return;
	}

	if( g_log_client>=0 && send(g_log_client,buf,pos-buf,0)>0 )
		return;
	else
		fprintf(stderr,"%s|%d|%s() %s\n",file,line,func,msg->data);
}

int isDebugMode()
{
	return !access(DBGMODE_FILENODE,F_OK);
}

void notifyLogServer()
{
	sem_t *s = sem_open(SEM_LOG_MNG,O_WRONLY);
	if( s ) {
		sem_post(s);
		sem_close(s);
	}
}

void remove_circle_logs()
{
	int count = 20;
	system("touch "REMOVE_CIRCLE_LOGS);
	notifyLogServer();

	while(count--) {
		usleep(100*1000);
		if( access(REMOVE_CIRCLE_LOGS,F_OK) ) break;
	}
}
