#include <libcommon.h>
#define APP_SETTING_INI_FILE "/data/SYS/app_setting.ini"

char *app_setting_get(const char *section,const char *key,char *value,int value_len)
{
	return ini_get_value_with_section(APP_SETTING_INI_FILE,section,key,value,value_len);
}

int app_setting_get_int(const char *section,const char *key,int def_value)
{
	char buf[64];
	if( !app_setting_get(section,key,buf,sizeof(buf)) ) return def_value;
	return atoi(buf);
}

int app_setting_set(const char *section,const char *key,const char *value)
{
	return ini_set_value_with_section(APP_SETTING_INI_FILE,section,key,value);
}

int app_setting_set_int(const char *section,const char *key,int val)
{
	char buf[64];
	snprintf(buf,sizeof(buf),"%d",val);
	return app_setting_set(section,key,buf);
}


/*
 * MSB   : master            switch
 * bit6  : cloud       audio switch
 * bit5  : warning     audio switch
 * bit4  : notice      audio switch
 * bit3~2: paper-taken audio switch
           0-0: no   paper-taken, no   beep
           0-1: no   paper-taken, play beep
           1-0: play paper-taken, no   beep
           1-1: paly paper-taken, play beep
 * bit1  :reserved
 * LSB   :reserved
 *
 *   MSB  6   5   4   3   2   1  LSB
 *  +---+---+---+---+---+---+---+---+
 *  |   |   |   |   |   |   |   |   |
 *  +---+---+---+---+---+---+---+---+
 *
 */

static uint8_t get_audio_switchs(void);

#define MASTER_SWITCH_GET      ((get_audio_switchs() & 0X80) >> 7)
#define CLOUD_SWITCH_GET       ((get_audio_switchs() & 0X40) >> 6)
#define WARNNING_SWITCH_GET    ((get_audio_switchs() & 0X20) >> 5)
#define NOTICE_SWITCH_GET      ((get_audio_switchs() & 0X10) >> 4)
#define PAPER_TAKEN_SWITCH_GET ((get_audio_switchs() & 0X08) >> 3)
#define PAPER_BEEP_SWITCH_GET  ((get_audio_switchs() & 0X04) >> 2)
#define PAPER_BEEP_CHANGE_GET  ((get_audio_switchs() & 0X02) >> 1)

#define MASTER_SWITCH_SET      (get_audio_switchs() | 0X80)
#define CLOUD_SWITCH_SET       (get_audio_switchs() | 0X40)
#define WARNNING_SWITCH_SET    (get_audio_switchs() | 0X20)
#define NOTICE_SWITCH_SET      (get_audio_switchs() | 0X10)
#define PAPER_TAKEN_SWITCH_SET (get_audio_switchs() | 0X08)
#define PAPER_BEEP_SWITCH_SET  (get_audio_switchs() | 0X04)

#define MASTER_SWITCH_CLEAR      (get_audio_switchs() & 0X7f)
#define CLOUD_SWITCH_CLEAR       (get_audio_switchs() & 0Xbf)
#define WARNNING_SWITCH_CLEAR    (get_audio_switchs() & 0Xdf)
#define NOTICE_SWITCH_CLEAR      (get_audio_switchs() & 0Xef)
#define PAPER_TAKEN_SWITCH_CLEAR (get_audio_switchs() & 0Xf7)
#define PAPER_BEEP_SWITCH_CLEAR  (get_audio_switchs() & 0Xfb)

static uint8_t get_audio_switchs(void)
{
	int value = app_setting_get_int(APP_SETTING_AUDIO_SECTION, APP_SETTING_AUDIO_SWITCHES, -1);
	if (value < 0x00 || value > 0xff)
	{
		//LogFatal("unusual %s value[%d], set all audio switchs on", APP_SETTING_AUDIO_SWITCHES, value);
		return 0xfd;
	}
	return value;
}

#define OFF 0
#define ON  1
/*******************************
 * state 0: OFF                *
 * state 1: ON                 *
 * type  1: PAPER_BEEP_SWITCH  *
 * type  2: PAPER_TAKEN_SWITCH *
 * type  3: NOTICE_SWITCH      *
 * type  4: WARNNING_SWITCH    *
 * type  5: CLOUD_SWITCH       *
 * type  6: MASTER_SWITCH      *
 *******************************/
int app_setting_set_audio_switches(uint8_t type, uint8_t state)
{
	if (state != OFF && state != ON)
		return -1;

	uint8_t value = get_audio_switchs();

	switch (type)
	{
	case APP_SETTING_PAPER_BEEP_AUDIO_SWITCH:
		value = state ? PAPER_BEEP_SWITCH_SET  : PAPER_BEEP_SWITCH_CLEAR;
		break;
	case APP_SETTING_PAPER_TAKEN_AUDIO_SWITCH:
		value = state ? PAPER_TAKEN_SWITCH_SET : PAPER_TAKEN_SWITCH_CLEAR;
		break;
	case APP_SETTING_NOTICE_AUDIO_SWITCH:
		value = state ? NOTICE_SWITCH_SET      : NOTICE_SWITCH_CLEAR;
		break;
	case APP_SETTING_WARNNING_AUDIO_SWITCH:
		value = state ? WARNNING_SWITCH_SET    : WARNNING_SWITCH_CLEAR;
		break;
	case APP_SETTING_CLOUD_AUDIO_SWITCH:
		value = state ? CLOUD_SWITCH_SET       : CLOUD_SWITCH_CLEAR;
		break;
	case APP_SETTING_MASTER_AUDIO_SWITCH:
		value = state ? MASTER_SWITCH_SET      : MASTER_SWITCH_CLEAR;
		break;
	default:
		return -1;
	}

	return app_setting_set_int(APP_SETTING_AUDIO_SECTION, APP_SETTING_AUDIO_SWITCHES, value);
}

int app_setting_get_audio_switches(uint8_t type)
{
	if (type < APP_SETTING_PAPER_BEEP_AUDIO_CHANGE || type > APP_SETTING_MASTER_AUDIO_SWITCH)
		return OFF;
	switch (type)
	{
		case 0:  return PAPER_BEEP_CHANGE_GET;
		case 1:  return PAPER_BEEP_SWITCH_GET;
		case 2:  return PAPER_TAKEN_SWITCH_GET;
		case 3:  return NOTICE_SWITCH_GET;
		case 4:  return WARNNING_SWITCH_GET;
		case 5:  return CLOUD_SWITCH_GET;
		case 6:  return MASTER_SWITCH_GET;
		default: return OFF;
	}
}