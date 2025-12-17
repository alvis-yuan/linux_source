#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>

#include <libcommon.h>

#include <global_var.h>
#include "hwinfo.h"

#define BOOT_NODE         "/sys/class/sunmi/base/BootloaderVersion"
#define FW_NODE         "/sys/class/sunmi/base/FirmwareVersion"
#define HW_NODE         "/sys/class/sunmi/base/HardwareVersion"
#define BOOT_MODE_PATH	"/sys/class/sunmi/base/boot_mode"
#define HW_TAG_NODE     "/sys/class/sunmi/base/gpio_hw_tag"

__attribute__((constructor)) void ignore_sigpipe()
{
	signal(SIGPIPE,SIG_IGN);
}

int HwinfoRead(char buf[HWINFO_WHOLE_LENS])
{
	int i,fd,len;

	for(i=0;i<30;i++) {
		fd = open(HWINFO_KERNEL_DEV,O_RDONLY|O_CLOEXEC);
		if( fd>=0 ) break;
		usleep(20*1000);
	}
	if( fd<0 ) {
		LogError("open(%s) fail(%d),%s",HWINFO_KERNEL_DEV,errno,strerror(errno));
		exit(1);
	}

	len = xread(fd,buf,HWINFO_WHOLE_LENS);
	close(fd);
	if( len==HWINFO_WHOLE_LENS ) {
		unsigned int crc = util_crc32(buf,HWINFO_CRC_DATALEN);
		HWINFO_BlockData_t *blk = _BlockN(buf,HWINFO_BLOCK_CRC);
		if( crc == strtoul(blk->content,NULL,16) ) {
			//LogDbg("%s crc OK!",HWINFO_KERNEL_DEV);
			return 0;
		} else {
			LogWarn("%s crc fail!",HWINFO_KERNEL_DEV);
		}
	} else {
		LogWarn("read fail");
	}
	return -1;
}

int hwinfo_get_cfg(const char *key,const char *cfgdata,char *value,int vallen)
{
	int len = strlen(key);
	if( len<=0 ) return -1;
	char *start = strstr(cfgdata,key);
	while( start ) {
		start += len;
		if( *start == '=' ) {
			start += 1;
			break;
		}
		start = strstr(start,key);
	}
	if( start ) {
		const char *end = strstr(start,"\r\n");
		if( !end ) end = cfgdata+strlen(cfgdata);
		snprintf(value,vallen,"%.*s",end-start,start);
		return 0;
	}
	return -1;
}

static void format_mac_str(const char oldmac[12],char mac[18])
{
	int i=0;
	char *p = mac;
	for(i=0;i<6;i++) {
		if( i ) *p++ = ':';
		*p++ = oldmac[2*i];
		*p++ = oldmac[2*i+1];
	}
	*p = 0;
}

