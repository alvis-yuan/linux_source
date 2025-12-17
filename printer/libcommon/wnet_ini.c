#include <libcommon.h>

#define WNET_INI_FILE "/tmp/wnet.ini"

char *wnet_ini_get(const char *key,char *value,int value_len)
{
	if( value && value_len>0 ) {
		memset(value,0,value_len);
		ini_get_value(WNET_INI_FILE,key,value,value_len);
	}
	return value;
}

int wnet_ini_get_int(const char *key,int def_value)
{
	char buf[64]={};
	if( wnet_ini_get(key,buf,sizeof(buf)) && strlen(buf) )
		return atoi(buf);
	return def_value;
}

int wnet_ini_set(const char *key,const char *value)
{
	return ini_set_value(WNET_INI_FILE,key,value);
}

int wnet_ini_set_int(const char *key,int value)
{
	char buf[64];
	snprintf(buf,sizeof(buf),"%d",value);
	return wnet_ini_set(key,buf);
}

