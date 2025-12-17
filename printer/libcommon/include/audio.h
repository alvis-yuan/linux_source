#ifndef __AUDIO_UTIL_H__
#define __AUDIO_UTIL_H__

#define MQ_AUDIO_SERVICE	"/mq_audio_service"
#define AUDIO_MQ_MAX_SIZE	2048
#define AUDIO_CONFIG_FILE	"/data/SYS/audio.cfg"

#define AUD_ID_CLOUD_WAVE		-1
#define AUD_ID_OTHER_FILE		-2
#define AUD_ID_SET_TTS_PARAM	-3
#define AUD_ID_SET_SYS_VOLUME	-4
#define AUD_ID_SET_VOLUMEADD	-5
#define AUD_ID_SET_VOLUMESUB	-6
#define AUD_ID_UPDATE_RES		-7
#define AUD_DEL_LIST_NODE       -8
#define AUD_GEN_FIXED       	-9

typedef struct {
	short cmd;
	short repeats;
	short delay;
	char data[];
}audio_msg_t;

typedef struct {
	short cmd;
	short vol;
	short play;
} msg_set_vol_t;

typedef struct {
	char version[15];
	char PreGenerated;
	char voice_name[32];
	char text_encoding[16];
	short sample_rate; //8000 or 16000
	char speed; //0~100
	char volume; //0~100
	char pitch;	//0~100
	char rdn;
	char online; // 0-offline 1-webapi 2-SDK online
	char allow_net; // 0-disable 1-enable on all net 2-only enable lan/wifi,gprs/4G disable
	char tte[16]; //文本编码格式
	char ent[16]; //引擎类型 intp65(中文,默认),intp65_en(英文),mtts(小语种)
	char bgs; //0-无背景音 1-有背景音
	char lang[32];
}__attribute__((packed)) tts_params_t;

#define AUDIO_CFG_MAGIC 0x50647541
typedef struct {
	int magic;
	short sys_volume; //系统音量, 1~8
	tts_params_t tts;
}__attribute__((packed)) audio_cfg_t; 

#define DEFAULT_AUDIO_CFG {"0.0.1",0,"xiaoyan","UTF8",16000,50,99,60,0,TTS_OFFLINE,TTS_WEBAPI_NET_ETH_WIFI,"UTF8","intp65",0,"chinese"}
enum {
	TTS_OFFLINE = 0, // 离线合成语音
	TTS_ONLINE_WEBAPI =1,	//在线webapi合成
	TTS_ONLINE_SDK,	//SDK在线合成
};

enum {
	TTS_WEBAPI_NET_NONE=0, //禁止webapi在线合成
	TTS_WEBAPI_NET_ALL,	//所有网络均允许webapi在线合成
	TTS_WEBAPI_NET_ETH_WIFI	//只在有线或WIFI网络下允许在线合成, GPRS/4G网络下禁用webapi
};

enum {
	VOLUME_MIN = 0,
	VOLUME_L1,
	VOLUME_L2,
	VOLUME_L3,
	VOLUME_L4,
	VOLUME_L5,
	VOLUME_L6,
	VOLUME_L7,
	VOLUME_L8,
	VOLUME_MAX,
	WIFI_CONNECTED,
	MOBILE_CONNECTED,
	LAN_CONNECTED,
	NO_PAPER,
	PAPER_JAM,
	PAPER_TAKEN,
	PAPER_RUN_OUT,
	PAPER_BLOCK,
	PAPER_GET,
	PAPER_TAG_SPLIT,
	NET_DISCONNECTED,
	TOO_HOT,
	//MOTOR_TOO_HOT,
	TOO_COLD,
	CUT_ERROR,
	DOOR_OPEN,
	VOLTAGE_TOO_HIGH,
	VOLTAGE_TOO_LOW,
	WIFI_TIMEOUT,
	WIFI_SET,
	PRINTER_ERROR,
	FACTORYSTART,
	FACTORYEND,
	BURNINTESTSTART,
	BURNINTESTEND,
	POWERONEND,
	NET_CHECKING,
	USBSTORAGEOK,
	BURNINTESTFINISH,
	NEED_WIFI_UPDTE,
	OTAFILE_BEGINE_DLD,
	RSCFILE_BEGINE_DLD,
	NEW_VERSION_NOW_UPDATE,
	UPDATE_FAILED,
	UPDATE_AFTER_REBOOT,
	UPDATE_DONT_POWEROFF,
	UPDATE_SUCC_REBOOT,
	RSC_UPDATE_SUCC,
	UPDATE_AFTER_SECONDS,
	FUNCTION_KEY_PRESS,
	RESETTING,
	WIFI_SET_END,
	BTPAIRING,
	BTPAIRING_END,

	WAVE_FILE_INDEX_MAX,
};

#endif
