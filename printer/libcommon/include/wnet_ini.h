#ifndef __WNET_INI_H__
#define __WNET_INI_H__

char *wnet_ini_get(const char *key,char *value,int value_len);
int wnet_ini_get_int(const char *key,int def_value);
int wnet_ini_set(const char *key,const char *value);
int wnet_ini_set_int(const char *key,int value);

#define WNET_KEY_IMEI     "imei"
#define WNET_KEY_ICCID    "iccid"
#define WNET_KEY_IMSI     "imsi"
#define WNET_KEY_MCC      "mcc"
#define WNET_KEY_MNC      "mnc"
#define WNET_KEY_APN      "apn"
#define WNET_KEY_DIAL     "dial"
#define WNET_KEY_ISP      "isp"
#define WNET_KEY_TYPE     "type"
#define WNET_KEY_CPSI     "cpsi"  //system information(network information)
#define WNET_KEY_VER      "ver"
#define WNET_KEY_LBS      "lbs"
#define WNET_KEY_RSSI_DB  "rssi_db"
#define WNET_KEY_RSSI     "rssi"
#define WNET_KEY_STATUS   "status"

#define WNET_FOTA_BIN_PATH "/data/wnet_fota/wnet_fota.bin"

#endif
