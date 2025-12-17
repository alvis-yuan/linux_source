#include <libcommon.h>

#define TRADE_INI_FILE "/tmp/trade/trade.ini"

char *trade_ini_get(const char *key,char *value,int value_len)
{
	if( value && value_len>0 ) {
		memset(value,0,value_len);
		ini_get_value(TRADE_INI_FILE, key, value, value_len);
	}
	return value;
}

int trade_ini_get_int(const char *key,int def_value)
{
	char buf[64]={};
	if (trade_ini_get(key, buf, sizeof(buf)) && strlen(buf))
		return atoi(buf);
	return def_value;
}

int trade_ini_set(const char *key, const char *value)
{
	return ini_set_value(TRADE_INI_FILE, key, value);
}

int trade_ini_set_int(const char *key, int value)
{
	char buf[64];
	snprintf(buf,sizeof(buf),"%d",value);
	return trade_ini_set(key, buf);
}

