#ifndef __TM_H__
#define __TM_H__

#define TM_MQ_MAX_SIZE    128
#define MQ_TM_SERVICE     "/mq_tm_service"
#define UPDATE_STAT_FILE  "/dev/update_stat"
#define TM_SEM_FILE       "/sem_install_stat"
#define INSTALL_PROCEDURE "/data/SYS/install.procedure"

#define SetUpdateStatus(st) UtilRunCmd("echo -n %d > %s",st,UPDATE_STAT_FILE)

#define TM_CMD_INSTALL	1
#define TM_CMD_HWINFO_DUMP	2
#define TM_CMD_SET_HWINFO 3
typedef struct {
    short cmd;
    short len;
    char data[];
} tm_msg_t;

typedef struct {
	short type;
	short lens;
	char content[];
} tm_mq_sethwinfo_data_t;


int FileInstaller(const char *filename);
int sys_hwinfo_set(int type,const void *data,int len);
void FileInstall_Complete();

#endif
