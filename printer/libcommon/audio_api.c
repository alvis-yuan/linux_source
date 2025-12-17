#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <errno.h>
#include <sys/mman.h>

#include <libcommon.h>


static uint8_t need_play_text(const char *text)
{
	if (!app_setting_get_audio_switches(APP_SETTING_MASTER_AUDIO_SWITCH))
		return 0;

	if (!text || !strlen(text))
	{
		LogError("text must not be empty!");
		return 0;
	}

	return app_setting_get_audio_switches(APP_SETTING_CLOUD_AUDIO_SWITCH);
}

static uint8_t need_play_file(const char *filename)
{
	const char *prefix = "/data/PUB/beep/";
#define BEE_FILE_PATH "/home/audio/bee.wav"
	if (!app_setting_get_audio_switches(APP_SETTING_MASTER_AUDIO_SWITCH))
		return 0;

	if (!filename || !strlen(filename)) {
		LogError("fname must not be empty!");
		return 0;
	}

	if ((!strcmp(filename, BEE_FILE_PATH)) || strncmp(filename, prefix, strlen(prefix)) == 0) {
		return app_setting_get_audio_switches(APP_SETTING_PAPER_BEEP_AUDIO_SWITCH);
	}

	return app_setting_get_audio_switches(APP_SETTING_CLOUD_AUDIO_SWITCH);
#undef BEE_FILE_PATH
}

uint8_t need_paly_fixed(int id)
{
	if (!app_setting_get_audio_switches(APP_SETTING_MASTER_AUDIO_SWITCH))
		return 0;

	if( id<0 || id>=WAVE_FILE_INDEX_MAX ) {
		LogError("id must be in 0 - %d",WAVE_FILE_INDEX_MAX);
		return 0;
	}

	switch (id)
	{
		case VOLUME_MIN       :
		case VOLUME_L1        :
		case VOLUME_L2        :
		case VOLUME_L3        :
		case VOLUME_L4        :
		case VOLUME_L5        :
		case VOLUME_L6        :
		case VOLUME_L7        :
		case VOLUME_L8        :
		case VOLUME_MAX       :
		case WIFI_CONNECTED   :
		case MOBILE_CONNECTED :
		case LAN_CONNECTED    :
		case PAPER_GET        :
		case NET_DISCONNECTED :
		case DOOR_OPEN        :
		case POWERONEND       :
		case NET_CHECKING     : return app_setting_get_audio_switches(APP_SETTING_NOTICE_AUDIO_SWITCH)?1:0;
		case PAPER_RUN_OUT    :
		case NO_PAPER         :
		case PAPER_JAM        :
		case PAPER_BLOCK      :
		case TOO_HOT          :
		case TOO_COLD         :
		case VOLTAGE_TOO_HIGH :
		case VOLTAGE_TOO_LOW  : return app_setting_get_audio_switches(APP_SETTING_WARNNING_AUDIO_SWITCH)?1:0;
		case PAPER_TAKEN      : return app_setting_get_audio_switches(APP_SETTING_PAPER_TAKEN_AUDIO_SWITCH)?1:0;

		default               : return 1;
	}
}

static void audio_mq_send_msg(short cmd,short repeats,short delay,const char *data)
{
	int len;
	char buf[AUDIO_MQ_MAX_SIZE+1] = {};
	audio_msg_t *msg = (audio_msg_t *)buf;
	msg->cmd = cmd;
	msg->repeats = repeats;
	msg->delay = delay;
	len = sizeof(*msg);
	if( data && strlen(data) )
		len += snprintf(msg->data,sizeof(buf)+buf-msg->data,"%s",data);
	mqd_t mq = mq_open(MQ_AUDIO_SERVICE,O_WRONLY|O_NONBLOCK);
	if( mq<0 ) {
		LogError("AudioServer may not run!!!");
		return;
	}
	if( mq_send(mq,buf,len,0)<0 )
		LogError("mq_send fail(%d),%s",errno,strerror(errno));
	mq_close(mq);
}

void GenFixedAudio(int id)
{
	audio_mq_send_msg(AUD_GEN_FIXED, id, 0, "");
}

void AudioPlayFixed(int id, short repeats, short delay)
{
	if (!need_paly_fixed(id))
		return;

	audio_mq_send_msg(id, repeats, delay, "");
}

void AudioPlayFile(const char *fname, short repeats, short delay)
{
	if (!need_play_file(fname))
		return;

	audio_mq_send_msg(AUD_ID_OTHER_FILE, repeats, delay, fname);
}

void AudioPlayText(const char *text, short repeats, short delay)
{
	if (!need_play_text(text))
		return;

	audio_mq_send_msg(AUD_ID_CLOUD_WAVE, repeats, delay, text);
}

void AudioSetVolume(int vol,int play)
{
	audio_mq_send_msg(AUD_ID_SET_SYS_VOLUME,vol,play,0);
}

void AudioVolumeAdd(int play)
{
	audio_mq_send_msg(AUD_ID_SET_VOLUMEADD,0,play,0);
}

void AudioVolumeSub(int play)
{
	audio_mq_send_msg(AUD_ID_SET_VOLUMESUB,0,play,0);
}

void AudioSetTTSParam(const char *tts_param)
{
	audio_mq_send_msg(AUD_ID_SET_TTS_PARAM,0,0,tts_param);
}

void AudioResUpdate(void)
{
	audio_mq_send_msg(AUD_ID_UPDATE_RES,0,0,0);
}

void AudioDelListNode(short id)
{
	char data[8] = {};
	snprintf(data, sizeof(data), "%d", id);
	audio_mq_send_msg(AUD_DEL_LIST_NODE, 0, 0, data);
}

audio_cfg_t *get_audio_cfg_mmap()
{
	audio_cfg_t *cfg = NULL;
	int fd = open(AUDIO_CONFIG_FILE,O_RDONLY);
	if( fd<0 ) return NULL;
	off_t sz = lseek(fd,0,SEEK_END);
	if( sz<sizeof(*cfg) ) return cfg; 
	cfg = (audio_cfg_t *)mmap(NULL,sizeof(audio_cfg_t),PROT_READ,MAP_SHARED,fd,0);
	close(fd);
	if( cfg && cfg->magic == AUDIO_CFG_MAGIC ) return cfg;
	if( cfg ) munmap(cfg,sizeof(*cfg));
	return NULL;
}