static int _sync_hwinfo_params(const void *buf,int len,global_var_t *var)
{
#define NET_CONFIGURATION(var, x, y, z) do {var->lan_exist  = x; var->wifi_exist = y; var->wnet_exist = z;} while(0)

	HWINFO_BlockData_t *blk;

	if( len != HWINFO_WHOLE_LENS ) return -1;
	unsigned int crc = util_crc32(buf,HWINFO_CRC_DATALEN);
	blk = _BlockN(buf,HWINFO_BLOCK_CRC);
	if( crc != strtoul(blk->content,NULL,16) ) {
		LogWarn("crc fail!");
		return -1;
	}

	blk = _BlockN(buf,HWINFO_BLOCK_SN);
	snprintf(var->sn,sizeof(var->sn),"%.*s",atoi(blk->lens),blk->content);
	blk = _BlockN(buf,HWINFO_BLOCK_PROJECT);
	snprintf(var->project,sizeof(var->project),"%.*s",atoi(blk->lens),blk->content);
	if( !strcmp(var->project,"NT211") )
		var->prt_dots_per_line = 384;
	else
		var->prt_dots_per_line = 576;
	blk = _BlockN(buf,HWINFO_BLOCK_MAC);
	if( atoi(blk->lens)>=12 ) format_mac_str(blk->content,var->wifi_mac);
	blk = _BlockN(buf,HWINFO_BLOCK_BT_MAC);
	if( atoi(blk->lens)>=12 ) format_mac_str(blk->content,var->bt_mac);
	blk = _BlockN(buf,HWINFO_BLOCK_LAN_MAC);
	if( atoi(blk->lens)>=12 ) format_mac_str(blk->content,var->lan_mac);
	blk = _BlockN(buf,HWINFO_BLOCK_MQTT_PWD);
	snprintf(var->broker_pwd,sizeof(var->broker_pwd),"%.*s",atoi(blk->lens),blk->content);
	blk = _BlockN(buf,HWINFO_BLOCK_MQTT_ADDR);
	snprintf(var->broker_addr,sizeof(var->broker_addr),"%.*s",atoi(blk->lens),blk->content);
	blk = _BlockN(buf,HWINFO_BLOCK_MQTT_PORT);
	snprintf(var->broker_port,sizeof(var->broker_port),"%.*s",atoi(blk->lens),blk->content);
	blk = _BlockN(buf,HWINFO_BLOCK_MQTT_USERNAME);
	snprintf(var->broker_uname,sizeof(var->broker_uname),"%.*s",atoi(blk->lens),blk->content);
	blk = _BlockN(buf,HWINFO_BLOCK_HTTP_URL);
	snprintf(var->cloud_url,sizeof(var->cloud_url),"%.*s",atoi(blk->lens),blk->content);
	blk = _BlockN(buf,HWINFO_BLOCK_CLOUD_TOKEN);
	snprintf(var->cloud_token,sizeof(var->cloud_token),"%.*s",atoi(blk->lens),blk->content);
	blk = _BlockN(buf,HWINFO_BLOCK_DISABLE_CONSOLE);
	if( atoi(blk->lens) == strlen(CONSOLE_RX_DISABLE_TAG) && !strncmp(blk->content,CONSOLE_RX_DISABLE_TAG,strlen(CONSOLE_RX_DISABLE_TAG))){
		var->disable_console_rx = 1;
	} else{
		var->disable_console_rx = 0;
	}
	blk = _BlockN(buf,HWINFO_BLOCK_BOOT_SELECT);
	if( atoi(blk->lens)>0 )	var->boot_select = blk->content[0];
	HWINFO_CfgData_t *cfg = _CfgData(buf);

	char data_report[32] = {};
	if( atoi(cfg->lens)>0 )	hwinfo_get_cfg("DATA_REPORT",cfg->content,data_report,sizeof(data_report));
	if( !strcmp(data_report, "ON") ) var->data_report = 1;
	else var->data_report = 0;

	if( atoi(cfg->lens)>0 )	hwinfo_get_cfg("MODEL",cfg->content,var->model,sizeof(var->model));
	if( !strlen(var->model) )
		snprintf(var->model,sizeof(var->model),"%s",var->project);
	/*reserved for V0.01 config*/
	if( !strcmp(var->model,"NT310") ) {
		NET_CONFIGURATION(var,1,0,0);
	} else if( !strcmp(var->model,"NT311") ) {
		NET_CONFIGURATION(var,1,1,0);
	} else if( !strcmp(var->model,"NT312") || !strcmp(var->model,"NT312_D")  ) {
		NET_CONFIGURATION(var,1,1,1);
	} else if( !strcmp(var->model,"NT313") ) {
		NET_CONFIGURATION(var,1,0,1);
	} else if( !strcmp(var->model,"NT211") || !strcmp(var->model,"NT211_S") || !strcmp(var->model,"NT213") || !strcmp(var->model,"NT215")
		|| !strcmp(var->model,"NT217") || !strcmp(var->model,"NT219") ) {
		NET_CONFIGURATION(var,0,1,1);
	} else if( !strcmp(var->model,"NT212") || !strcmp(var->model,"NT212_S") || !strcmp(var->model,"NT214") || !strcmp(var->model,"NT216") ) {
		NET_CONFIGURATION(var,0,1,0);
	} else if( !strcmp(var->model,"NT218") || !strcmp(var->model,"NT21A") ){
		NET_CONFIGURATION(var,0,0,1);
	}else {
		NET_CONFIGURATION(var,0,1,0);
	}

	/*V0.02 config would reset the relevant paramters*/
	if( atoi(cfg->lens)>0 ) {
		hwinfo_get_cfg("WNET",cfg->content,var->wnet_md,sizeof(var->wnet_md));
		hwinfo_get_cfg("WIFI",cfg->content,var->wifi_md,sizeof(var->wifi_md));
		hwinfo_get_cfg("LAN",cfg->content,var->lan_md,sizeof(var->lan_md));
		hwinfo_get_cfg("PRINTER",cfg->content,var->prt_md,sizeof(var->prt_md));
		LogInfo("WNET:[%s],WIFI:[%s],LAN:[%s],PRINTER:[%s]",var->wnet_md,var->wifi_md,var->lan_md,var->prt_md);
		if( !strcasecmp(var->wnet_md,"none") )
			var->wnet_exist = 0;
		else if( var->wnet_md[0] )
			var->wnet_exist = 1;
		if( !strcasecmp(var->wifi_md,"none") )
			var->wifi_exist = 0;
		else if( var->wifi_md[0] )
			var->wifi_exist = 1;
		if( !strcasecmp(var->lan_md,"none") )
			var->lan_exist = 0;
		else if( var->lan_md[0] )
			var->lan_exist = 1;
	}
	return 0;
#undef NET_CONFIGURATION
}

