#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>

#include <libcommon.h>

static bt_shm_t *shm = NULL;
static sem_t *ble_rxbuf_sem;
static sem_t *ble_txbuf_sem;
static sem_t *spp_rxbuf_sem;
static sem_t *spp_txbuf_sem;

#define BT_WEB_OPERATING "/data/bluetooth/bt_web_is_operating"

#define BLE_RX_POOL   (shm->ble_session.rxPool)
#define BLE_TX_POOL   (shm->ble_session.txPool)
#define SPP_RX_POOL   (shm->spp_session.rxPool)
#define SPP_TX_POOL   (shm->spp_session.txPool)

#define CLEAR_BUF(prot,P,f)   \
do{ \
	if(f & (1<<1)){\
		sem_wait(prot##_rxbuf_sem);\
		ringbuffer_t * pRxBuffer = &(shm->prot##_session.rxBuff);\
		clear_ringBuffer(pRxBuffer,P##_RX_POOL,BT_##P##_RXBUF_MAX_LEN);\
		sem_post(prot##_rxbuf_sem);\
	}\
	if(f & (1<<0)){\
		sem_wait(prot##_txbuf_sem);\
		ringbuffer_t * pTxBuffer = &(shm->prot##_session.txBuff);\
		clear_ringBuffer(pTxBuffer,P##_TX_POOL,BT_##P##_TXBUF_MAX_LEN);\
		sem_post(prot##_txbuf_sem);\
	}\
}while(0);


#define SEND_DATA(prot,p,v)   \
do { \
	int rlen,buff_len,free_len,dlen=0;\
	if(sem_trywait(prot##_txbuf_sem)!=0)\
	 {\
	 	return  0;\
	 }\
	 ringbuffer_t * pTxBuffer = &(shm->prot##_session.txBuff);\
	rlen=  get_ringBuffer_btoRead(pTxBuffer);\
	buff_len = get_ringBuffer_length(pTxBuffer);\
	free_len = buff_len - rlen;\
	if(free_len > 0)  \
	{\
		dlen = free_len < tlen ? free_len:tlen;\
		write_ringBuff(v,dlen,pTxBuffer,p);\
		sem_post(prot##_txbuf_sem);\
		return dlen;\
	}\
	else\
	{\
		LogError("Error,Error:  no space to write data to txBuff free_len=%d,rlen=%d",free_len,rlen);\
		sem_post(prot##_txbuf_sem);\
		return dlen;\
	}\
} while (0);


#define RECV_DATA(prot,p,v,len)   \
do{ \
	int new_data=0,dlen=0;\
	 if(sem_trywait(prot##_rxbuf_sem)!=0)\
	 {\
	 	return  0;\
	 }\
	ringbuffer_t * pRxBuffer = &(shm->prot##_session.rxBuff);\
	new_data = get_ringBuffer_btoRead(pRxBuffer);\
	if(new_data >0)\
	{\
		dlen = ((len<new_data) ? len:new_data);\
		read_ringBuff(v,dlen,pRxBuffer,p);\
		sem_post(prot##_rxbuf_sem);\
		return dlen;\
	}\
	else\
	{\
		sem_post(prot##_rxbuf_sem);\
		return 0;\
	}\
}while(0)

#define  MODULE_SHM_CHECK()  \
do{\
	if(shm ==NULL)\
		return -1;\
}while(0)

static unsigned int write_ringBuff(unsigned char *buff, unsigned int size,ringbuffer_t *pRingBuffer,unsigned char *pool)
{
	unsigned int len =0;
	unsigned int ringBuf_bw = pRingBuffer->bw;
	unsigned int ringBuf_len = pRingBuffer->length;
	//unsigned char *ringBuf_src = pRingBuffer->source;
	unsigned char *ringBuf_src = pool;

	if((ringBuf_bw +size) <= ringBuf_len)
	{
		memcpy(ringBuf_src + ringBuf_bw, buff, size);
	}
	else
	{
		len= ringBuf_len- ringBuf_bw;
		memcpy(ringBuf_src + ringBuf_bw, buff, len);
		memcpy(ringBuf_src , buff+len, size -len);
	}

	pRingBuffer->bw = (pRingBuffer->bw +size)%ringBuf_len;
	pRingBuffer->btoRead += size;
	return size;
}

static unsigned int read_ringBuff(unsigned char *buff, unsigned int size,ringbuffer_t *pRingBuffer,unsigned char *pool)
{
	unsigned int len =0;
	unsigned int ringBuf_br = pRingBuffer->br;
	unsigned int ringBuf_len = pRingBuffer->length;
	unsigned char *ringBuf_src = pool;

	if((ringBuf_br +size) <= ringBuf_len)
	{
		memcpy( buff,ringBuf_src+ringBuf_br, size);
	}
	else
	{
		len= ringBuf_len- ringBuf_br;
		memcpy( buff,ringBuf_src + ringBuf_br, len);
		memcpy( buff+len,ringBuf_src , size -len);
	}

	pRingBuffer->br= (pRingBuffer->br +size)%ringBuf_len;
	pRingBuffer->btoRead -= size;
	return size;
}


static  unsigned int get_ringBuffer_btoRead(ringbuffer_t *pRingBuffer)
{
	return  pRingBuffer->btoRead;
}

static  unsigned int get_ringBuffer_length(ringbuffer_t *pRingBuffer)
{
	return  pRingBuffer->length;
}

static int clear_ringBuffer(ringbuffer_t *pRingBuffer,unsigned char *pBuf,unsigned int buf_len)
{
	pRingBuffer->br =0;
	pRingBuffer->bw =0;
	pRingBuffer->btoRead =0;
	//
	//memset(pRingBuffer->source,0,pRingBuffer->length);
	memset(pBuf,0,buf_len);
	return  0;
}

static int do_send_data( bt_Prot prot,unsigned int  conn_id,unsigned char  *value,unsigned int  tlen)
{
	if(prot == PRO_BLE)
	{
		if((conn_id == shm->ble_session.conn_id) &&(NORMAL_MODE==shm->ble_session.mode))
		{
			SEND_DATA(ble,BLE_TX_POOL,value);
		}
	}
	else if(prot == PRO_SPP)
	{
		SEND_DATA(spp,SPP_TX_POOL,value);
	}
	return -1;
}

static int do_read_data(bt_Prot prot,unsigned int  conn_id,unsigned char *data,unsigned int len)
{
	if(prot == PRO_BLE)
	{
		if((conn_id == shm->ble_session.conn_id) &&(NORMAL_MODE==shm->ble_session.mode))
		{
			RECV_DATA(ble,BLE_RX_POOL,data,len);
		}
	}
	else if(prot == PRO_SPP)
	{
		if(conn_id == shm->spp_session.conn_id)
		{
			RECV_DATA(spp,SPP_RX_POOL,data,len);
		}
	}
	return -1;
}

static void mq_send_msg(short cmd, bt_Prot prot, void *data,int dlen)
{
	int len=0;
	char buf[BT_MQ_MAX_SIZE+1] = {};
	bt_msg_t *msg = (bt_msg_t *)buf;
	msg->prot= prot;
	msg->cmd = cmd;
	
	len = sizeof(*msg);
	if(dlen > sizeof(buf)-len)
	{
		LogError("data is large than then mq buf!!!");
		return;
	}
	if(data!=NULL)
	{
		memcpy(msg->data,(char*)data,dlen);
		len+=dlen;
	}
	
	mqd_t mq = mq_open(MQ_BT_SERVICE,O_WRONLY|O_NONBLOCK);
	if( mq<0 ) {
		LogError("BtServer may not run!!!");
		return;
	}
	if( mq_send(mq,buf,len,0)<0 )
		LogError("mq_send fail(%d),%s",errno,strerror(errno));
	mq_close(mq);
}

void BtPwrOn(void)
{
	mq_send_msg(BT_CMD_OPEN,PRO_BLE,0,0);
}

void BtPwrOff(void)
{
	mq_send_msg(BT_CMD_CLOSE,PRO_BLE,0,0);
}

void BtStartParing(void)
{
	mq_send_msg(BT_CMD_PARING,PRO_BLE,0,0);
}

void BtAllowConnect(void)
{
	mq_send_msg(BT_CMD_CONNECT,PRO_BLE,0,0);
}

bool bt_web_mq_is_ready(void)
{
	return !!access(BT_WEB_OPERATING, F_OK);
}

void occupy_bt_web_mq(void)
{
	creat(BT_WEB_OPERATING, S_IRWXU);
}

void free_bt_web_mq(void)
{
	unlink(BT_WEB_OPERATING);
}

void BtFactoryTest(const char  *fac_station)
{
	if(fac_station)
		mq_send_msg(BT_CMD_FAC_BT_TEST,PRO_BLE,(void *)fac_station,strlen(fac_station));
	else
		mq_send_msg(BT_CMD_FAC_BT_TEST,PRO_BLE,0,0);
}


void BtMonitorEnable(int enable,bt_Prot prot)
{
	char data=enable?0x01:0x00;
	mq_send_msg(BT_CMD_MONITOR_CFG,prot,&data,1);
}

void BtWebReadNotify(void)
{
	mq_send_msg(BT_CMD_WEB_NOTIFY,PRO_BLE,0,0);
}

void BtWebSet(void *data, int dlen)
{
	mq_send_msg(BT_CMD_WEB_SET, PRO_BLE, data, dlen);
}

/************************************************************/

void mq_send_msg_web(const bt_web_msg_data *buf)
{
	mqd_t mq;
	int msgsize = sizeof(bt_web_msg_data) + 1;
	struct mq_attr attr = {
		.mq_maxmsg = 5,
		.mq_msgsize = msgsize,
		.mq_curmsgs = 0,
	};

	if (NULL == buf) {
		return;
	}

	mq = mq_open(MQ_BT_WEB_SERVICE, O_WRONLY | O_NONBLOCK | O_CREAT, 0666, &attr);
	if (mq < 0)
	{
		LogError("mq_open(%s) fail(%d), %s!", MQ_BT_WEB_SERVICE, errno, strerror(errno));
		return;
	}

	if (mq_send(mq, (const char *)buf, sizeof(bt_web_msg_data), 0) < 0) {
		LogError("mq_send fail(%d), %s", errno, strerror(errno));
	}

	mq_close(mq);
}

int mq_recv_msg_web(bt_web_msg_data *buf)
{
	mqd_t mq;
	int sz;
	int msgsize = sizeof(bt_web_msg_data) + 1;
	struct mq_attr attr = {
		.mq_maxmsg = 5,
		.mq_msgsize = msgsize,
		.mq_curmsgs = 0,
	};

	if (NULL == buf) {
		LogError("out buffer is NULL");
		return -1;
	}

	mq = mq_open(MQ_BT_WEB_SERVICE, O_RDONLY | O_NONBLOCK | O_CREAT, 0666, &attr);
	if (mq < 0)
	{
		LogError("mq_open(%s) fail(%d), %s!", MQ_BT_WEB_SERVICE, errno, strerror(errno));
		return -2;
	}

	sz = mq_receive(mq, (char *)buf, msgsize, 0);
	LogInfo("mq recv size:%d", sz);
	if (sz < 0) {
		LogError("mq_receive %d:%s", errno, strerror(errno));
		sz = 0;
	}

	mq_close(mq);

	return sz;
}

/************************************************************/
int Bt_api_init(void)
{
	int fd;
#define OPEN_SEM(sem__,n__) \
do { \
	sem__ = sem_open(n__, O_RDWR); \
	if (sem__ == SEM_FAILED) { \
		LogFatal("sem_open() failed. name=\"%s\", errno=%d (%s)", n__, errno, strerror(errno)); \
		return -1; \
	} \
} while (0)

	OPEN_SEM(ble_txbuf_sem, BT_SERVER_SEM_BLE_TXBUFFER);
	OPEN_SEM(ble_rxbuf_sem, BT_SERVER_SEM_BLE_RXBUFFER);
	OPEN_SEM(spp_txbuf_sem, BT_SERVER_SEM_SPP_TXBUFFER);
	OPEN_SEM(spp_rxbuf_sem, BT_SERVER_SEM_SPP_RXBUFFER);

	fd = shm_open(BT_SERVER_SHM_NAME, O_RDWR, 0600);
	if (fd == -1) {
		LogFatal("shm_open() failed. errno=%d (%s)", errno, strerror(errno));
		return -1;
	}

	shm = (bt_shm_t *)mmap(NULL, sizeof(bt_shm_t),
			PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (shm == MAP_FAILED) {
		LogFatal("mmap() failed. errno=%d (%s)", errno, strerror(errno));
		return -1;
	}

	return 0;
}

int BtFacTestResult(void)
{
	//return shm->bt_fac_test_result;
	return  0;
}

unsigned int BtOpenSession(bt_Prot prot)
{
	if(prot ==PRO_BLE)
	{
		if((NORMAL_MODE==shm->ble_session.mode))
			return shm->ble_session.conn_id;
		else
			return 0;
	}
	else if (prot ==PRO_SPP)
	{
		return shm->spp_session.conn_id;
	}
	return 0;
}


int BtCloseSession(bt_Prot prot)
{
	MODULE_SHM_CHECK();
	
	if(prot ==PRO_BLE)
	{
		if((NORMAL_MODE==shm->ble_session.mode))
			mq_send_msg(BT_CMD_CLOSE_SESSION,PRO_BLE,0,0);
		else
			return -1;
	}
	else if (prot ==PRO_SPP)
	{
		mq_send_msg(BT_CMD_CLOSE_SESSION,PRO_SPP,0,0);
	}
	return 0;
}

int BtGetState(bt_Prot prot)
{
	MODULE_SHM_CHECK();
	
	if(prot ==PRO_BLE)
	{
		return shm->ble_stat;
	}
	else if (prot ==PRO_SPP)
	{
		return shm->spp_stat;
	}

	return -1;
}

int BtGetInfo(bt_Prot prot,unsigned char  *mac)
{
	 char  buf[64]={0};

	MODULE_SHM_CHECK();
	
	if(prot ==PRO_BLE)
	{
		if(shm->ble_stat)
		{
			snprintf(buf,64,"%02X:%02X:%02X:%02X:%02X:%02X", shm->ble_session.remote_bda[0], shm->ble_session.remote_bda[1],
                             shm->ble_session.remote_bda[2], shm->ble_session.remote_bda[3],
                              shm->ble_session.remote_bda[4], shm->ble_session.remote_bda[5]);
		   memcpy(mac, buf, 17);
		   return 0;
		}
		else
		{	
			return -1;
		}
	}
	else if (prot ==PRO_SPP)
	{
		if(shm->spp_stat)
		{
			snprintf(buf,64,"%02X:%02X:%02X:%02X:%02X:%02X", shm->spp_session.remote_bda[0], shm->spp_session.remote_bda[1],
                             shm->spp_session.remote_bda[2], shm->spp_session.remote_bda[3],
                              shm->spp_session.remote_bda[4], shm->spp_session.remote_bda[5]);
		   memcpy(mac, buf, 17);
		   return 0;
		}
		else
		{	
			return -1;
		}
	}
	return -1;
}

int BtRecvData( bt_Prot prot,unsigned int  conn_id,unsigned char *data, unsigned int dataLenght, unsigned int timeoutMs)
{
	int ret=-1,iRet,cnt=0;
	unsigned long time;

	MODULE_SHM_CHECK();
	
	if(timeoutMs == 0)
	{
		cnt = do_read_data(prot,conn_id,data,dataLenght);
		if(cnt >=0)
		{
			ret = cnt;
		}
		else
		{
			ret =-1;  //BASE_EINVAL
		}
	}
	else
	{	
		cnt =0;
		time=SysGetTickCount();
		while((cnt<dataLenght) && ((SysGetTickCount()-time) < timeoutMs))
		{
			iRet = do_read_data(prot,conn_id,data,dataLenght);
			if(iRet>0)
			{
				cnt+=iRet;
			}
			SysDelay(5);
		}

		ret =cnt;
	}
		
	return ret;
}

int BtSendData(bt_Prot prot,unsigned int  conn_id,unsigned char *data,int dataLenght)
{
	int ret=-1,cnt=0;

	MODULE_SHM_CHECK();
	
	cnt = do_send_data(prot,conn_id,data,dataLenght);
	if(cnt >=0)
	{
		ret = cnt;
	}
	else
	{
		ret =-1; 
	}

	return ret;
}

int BtFlush(bt_Prot prot,int flags)
{
	int ret =0;
	MODULE_SHM_CHECK();
	
	if(prot==PRO_BLE)
	{
		CLEAR_BUF(ble,BLE,flags);
	}
	else if(prot==PRO_SPP)
	{
		CLEAR_BUF(spp,SPP,flags);
	}
	else
		ret =-1;
	return ret;
}

int BtGetLife(bt_Prot prot)
{
	MODULE_SHM_CHECK();
	
	if(prot ==PRO_BLE)
	{
		return shm->ble_alive;
	}
	else if (prot ==PRO_SPP)
	{
		return shm->spp_alive;
	}

	return -1;
}
