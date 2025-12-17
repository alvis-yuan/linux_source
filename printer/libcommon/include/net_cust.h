#ifndef __NET_CUST_H__
#define __NET_CUST_H__

//system error code
#define BASE_SYS_EREINIT     -2000
#define BASE_SYS_ETIMEOUT    -2001
#define BASE_SYS_EDATA       -2002
#define BASE_SYS_EAGAIN      -2003


//Net Error Code
#define BASE_NET_ENOREADY      -6000  /* Net No Ready */
#define BASE_NET_ENOLBS        -6001
#define BASE_NET_EDNFAILE      -6002

/******************** common ***************/
/*common Error Code*/
#define	BASE_ENOENT      -1000	/* No such file or directory */
#define	BASE_EIO         -1001	/* I/O error */
#define	BASE_ENXIO       -1002	/* No such device or address */
#define	BASE_ENOMEM      -1003	/* Out of memory */
#define	BASE_EACCES      -1004	/* Permission denied */
#define	BASE_EBUSY       -1005	/* Device or resource busy */
#define	BASE_EEXIST      -1006	/* File exists */
#define	BASE_ENODEV      -1007	/* No such device */
#define	BASE_EINVAL      -1008	/* Invalid argument */
#define BASE_EINTR        -1009
#define BASE_EAGAIN       -1010  
#define BASE_BATTERY_LOW  -1011
#define BASE_ECONNRESET   -1012
#define BASE_ENOSPC       -1013
#define BASE_ECERT          -1014


#define WIFI_AUTH_NONE          0
#define WIFI_AUTH_WEP           1
#define WIFI_AUTH_WPA_PSK       2
#define WIFI_AUTH_WPA2_PSK      4    
#define WIFI_AUTH_WPA_WPA2_PSK  (WIFI_AUTH_WPA_PSK|WIFI_AUTH_WPA2_PSK)


#define WIFI_SCAN_AP_MAX   30

typedef struct{
    unsigned int stSize;
    char ipAddr[64];
    char gateway[64];
    char submask[64];
    char dns1[64];
    char dns2[64];
}ST_NET_SETTING;

typedef struct{
    unsigned int stSize;
    char essid[64]; 
    unsigned int authMode;
    unsigned int idx;
    unsigned int len[4];
    unsigned char key[4][64];
}ST_WIFI_AP;

typedef struct{
    unsigned int stSize;
    char apn[64];
    char uid[64];
    char pwd[64];
    char dialNum[64];
}ST_WNET;

typedef struct{
    char Gis[64];
    char GisDesc[256];
    unsigned char reserved[256];
}ST_LBS;

typedef enum{
    NET_DISABLE=0,
    NET_DISABLING,
    NET_ENABLING,
    NET_ENABLE,
    NET_CONNECTED,
    NET_CONNECTING,
    NET_READY,
}NET_STATE;

typedef struct{
    unsigned int stSize;
    char  essid[64];
    char  bssid[64];
    unsigned int authMode;
    int rssi;
    int rssiDB;
    int dhcpEnable;
    char mac[64];
    char ipAddr[64];
    char gateway[64];
    char submask[64];
    char dns1[64];
    char dns2[64];
}ST_WIFI_INFO;

typedef struct{
	unsigned int stSize;		//缁撴瀯浣撳ぇ灏?蹇呴』绛変簬sizeof(ST_WIFI_INFO)
	char essid[64];			//瀛楃涓?鐑偣鍚嶅瓧
	char bssid[64];		//WIFI鐑偣MAC鍦板潃锛屽瓧绗︿覆
	unsigned int authMode;	//WIFI璁よ瘉妯″紡,鍙傝€僕IFI璁よ瘉妯″紡
	int rssi;				//淇″彿寮哄害锛屽€间负0~4锛?琛ㄧず鏃犱俊鍙凤紝4琛ㄧず淇″彿鏈€寮?
	int freq;
}ST_WIFI_SCAN_AP;

typedef struct {
	int count;
	ST_WIFI_SCAN_AP list[];
}ST_WIFI_SCAN_RESULT;

typedef struct{
    unsigned int stSize;
    char dataInfo[64];
    int rssi;
    char ipAddr[64];
    char gateway[64];
    char submask[64];
    char dns1[64];
    char dns2[64];
}ST_WNET_INFO;


typedef struct{
	unsigned int stSize;		//缁撴瀯浣撳ぇ灏?蹇呴』绛変簬sizeof(ST_LAN_INFO)
	int dhcpEnable;				//1锛氫娇鑳絛hcp鑷姩鑾峰彇ip鍦板潃锛?锛氱鐢╠hcp
	char mac[64];				//LAN  MAC鍦板潃淇℃伅
	char ipAddr[64];			//IP鍦板潃,瀛楃涓诧紝鍙傝€僆P鍦板潃瀛楃涓?
	char gateway[64];		//缃戝叧,瀛楃涓诧紝鍙傝€僆P鍦板潃瀛楃涓?
	char submask[64];		//瀛愮綉鎺╃爜,瀛楃涓诧紝鍙傝€僆P鍦板潃瀛楃涓?
	char dns1[64];				//浼樺厛DNS鏈嶅姟鍣ㄥ湴鍧€,瀛楃涓诧紝鍙傝€僆P鍦板潃瀛楃涓?
	char dns2[64];				//澶囩敤DNS鏈嶅姟鍣ㄥ湴鍧€,瀛楃涓诧紝鍙傝€僆P鍦板潃瀛楃涓?
}ST_LAN_INFO;

#endif
