#ifndef __APP_SETTING_H__
#define __APP_SETTING_H__

#include <time.h>

#define DEFAULT_SYSTEM_TIME	1577836800	//2020-01-01 00:00:00 GMT
#define MIN_SYNCED_TIME		1625097600	//2021-07-01 00:00:00 GMT
#define isTimeSynced() ( time(0)>MIN_SYNCED_TIME )

#define APP_SETTING_PRINT_DISTANCE    "print_distance"
#define APP_SETTING_AUDIO_SECTION     "audio"
#define APP_SETTING_AUDIO_SWITCHES    "audio_switches"
#define APP_SETTING_SECTION_WNET      "wnet"
#define APP_SETTING_WNET_KEY_SWITCH   "switch" //0: not external app decide;  1:internal app decide
#define APP_SETTING_WNET_KEY_APN      "apn"
#define APP_SETTING_WNET_KEY_DIAL_NUM "dial_number"
#define APP_SETTING_WNET_KEY_FOTA     "fota"   //0: previous status not in fota update;  1:previous status in fota update
#define APP_SETTING_WNET_VAL_FOTA_TRIGGER   99
#define APP_SETTING_WNET_KEY_SIM_MODE "mode"   //0: HWSIM;  1:SOFTSIM


#define APP_SETTING_MASTER_AUDIO_SWITCH      6
#define APP_SETTING_CLOUD_AUDIO_SWITCH       5
#define APP_SETTING_WARNNING_AUDIO_SWITCH    4
#define APP_SETTING_NOTICE_AUDIO_SWITCH      3
#define APP_SETTING_PAPER_TAKEN_AUDIO_SWITCH 2
#define APP_SETTING_PAPER_BEEP_AUDIO_SWITCH  1
#define APP_SETTING_PAPER_BEEP_AUDIO_CHANGE  0
int app_setting_set_audio_switches(uint8_t type, uint8_t state);
int app_setting_get_audio_switches(uint8_t type);


char *app_setting_get(const char *section,const char *key,char *value,int value_len);
int app_setting_get_int(const char *section,const char *key,int def_value);
int app_setting_set(const char *section,const char *key,const char *value);
int app_setting_set_int(const char *section,const char *key,int val);

#endif
