#include <libcommon.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <mqueue.h>
#include <hwinfo.h>

int FileInstaller(const char *filename)
{
	sem_t *sem = NULL;
	char buf[TM_MQ_MAX_SIZE] = {};
	tm_msg_t *msg = (tm_msg_t *)buf;
	msg->cmd = TM_CMD_INSTALL;
	msg->len = snprintf(msg->data,sizeof(buf)-sizeof(*msg),"%s",filename)+1;
	mqd_t mq = mq_open(MQ_TM_SERVICE,O_WRONLY|O_NONBLOCK);
	if( mq<0 ) {
		LogError("TM service is down!");
		return -1;
	}
	int len = msg->len+sizeof(*msg);
	unlink(UPDATE_STAT_FILE);
	sem_unlink(TM_SEM_FILE);
	char *pStat = NULL;
	sem = sem_open(TM_SEM_FILE,O_RDWR|O_CREAT,0644,0);
	int ret = mq_send(mq,buf,len,0);
	mq_close(mq);
	if( ret<0 ) {
		LogError("TM service maybe down!");
		goto out;
	}
	if( !sem ) {
		ret = -1;
		goto out;
	}
	sem_wait(sem);
	pStat = (char *)util_file2buf(UPDATE_STAT_FILE,0);
	if( pStat ) {
		ret = atoi(pStat);
		free(pStat);
	} else {
		LogError("Can not read status file %s",UPDATE_STAT_FILE);
		ret = -1;
	}
out:
	unlink(UPDATE_STAT_FILE);
	if( sem ) sem_close(sem);
	sem_unlink(TM_SEM_FILE);
	return ret;
}

void FileInstall_Complete()
{
	sem_t *sem = sem_open(TM_SEM_FILE,O_WRONLY);
	if( sem ) {
		sem_post(sem);
		sem_close(sem);
	}
}

int sys_hwinfo_set(int type,const void *data,int len)
{
	if( type<HWINFO_BLOCK_SN ) return -1;
	if( type>=HWINFO_BLOCK_LARGE_PEM_START && type<=HWINFO_BLOCK_LARGE_CFG_END ) return -1;
	if( type>=HWINFO_BLOCK_CRC || type==HWINFO_BLOCK_UPDATE_COUNT) return -1;
	if( len>HWINFO_CONTENT_LENS ) {
		LogWarn("content len(%d) too large",len);
		return -1;
	}
	char buf[128] = {};
	tm_msg_t *msg = (tm_msg_t *)buf;
	tm_mq_sethwinfo_data_t *info_d = (tm_mq_sethwinfo_data_t *)msg->data;
	info_d->lens = len;
	info_d->type = type;
	if( len>0 )
		memcpy(info_d->content,data,len);
	msg->len = len + sizeof(*info_d);
	msg->cmd = TM_CMD_SET_HWINFO;
	mqd_t mq = mq_open(MQ_TM_SERVICE,O_WRONLY|O_NONBLOCK);
	if( mq<0 ) {
		LogError("TM service is down!");
		return -1;
	}
	int ret = mq_send(mq,buf,msg->len+sizeof(*msg),0);
	mq_close(mq);
	if( ret<0 ) {
		LogError("TM service maybe down!");
		return ret;
	}
	util_msleep(30);
	return 0;
}
