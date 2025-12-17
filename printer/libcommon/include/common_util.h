#ifndef __COMMON_UTIL_H__
#define __COMMON_UTIL_H__

#include <stdarg.h>
#include <cJSON.h>
#include <mqueue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int util_chk_json_format(const char *jsonStr);
void util_set_default_time(void);
int util_runcmd(const char *f,int line,const char *func,const char *fmt,...);
#define UtilRunCmd(fmt,args...) util_runcmd(__FILE__,__LINE__,__func__,fmt,##args)
void util_msleep(int ms);
void *util_file2buf(const char *fname,int *size);
cJSON *util_file2cJSON(const char *fname);
unsigned int util_crc32(const void *data,int size);
void flush_mq(mqd_t mq);
int util_md5file(const char *file, char md5[33]);
void util_md5sum(const void *buf,int len,char md5[33]);
int util_read2buff(const char *filename, char *buff, int len);
int util_write_node(const char *node, const char *str);
unsigned int SysTick(void);
void SysDelay(unsigned int timeoutMs);
void SysReboot(void);
int SystemWithResult(const char *cmd, char *result, int size);
int FileSize(const char *name);
int FileExistence(const char *name);
void SysBcd2Asc(unsigned char *Asc, unsigned char *Bcd, int inlen);
int util_get_ip(const char *ifname,uint32_t *ip,uint32_t *netmask);
int GetInterfaceAddresses(const char *name, struct in_addr *addr, struct in_addr *netmask);
int util_get_str_ip(const char *ifname,char strIP[18],char strNetmask[18]);
int util_nslookup(char *ip,int ip_len,const char *domain,const char *nameserver);
long long util_get_rx_bytes(const char *ifname);
char *util_get_str_gw(const char *ifname,char *gw,int len);
char *util_get_str_mac(const char *ifname,char *mac,int len);
int util_random_str(char *str,int len);

#define SysGetTickCount SysTick
#define cmdRun system

#define cJSON_GetValueInt(it,name,defValue) ({ \
	int v = defValue; \
	cJSON *o = cJSON_GetObjectItem(it,name); \
	if( o ) { \
		if( o->type == cJSON_Number ) v = o->valueint; \
		else if( o->type == cJSON_False )	v = 0; \
		else if( o->type == cJSON_True ) v = 1; \
	} \
	v; \
})
#define cJSON_GetValueString(it,name,def) ({ \
	char *v = NULL; \
	cJSON *o = cJSON_GetObjectItem(it,name); \
	if( o && o->type==cJSON_String ) v=o->valuestring; \
	(v?v:def); \
})

#endif
