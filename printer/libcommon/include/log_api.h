#ifndef __LOG_API_H__
#define __LOG_API_H__

#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#define LOG_CONFIG_FILE 	"/data/SYS/log.ini"
#define SOCK_LOG_SERVER 	"/tmp/logserver"
#define SEM_LOG_MNG			"/log_mng.sem"
#define REMOVE_CIRCLE_LOGS  "/dev/remove_circle_logs"
/* #logdir #tree
logdir/
|-- abnormal
|-- cyclic
`-- oss
    `-- dat
*/
#define LOGDIR_DATA 		"/data/logdir"
#define LOGDIR_TEMP 		"/tmp/logdir"
#define LOG_OSS_DAT_DIR 	LOGDIR_TEMP"/files"
#define LOG_HTTP_INFO_DIR	LOGDIR_TEMP"/http"
#define OSSDIR_DATA 		LOGDIR_DATA"/oss"
#define OSSDIR_TEMP 		LOGDIR_TEMP"/oss"
#define ABNORMAL_DIR  		LOGDIR_DATA"/abnormal"

#define DBGMODE_FILENODE "/dev/debug_mode"

typedef struct {
	int  pid;
	char level;
	char data[];
} __attribute__((packed)) log_msg_data_t;

typedef struct {
	int  pid;
	char level;
	char datafile[64];
	log_msg_data_t msg[];
} __attribute__((packed)) log_msg_with_datafile_t;

enum {
	ID_GEN_ABNORMAL_LOG = -1,
	ID_GEN_DEBUG_LOG = -2,
	ID_RECONFIG_LOG = -3,
	ID_1MIN_TIMER = -4,
	ID_10MIN_TIMER = -5,
	ID_SPLIT_FILE = -6,
};

enum {
	LOG_LEVEL_FATAL=0,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_WARN,
	LOG_LEVEL_INFO,
	LOG_LEVEL_DEBUG,
	LOG_LEVEL_VERBOSE = 100,
};

void notifyLogServer();
void remove_circle_logs();

void log_api_init(void);
int isDebugMode(void);
int LogSendMsg(int pid,int level,const void *data,int datalen);
void LogWrite(char loglevel,const char *file,int line,const char *func,const char *fmt,...);
void LogWithDataFile(char lvl,const char *file,const char *fi,int ln,const char *fu,const char *fmt,...);
#define LogDbgWithFile(f,fmt,args...)   LogWithDataFile(LOG_LEVEL_DEBUG,f,__FILE__,__LINE__,__func__,fmt,##args)
#define _LogWrite(n,fmt,args...) LogWrite(n,__FILE__,__LINE__,__func__,fmt,##args)
#define LogFatal(fmt,args...) _LogWrite(LOG_LEVEL_FATAL,fmt,##args)
#define LogError(fmt,args...) _LogWrite(LOG_LEVEL_ERROR,fmt,##args)
#define LogWarn(fmt,args...)  _LogWrite(LOG_LEVEL_WARN,fmt,##args)
#define LogInfo(fmt,args...)  _LogWrite(LOG_LEVEL_INFO,fmt,##args)
#define LogDbg(fmt,args...)   _LogWrite(LOG_LEVEL_DEBUG,fmt,##args)
#define localDbg(fmt,args...) fprintf(stderr,"%s|%d|%s() "fmt"\n",__FILE__,__LINE__,__func__,##args)

#define _DEBUG_VERBOSE_ 0 //需要打印一些临时调试信息时打开此开关，调用LogVerbose打LOG
#if _DEBUG_VERBOSE_
#define LogVerbose(fmt,args...)   _LogWrite(LOG_LEVEL_VERBOSE,fmt,##args)
#else
#define LogVerbose(fmt,args...)
#endif

static inline void SendToLogserver(int pid,int level,const void *data,int dsize)
{
	char buf[2048];
	log_msg_data_t *msg = (log_msg_data_t *)buf;
	struct sockaddr_un addr = {AF_UNIX,SOCK_LOG_SERVER};
	int client = socket(AF_UNIX, SOCK_DGRAM, 0);
	if( client<0 ) {
		localDbg("socket error(%d),%s",errno,strerror(errno));
		return;
	}
	msg->pid = pid;
	msg->level = level;
	int len = sizeof(*msg);
	if( data && dsize>0 ) {
		if( dsize>sizeof(buf)-sizeof(*msg) ) dsize = sizeof(buf)-sizeof(*msg);
		memcpy(msg->data,data,dsize);
		len += dsize;
	}
	if( sendto(client,buf,len,0,(struct sockaddr *)&addr,sizeof(addr))<0 )
		localDbg("sendto logserver error(%d),%s",errno,strerror(errno));
	close(client);
}

#endif