static int _FileRead(const char *fname,void *buf,int len)
{
	FILE *fp = fopen(fname,"rb");

	if( buf ) memset(buf,0,len);
	if( !fp ) return -1;
	int ret = fread(buf,1,len,fp);
	fclose(fp);
	return ret;
}

static void _global_var_init(global_var_t *var)
{
	char buf[HWINFO_WHOLE_LENS] = {},tmp[32]={};

	_FileRead(BOOT_MODE_PATH,tmp,sizeof(tmp));
	var->boot_mode = atoi(tmp);
	_FileRead(FW_NODE,var->fw_ver,sizeof(var->fw_ver));
	_FileRead(HW_NODE,var->hw_ver,sizeof(var->hw_ver));
	_FileRead(HW_TAG_NODE,var->hw_tag,sizeof(var->hw_tag));
	LogWarn("bootmode=%d,fw_ver=%s,hw_ver=%s,hw_tag=%s",var->boot_mode,var->fw_ver,var->hw_ver,var->hw_tag);
	_FileRead(BOOT_NODE,var->boot_ver,sizeof(var->boot_ver));
	if (0 == strlen(var->boot_ver)) {
		snprintf(var->boot_ver, sizeof(var->boot_ver), UBOOT_VERSION);
	}
	if( HwinfoRead(buf)<0 ) {
		LogError("Read hwinfo fail!");
		exit(1);
	}
	_sync_hwinfo_params(buf,sizeof(buf),var);
	if( !var->sn[0] ) {
		if( var->boot_mode == BOOT_MODE_NORMAL )
			var->boot_mode = BOOT_MODE_FACTORY;
		LogWarn("SN is empty,boot_mode=%d",var->boot_mode);
	}
}

void sync_global_var(const void *buf,int len)
{
	int fd = shm_open(GLOBAL_VAR_CFG_FILE,O_RDWR,0666);
	if( fd<0 ) {
		LogError("open %s fail(%d),%s",GLOBAL_VAR_CFG_FILE,errno,strerror(errno));
		return;
	}
	global_var_t *var = (global_var_t *)mmap(NULL,sizeof(global_var_t),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
	close(fd);
	_sync_hwinfo_params(buf,len,var);
	msync(var,sizeof(global_var_t),MS_SYNC);
	munmap(var,sizeof(global_var_t));
}

global_var_t *sys_global_var()
{
	static global_var_t *g_glb_var = NULL;
	if( g_glb_var ) return g_glb_var;
	int fd = shm_open(GLOBAL_VAR_CFG_FILE,O_RDWR|O_CREAT,0666);
	if( fd<0 ) {
		LogError("open %s fail(%d),%s",GLOBAL_VAR_CFG_FILE,errno,strerror(errno));
		exit(1);
	}
	ftruncate(fd,sizeof(global_var_t));
	g_glb_var = (global_var_t *)mmap(NULL,sizeof(global_var_t),PROT_READ,MAP_SHARED,fd,0);
	if( g_glb_var->magic != GLOBAL_CFG_MAGIC ) {
		LogDbg("init share memery %s", GLOBAL_VAR_CFG_FILE);
		global_var_t *tmp = (global_var_t *)mmap(NULL,sizeof(global_var_t),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
		memset(tmp,0,sizeof(*tmp));
		tmp->magic = GLOBAL_CFG_MAGIC;
		_global_var_init(tmp);
		munmap(tmp,sizeof(global_var_t));
	}
	close(fd);
	return g_glb_var;
}

void sys_save_lbs(char *lbs)
{
	char *p = strrchr(lbs,':');
	p = p?p+1:lbs;
	while( isspace(*p) ) p++;
	char *p1 = strchr(p,','); //第一个','
	if( p1 ) {
		p1 = strchr(p1+1,',');
		if( p1 ) *p1 =0; //第二个','结束
	}
	p1 = p;
	strtok(p1,"\r\n");
	wnet_ini_set(WNET_KEY_LBS,p);
}

void sys_get_lbs(char *lng,char *lat)
{
	char buf[256]={};
	wnet_ini_get(WNET_KEY_LBS,buf,sizeof(buf));
	char *p =strchr(buf,',');
	if( p )
		*p++ = 0;
	else
		p = "";
	strcpy(lng,buf);
	strcpy(lat,p);
}

bool sys_lbs_exist()
{
	char buf[256]={};
	wnet_ini_get(WNET_KEY_LBS,buf,sizeof(buf));
	return !!buf[0];
}

