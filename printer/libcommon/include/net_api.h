#ifndef __NETWORK_API_H__
#define __NETWORK_API_H__

#include <net_cust.h>
#include <cJSON.h>


/*
*open WNET_IPV4V6 when modules (L506 and EG912) are ready 
*/
#define WNET_IPV4V6                                         0       /* 0:close;  1:open;*/


enum {
	NET_NONE = 0,
	NET_WIFI,
	NET_WNET,
	NET_LAN,
	NET_MAX,
};

enum {
	NET_ST_DISABLE,
	NET_ST_LINKING,
	NET_ST_LINKUP,/* 有线网络已连接或无线WPA已连接 */
	NET_ST_CONNECTED, /* 已获取IP */
};

extern ST_WIFI_SCAN_RESULT *net_wifi_scan(void);
extern int NetGetInfo(char *interface,void *netInfo);
extern int NetGetCurRoute(void);
extern void NetStartTestMode();
extern void NetWnetStartFota(int mode);
extern void NetWnetStartHttpsFota(char *url);
extern void NetWnetGetFota();



#define MQ_NET_SERVICE	"/net_service"
#define MQ_NET_MAX_SIZE 128
enum {
	NET_CMD_WNET_START,
	NET_CMD_WNET_STOP,
	NET_CMD_WNET_RESTART,
	NET_CMD_WNET_FAC_TEST,
	NET_CMD_WNET_FOTA,
	NET_CMD_WNET_HTTPS_FOTA,
	NET_CMD_WNET_GET_FOTA,
};

typedef enum {
	LAN_ST_LINKDOWN,
	LAN_ST_CONNECTING,
	LAN_ST_CONNECTED
} lan_status_t;

typedef enum {
	WIFI_ST_LINKDOWN,
	WIFI_ST_WPA_COMPLETE,
	WIFI_ST_CONNECTED,
} wifi_status_t;

typedef enum wpa_states {
	WPA_ST_DISCONNECTED,
	WPA_ST_IFACE_DISABLED,
	WPA_ST_INACTIVE,
	WPA_ST_SCANNING,
	WPA_ST_AUTHENTICATING,
	WPA_ST_ASSOCIATING,
	WPA_ST_ASSOCIATED,
	WPA_ST_4WAY_HANDSHAKE,
	WPA_ST_GROUP_HANDSHAKE,
	WPA_ST_COMPLETED
} wpa_status_t;

typedef struct {
	short cmd;
	char data[];
} net_mq_msg_t;
int net_wifi_start(const char *ssid,const char *passwd);
bool net_wifi_is_stopped();
void net_wifi_stop();
void net_update_config(int net_type,const char *param);
bool net_lan_is_linkup(void);
bool net_wifi_is_connected();
cJSON *net_load_config(int net_type);
int net_wifi_get_rssi(int *rssi);
void NetStartTestMode();
const char *net_ifname(int net_type);
bool net_lan_is_connected();
void net_wifi_load();
int wpa_status(void);
bool net_wnet_is_connected();
int update_LPrtSrv_config(int net_type,const char *ip,const char *sm,const char *gw,const char *dns);
int net_LPrtSrv_ini_set(const char *section,const char *key,const char *value);
int net_LPrtSrv_ini_get(const char *section,const char *key,char *value,int len);
void net_wnet_restart();
#endif
