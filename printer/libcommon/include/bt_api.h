#ifndef __BULETOOTH_API_H__
#define __BULETOOTH_API_H__
#include <semaphore.h>

#define BT_BOND_RECORDS_BIN "/data/bluetooth/bind_records"
#define MQ_BT_WEB_SERVICE	"/mq_bt_web_service"
#define MQ_BT_SERVICE	"/mq_bt_service"
#define BT_MQ_MAX_SIZE	2048

#define BT_SERVER_SHM_NAME	"/shm_bt_server"
#define BT_SERVER_SEM_BLE_TXBUFFER	"/bt_server.ble_txbuf"
#define BT_SERVER_SEM_BLE_RXBUFFER	"/bt_server.ble_rxbuf"
#define BT_SERVER_SEM_SPP_TXBUFFER	"/bt_server.spp_txbuf"
#define BT_SERVER_SEM_SPP_RXBUFFER	"/bt_server.spp_rxbuf"

// 保存 wpa_cli 扫描wifi的信息
#define STORAGE_BLE_SCAN_WIFI_INFO  "/tmp/stroage_wifi_scan_info"
#define STORAGE_BLE_SCAN_WIFI_INFO_BAK  "/tmp/stroage_wifi_scan_info_bak"

#define BT_CMD_OPEN                0x61
#define BT_CMD_CLOSE              0x62
#define BT_CMD_PARING              0x63
#define BT_CMD_FAC_BT_TEST         0x64
#define BT_CMD_IBEACON_CFG         0x65
#define BT_CMD_MONITOR_CFG         0x66
#define BT_CMD_CLOSE_SESSION         0x70
#define BT_CMD_CONNECT             0x71
#define BT_CMD_WEB_NOTIFY          0x72
#define BT_CMD_WEB_SET             0x73


#define  BT_BLE_RXBUF_MAX_LEN      (1024*256)//256k
#define  BT_BLE_TXBUF_MAX_LEN      (1024*4)//4k
#define  BT_SPP_RXBUF_MAX_LEN      (1024*256)//256k
#define  BT_SPP_TXBUF_MAX_LEN      (1024*4)//4k

#define RINGBUF_SIZE  (1)//(1024*256)

//spp support passback flag
#define SPP_SUPPORT_PASSBACK 0x04

typedef enum{
	PRO_BLE=0,
	PRO_SPP,
	//TBD
}bt_Prot;

typedef enum{
	NORMAL_MODE=0,
	PARING_MODE,
}ble_work_mode_t;


typedef struct {
	short cmd;
	bt_Prot  prot;
	char data[];
}bt_msg_t;

typedef struct
{
	unsigned char  source[RINGBUF_SIZE];
	unsigned int  br;
	unsigned int  bw;
	unsigned int btoRead;
    unsigned int length;
}ringbuffer_t;

typedef struct {
	ble_work_mode_t       mode;
	unsigned int       conn_id;
	unsigned char      remote_bda[6];
	ringbuffer_t   rxBuff;
	ringbuffer_t   txBuff;
	unsigned char  rxPool[BT_BLE_RXBUF_MAX_LEN];
	unsigned char  txPool[BT_BLE_TXBUF_MAX_LEN];
}ble_session_t;

typedef struct {
	unsigned int       conn_id;
	unsigned char      remote_bda[6];
	ringbuffer_t   rxBuff;
	ringbuffer_t   txBuff;
	unsigned char  rxPool[BT_SPP_RXBUF_MAX_LEN];
	unsigned char  txPool[BT_SPP_TXBUF_MAX_LEN];
}spp_session_t;

typedef struct {
	unsigned int ble_alive; //ble pthread  stat:0 -exited, 1-running
	unsigned int spp_alive; //spp exit stat:0-exited,  1-running
	int ble_stat;    //ble connect stat:0-disconnected 1 -connected
	int spp_stat;    //spp connect stat:0-disconnected 1-connected
	ble_session_t  ble_session;
	spp_session_t  spp_session;
} bt_shm_t;

typedef struct{
	unsigned int stSize;				
	unsigned char ibeacon_uuid[16];
	unsigned char major_num[2];
	unsigned char minor_num[2];
	char db_in_1_meter; 
}BT_IBEACON_CONFIG;

extern int Bt_api_init(void);
extern void BtPwrOn(void);
extern void BtPwrOff(void);
extern void BtStartParing(void);
extern void BtFactoryTest(const char  *fac_station);
extern int BtGetState(bt_Prot prot);
extern int BtGetInfo(bt_Prot prot,unsigned char  *mac);
extern unsigned int BtOpenSession(bt_Prot prot);
extern int BtCloseSession(bt_Prot prot);
extern int BtRecvData( bt_Prot prot,unsigned int  conn_id,unsigned char *data, unsigned int dataLenght, unsigned int timeoutMs);
extern int BtSendData(bt_Prot prot,unsigned int  conn_id,unsigned char *data,int dataLenght);
extern int BtFlush(bt_Prot prot,int flags);
extern void BtMonitorEnable(int enable,bt_Prot prot);
extern int BtGetLife(bt_Prot prot);
extern void BtWebReadNotify(void);
extern void BtWebSet(void *data, int dlen);
void BtAllowConnect(void);

#ifndef APP_MAX_NB_REMOTE_STORED_DEVICES
#define APP_MAX_NB_REMOTE_STORED_DEVICES 10
#endif

typedef struct {
	char name[128];     // 设备名  Bluetooth规范: 设备名称是UTF-8编码的字符串，最大长度为248字节
	char mac[32];        // 设备地址
	char type[24];      // 设备类型
	char proto[8];      // 蓝牙通道
	bool connect;       // 连接状态
	char pairTime[24];  // 配对时间			2024-01-25 09:10:00
	char conTime[24];   // 最近的连接时间
} bt_pair_records;      // 配对记录

typedef struct {
	char name[64]; // 本机 设备名
	char mac[32];  // 本机 设备地址	08:00:27:cb:dc:81
	int mode;      // ble_work_mode_t
	int conNum;    // 连接数量: ble(0~1) + spp(0~4);
	int recNum;    // 记录数量: 0~10
	int bind;      // 绑定模式: 0-手动绑定; 1-自动绑定; 2-手动绑定+语音提醒
	bt_pair_records records[APP_MAX_NB_REMOTE_STORED_DEVICES];
} bt_web_msg_data;

typedef unsigned char BD_ADDR[6];
typedef struct {
	bool cleFlag;    // 清空记录标志，为 true 清空记录，所有设备在下次配对时需重新验证（手动绑定）
	int delCnt;  // 删除记录个数，值delMac数组成员个数
	BD_ADDR delMac[APP_MAX_NB_REMOTE_STORED_DEVICES]; // 删除记录，该mac设备在下次配对时需重新验证（手动绑定）
	int bind;        // 绑定模式: 0-手动绑定; 1-自动绑定; 2-手动绑定+语音提醒
} bt_web_msg_cmd;

extern void mq_send_msg_web(const bt_web_msg_data *buf);
extern int mq_recv_msg_web(bt_web_msg_data *buf);
bool bt_web_mq_is_ready(void);
void occupy_bt_web_mq(void);
void free_bt_web_mq(void);

#endif

