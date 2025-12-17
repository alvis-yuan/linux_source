#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <errno.h>
#include <sys/mman.h>
#include <ctype.h>

#include <libcommon.h>

static int net_api_send(short cmd,const void *data,int len)
{
	if( len<0 ) len = 0;
	if( len>MQ_NET_MAX_SIZE-sizeof(net_mq_msg_t) ) {
		LogError("data len(%d) must less than %d",len,MQ_NET_MAX_SIZE-sizeof(net_mq_msg_t));
		return -1;
	}
	mqd_t mq = mq_open(MQ_NET_SERVICE,O_WRONLY);
	if( mq<0 ) return -1;
	char buf[MQ_NET_MAX_SIZE]={};
	net_mq_msg_t *msg = (net_mq_msg_t *)buf;
	msg->cmd = cmd;
	if( data && len>0 )
		memcpy(msg->data,data,len);
	if( mq_send(mq,buf,len+sizeof(net_mq_msg_t),0)<0 ) {
		LogError("mq_send fail(%d),%s",errno,strerror(errno));
		mq_close(mq);
		return -1;
	}
	mq_close(mq);
	return 0;
}


#define state_match(a,b) (!strncmp(a,b,strlen(b)))
int wpa_status(void)
{
	int st = -1;
    char buf[128] = {};
    FILE *fp = popen("wpa_cli -i wlan0 status|grep wpa_state","r");

	if( !fp ) return st;

	if( fgets(buf,sizeof(buf),fp) ) {
		char *p = buf+strlen("wpa_state=");
		if( state_match(p,"DISCONNECTED") )
			st = WPA_ST_DISCONNECTED;
		else if( state_match(p,"INTERFACE_DISABLED") )
			st = WPA_ST_IFACE_DISABLED;
		else if( state_match(p,"INACTIVE") )
			st = WPA_ST_INACTIVE;
		else if( state_match(p,"SCANNING") )
			st = WPA_ST_SCANNING;
		else if( state_match(p,"AUTHENTICATING") )
			st = WPA_ST_AUTHENTICATING;
		else if( state_match(p,"ASSOCIATING") )
			st = WPA_ST_ASSOCIATING;
		else if( state_match(p,"ASSOCIATED") )
			st = WPA_ST_ASSOCIATED;
		else if( state_match(p,"4WAY_HANDSHAKE") )
			st = WPA_ST_4WAY_HANDSHAKE;
		else if( state_match(p,"GROUP_HANDSHAKE") )
			st = WPA_ST_GROUP_HANDSHAKE;
		else if( state_match(p,"COMPLETED") )
			st = WPA_ST_COMPLETED;
	}
	pclose(fp);
    return st;
}

const char *net_ifname(int net_type)
{
	if( net_type == NET_WIFI )
		return "wlan0";
	else if( net_type == NET_WNET )
		return "ppp0";
	else if( net_type == NET_LAN )
		return "eth0";
	else
		return NULL;
}

void NetStartTestMode()
{
	if( net_api_send(NET_CMD_WNET_FAC_TEST,NULL,0)<0 ){
		LogWarn("NetServer maybe not run!");
	}
}

void NetWnetStartFota(int mode)
{
	if( net_api_send(NET_CMD_WNET_FOTA,&mode,sizeof(mode))<0 ){
		LogWarn("NetServer maybe not run!");
	}
}

void NetWnetGetFota()
{
	int mode;
	if (net_api_send(NET_CMD_WNET_GET_FOTA, &mode, sizeof(mode)) < 0) {
		LogWarn("NetServer maybe not run!");
	}
}

void NetWnetStartHttpsFota(char *url)
{
    if (net_api_send(NET_CMD_WNET_HTTPS_FOTA, url, strlen(url)+1) < 0) {
        LogWarn("NetServer maybe not run!");
    }
}

void net_wnet_start()
{
	if( net_api_send(NET_CMD_WNET_START,NULL,0)<0 ){
		LogWarn("NetServer maybe not run!");
	}
}

void net_wnet_stop()
{
	if( net_api_send(NET_CMD_WNET_START,NULL,0)<0 ){
		LogWarn("NetServer maybe not run!");
	}
}

void net_wnet_restart()
{
	if( net_api_send(NET_CMD_WNET_RESTART,NULL,0)<0 ){
		LogWarn("NetServer maybe not run!");
	}
}

#define _cToX(c) (((c) >= '0' && (c) <= '9') ? ((c) - '0') : (((c) >= 'a' && (c) <= 'f') ? ((c) - 'a' + 10) : (((c) >= 'A' && (c) <= 'F') ? ((c) - 'A' + 10) : 0x10)))
static void _ConvToUTF8(char *dst,const char *src) 
{ 
	while( *src ) {
		if( !strncasecmp(src,"\\x",2) && isxdigit(src[2]) && isxdigit(src[3]) ) {
			*dst++ = ((_cToX(src[2])<<4)|_cToX(src[3]));
			src += 4;
		} else {
			*dst++ = *src++;
		}
	} 
	*dst = 0;
}

ST_WIFI_SCAN_RESULT *net_wifi_scan(void)
{
	char buf[256];
	int invalid_ssid_cnt = 0;

	if(access("/tmp/wpa_supplicant/wlan0",F_OK)){
		LogInfo("start wifi and waiting for complete");
		char ssid[32]={},pwd[32]={};
	    util_random_str(ssid,sizeof(ssid));
	    util_random_str(pwd,sizeof(pwd));
	    net_wifi_start(ssid,pwd);
		while(net_wifi_is_stopped())
			util_msleep(500);
		LogInfo("wifi start ok");
		sleep(5);
	}

	/* 等待？毫秒后，获取扫描wifi结果
	原：200ms (扫描) * 14 (通道数) + 200ms (间隔) * 13 (通道间间隔) = 5400 ms (科儿通)
	200ms (扫描) * 14 (通道数) +  20ms (间隔) * 13 (通道间间隔) = 3060 ms
	200ms (扫描) * 14 (通道数) +   5ms (间隔) * 13 (通道间间隔) = 2865 ms
	200ms (扫描) * 14 (通道数) +   2ms (间隔) * 13 (通道间间隔) = 2826 ms
	预留时间: C >> wpa_cli >> kernel >> wpa_cli >> C
	*/
	const int wait_wifi_scan_time = 3060 + 500 /*预留时间*/;
	LogInfo("starting wifi scan");
	system("wpa_cli -i wlan0 scan");
	util_msleep(wait_wifi_scan_time);
	FILE *fp = popen("wpa_cli -i wlan0 scan_results","r");
	if (!fp) {
		LogInfo("not found wifi scanned result");
		return NULL;
	}
	LogInfo("wifi scan ok");
	ST_WIFI_SCAN_RESULT *result = (ST_WIFI_SCAN_RESULT *)malloc(sizeof(ST_WIFI_SCAN_RESULT));
	if (!result) {
		goto out;
	}
	result->count = 0;
	char *p = fgets(buf,sizeof(buf),fp); //忽略第一行
	while( p && fgets(buf,sizeof(buf),fp) ) {
		char bssid[18]={},flags[64]={},ssid[256]={};
		int freq,rssi;
		int ret = sscanf(buf,"%17s\t%d\t%d\t%s%*c%255[^\n]",bssid,&freq,&rssi,flags,ssid);
		if (ret != 5) { //ssid可能为空
			invalid_ssid_cnt++;
			continue;
		}
		int index = result->count++;
		result = (ST_WIFI_SCAN_RESULT *)realloc(result,sizeof(ST_WIFI_SCAN_RESULT)+result->count*sizeof(ST_WIFI_SCAN_AP));
		if (!result) {
			goto out;
		}
		memset(&result->list[index],0,sizeof(ST_WIFI_SCAN_AP));
		strcpy(result->list[index].bssid,bssid);
		if( strstr(flags,"WEP") ) result->list[index].authMode |= WIFI_AUTH_WEP;
		if( strstr(flags,"WPA-") ) result->list[index].authMode |= WIFI_AUTH_WPA_PSK;
		if( strstr(flags,"WPA2-") ) result->list[index].authMode |= WIFI_AUTH_WPA2_PSK;
		_ConvToUTF8(result->list[index].essid,ssid);
		result->list[index].rssi = rssi;
		result->list[index].freq=freq;
		result->list[index].stSize = sizeof(ST_WIFI_SCAN_AP);
	}
	LogInfo("[scan ssid] all:%d, discard:%d, valid:%d", result->count + invalid_ssid_cnt, invalid_ssid_cnt, result->count);
out:
	if (fp) {
		pclose(fp);
	}
	return result;
}


int NetGetInfo(char *interface,void *netInfo)
{
	return -1;
}

int NetGetCurRoute(void)
{
	char buf[256]={};
	int index=NET_NONE;

	FILE *f = popen("iproute|grep default","r");
	if(f==NULL)
	{
		LogError("iproute  error:%d\n",errno);
		return NET_NONE;
	}

	if( fgets(buf,sizeof(buf),f) ) { // get first line only
		char *p = strstr(buf,"dev ");
		if( p ) {
			int i;
			p += strlen("dev ");
			char *p1 = strchr(p,' ');
			if( p1 ) *p1 = 0;
			for(i=0;i<NET_MAX;i++) {
				const char *ifname = net_ifname(i);
				if( ifname && !strcmp(ifname,p) ) {
					index = i;
					break;
				}
			}
		}
	}

	pclose(f);
	return index;
}


#define SIOCETHTOOL	  0x8946		/* Ethtool interface		*/
#define ETHTOOL_GLINK		0x0000000a
struct ethtool_value {
	unsigned cmd;
	unsigned data;
};
bool net_lan_is_linkup(void)
{
	struct ethtool_value edata ={.cmd=ETHTOOL_GLINK};
	struct ifreq ifr={
		.ifr_name="eth0",
		.ifr_data=(caddr_t)&edata,
	};

	int sock = socket(AF_INET,SOCK_DGRAM,0);
	int ret = ioctl(sock,SIOCETHTOOL,&ifr);
	close(sock);
	if( ret>=0 && edata.data ) return 1;
	return 0;
}

bool net_lan_is_connected()
{
	unsigned ip=0;
	util_get_ip(net_ifname(NET_LAN),&ip,0);
	if( ip==0 || ip==(unsigned)-1 ) return false;
	return true;
}

static void wifi_save_wpa_conf(const char *ssid,int auth,const char *key)
{
	const char *fname = "/tmp/wpa_supplicant.conf";
	FILE *fp = fopen(fname,"w");
    fprintf(fp,	"update_config=1\nctrl_interface=/var/run/wpa_supplicant\n"
    			"ctrl_interface_group=0\n\n"
    			"network={\n"
    			"ssid=\"%s\"\n"
    			"scan_ssid=1\n",ssid);

	if( auth==WIFI_AUTH_NONE ) {
        fprintf(fp,"key_mgmt=NONE\n");
	} else if( auth==WIFI_AUTH_WEP ) {
        fprintf(fp,"key_mgmt=NONE\n");
        fprintf(fp,"wep_key0=");
		int i,len = strlen(key);
		for(i=0;i<len;i++)
			fprintf(fp,"%02X",key[i]);
		fprintf(fp,"\nwep_tx_keyidx=0\n");
	} else {
        fprintf(fp,"key_mgmt=WPA-PSK\npsk=\"%s\"\n",key);
	}
	
    fprintf(fp,"bssid=\n");
    fprintf(fp,"priority=1\n");
    fprintf(fp,"}");
    fclose(fp);
}

void net_wifi_load()
{
	if( !sys_global_var()->wifi_exist ) return;

	cJSON *root = net_load_config(NET_WIFI);
	if( !root ) {
		LogError("WIFI not configured!");
		return;
	}
	cJSON *data = cJSON_GetObjectItem(root,"D");
	if( data ) {
		char *ssid = cJSON_GetValueString(data,"S","");
		char *type = cJSON_GetValueString(data,"T","");
		char *key  = cJSON_GetValueString(data,"P","");
		if( strlen(ssid) && (!strcmp(type,"N") || strlen(key)) ) {
			int auth = WIFI_AUTH_NONE;
			if( !strcmp(type,"WEP") )
				auth = WIFI_AUTH_WEP;
			else if( !strcmp(type,"WPA") )
				auth = WIFI_AUTH_WPA_WPA2_PSK;
			wifi_save_wpa_conf(ssid,auth,key);
			system("/home/scripts/wifi.sh restart  >/dev/null 2>&1");
			LogInfo("Start WiFi %s ...",ssid);
		} else {
			LogError("WIFI config error,S=\"%s\" T=\"%s\" P=\"%s\"",ssid,type,key);
		}
	} else {
		LogError("No data in WIFI config");
	}
	cJSON_Delete(root);
}

int net_wifi_start(const char *ssid,const char *passwd)
{
	if( !ssid || !strlen(ssid) ) return -1;

	FILE *fp = fopen("/tmp/wpa_supplicant.conf","w");
	if( !fp ) return -1;
	fprintf(fp, "update_config=1\n"
				"ctrl_interface=/var/run/wpa_supplicant\n"
				"ctrl_interface_group=0\n\n"
				"network={\n"
				"ssid=\"%s\"\n",ssid);
	fprintf(fp,"scan_ssid=1\n");
	if( !passwd || !strlen(passwd) )
		fprintf(fp,"key_mgmt=NONE\n");
	else
		fprintf(fp,"key_mgmt=WPA-PSK\npsk=\"%s\"\n",passwd);
	fprintf(fp, "bssid=\n"
				"priority=1\n}\n");
	fclose(fp);

	LogWarn("Start WiFi %s ...",ssid);
	return system("/home/scripts/wifi.sh restart");
}

void net_wifi_stop()
{
	LogWarn("Stop WiFi ...");
	system("/home/scripts/wifi.sh stop >/dev/null  2>&1");
}

bool net_wifi_is_stopped()
{
	return !!access("/tmp/wpa_supplicant/wlan0",F_OK);
}

bool net_wifi_is_connected()
{
	unsigned ip=0;
	if( wpa_status() != WPA_ST_COMPLETED ) return false;
	util_get_ip("wlan0",&ip,0);
	if( ip==0 || ip==(unsigned)-1 ) return false;
	return true;
}

void net_wifi_get_info()
{

}

int net_wifi_get_rssi(int *rssi)
{
	*rssi = 0;
	FILE *fp = popen("cat /proc/net/wireless|grep wlan0","r");
	if( !fp ) return -1;
	char buf[512]={};
	fgets(buf,sizeof(buf),fp);
	pclose(fp);
	int ret = sscanf(buf," wlan0: %*d %*d. %d.",rssi);
	if( ret<1 ) return -1;
	return 0;
}

void net_update_config(int net_type,const char *param)
{
	char *setting_name = NULL;
	if( net_type == NET_WIFI )
		setting_name = "WIFI";
	else if( net_type == NET_WNET )
		setting_name = "WNET";
	else if( net_type == NET_LAN )
		setting_name = "LAN";
	else
		return;
	cJSON *root = util_file2cJSON(CONFIG_NODE);
	if( root ) {
		int i,count = cJSON_GetArraySize(root);
		for(i=0;i<count;i++) {
			cJSON *it = cJSON_GetArrayItem(root,i);
			char *name = cJSON_GetValueString(it,"SETTING", NULL);
			if( name && !strcmp(name,setting_name) ) {
				cJSON_DeleteItemFromArray(root,i);
				break;
			}
		}
	} else {
		root = cJSON_CreateArray();
	}
	if( !root ) return;
	if( param && param[0] ) {
		cJSON *setting = cJSON_Parse(param);
		if( !setting ) {
			LogError("cJSON_Parse fail,param=%s",param);
			goto out;
		}
		cJSON_AddItemToArray(root,setting);
	}
	FILE *fp = fopen(CONFIG_NODE,"w");
	if( fp ) {
		char *str = cJSON_PrintUnformatted(root);
		fputs(str,fp);
		fclose(fp);
		free(str);
	}
	if( net_type == NET_WIFI ) {
		if( param && param[0] )
			net_wifi_load();
		else
			net_wifi_stop();
	} else if( net_type == NET_WNET ) {
		net_wnet_restart();
	}
out:
	cJSON_Delete(root);
}

cJSON *net_load_config(int net_type)
{
	cJSON *config = NULL;
	char *setting_name = NULL;
	if( net_type == NET_WIFI )
		setting_name = "WIFI";
	else if( net_type == NET_WNET )
		setting_name = "WNET";
	else if( net_type == NET_LAN )
		setting_name = "LAN";
	else
		return config;
	cJSON *root = util_file2cJSON(CONFIG_NODE);
	if( !root ) return config;
	int i,count = cJSON_GetArraySize(root);
	for(i=0;i<count;i++) {
		cJSON *it = cJSON_GetArrayItem(root,i);
		char *name = cJSON_GetValueString(it,"SETTING", NULL);
		if( name && !strcmp(name,setting_name) ) {
			cJSON_DetachItemFromArray(root, i);
			config = it;
			break;
		}
	}
	cJSON_Delete(root);
	return config;
}

int net_wnet_get_info(ST_WNET_INFO *info)
{
    memset(info,0,sizeof(ST_WNET_INFO));

    if( util_get_str_ip("ppp0",info->ipAddr,info->submask)<0 )
		return -1;
	FILE *fp = fopen("/tmp/wnet_resolv.conf","r");
	if( fp ) {
		char buf[128],*p;
		while( (p=fgets(buf,sizeof(buf),fp))!=NULL ) {
			if( sscanf(buf,"nameserver %[0-9.]",info->dns1) == 1 )
				break;
		}
		if( p ) {
			while( (p=fgets(buf,sizeof(buf),fp))!=NULL ) {
				if( sscanf(buf,"nameserver %[0-9.]",info->dns2) == 1 )
					break;
			}
		}
		fclose(fp);
	}

	info->rssi = wnet_ini_get_int(WNET_KEY_RSSI,0);

    return 0;

}

bool net_wnet_is_connected()
{
	unsigned ip;
	if( (access("/sys/class/net/ppp0",F_OK)==0) && util_get_ip("ppp0",&ip,NULL)==0 ) return true;
	return false;
}

//local area net print server setting
#define LOCAL_PRT_SRV_INI_FILE  "/data/SYS/lsrv_cfg.ini"
int net_LPrtSrv_ini_set(const char *section,const char *key,const char *value)
{
	return ini_set_value_with_section(LOCAL_PRT_SRV_INI_FILE,section,key,value);
}

int net_LPrtSrv_ini_get(const char *section,const char *key,char *value,int len)
{
	char *p = ini_get_value_with_section(LOCAL_PRT_SRV_INI_FILE,section,key,value,len);
	return p?0:-1;
}

static bool _set_static_dns_info(const char *ifname, const char *dns)
{
	FILE *fp;
	FILE *tmp_fp;
	char tmp_str[32] = { };
	char buf[128] = { };
    bool change = false;
    const char *tmp_file = "/dev/nameserver.tmp";
    char dns_file[32] = {};
    bool add = dns ? true : false;

    snprintf(dns_file, sizeof(dns_file), "/dev/nameserver.%s", ifname);

    tmp_fp = fopen(tmp_file, "w");
	if (tmp_fp == NULL) {
		LogError("open file failed");
		return false;
	}

    fp = fopen(dns_file, "r");
    if (fp == NULL) {
        if (!add) {
            LogError("delete static dns but %s file inexist", dns_file);
            fclose(tmp_fp);
            return false;
        } else {
            LogInfo("%s file inexist, create new file", dns_file);
        }
    } else {
        // 先删除静态ip对应的DNS信息
        snprintf(tmp_str, sizeof(tmp_str), "# %s static", ifname);
        while (fgets(buf, sizeof(buf), fp)) {
            if (strstr(buf, tmp_str)) {
                LogInfo("remove old static dns: %s", buf);
                change = true;
                continue;
            }
            LogInfo("write %s to %s", buf, tmp_file);
            fwrite(buf, strlen(buf), 1, tmp_fp);
            memset(buf, 0, sizeof(buf));
        } 
    }

    if (add) { //添加静态ip对应的DNS信息
        snprintf(buf, sizeof(buf), "nameserver %s # %s static\n", dns, ifname);
        fwrite(buf, strlen(buf), 1, tmp_fp);
        LogInfo("add static dns: %s", buf);
        change = true;
    }

	fclose(tmp_fp);
    if (fp) {
        fclose(fp);
    }
	if (rename(tmp_file, dns_file)) {
		LogError("rename failed %d:%s", errno, strerror(errno));
		return false;
	}

    return change;
}

static bool is_valid_ipv4(const char *str)
{
    int num = 0;
    int dots = 0;

    if (!str || strlen(str) == 0) {
        return false;
    }

    int len = strlen(str);
    
    // IPv4 地址长度范围：7.0.0.1(7) 到 255.255.255.255(15)
    if (len < 7 || len > 15) {
        return false;
    }
    
    // 临时变量用于遍历字符串
    char temp[256];
    snprintf(temp, sizeof(temp), "%s", str);
    
    char *token = strtok(temp, ".");
    while (token != NULL) {
        // 每个部分最多3个数字
        if (strlen(token) > 3) {
            return false;
        }
        
        // 检查是否全是数字
        for (int i = 0; token[i] != '\0'; i++) {
            if (!isdigit(token[i])) {
                return false;
            }
        }
        
        // 转换为数字并检查范围
        num = atoi(token);
        if (num < 0 || num > 255) {
            return false;
        }
        
        // 检查前导零（如 001.002.003.004 不合法）
        if (strlen(token) > 1 && token[0] == '0') {
            return false;
        }
        
        dots++;
        token = strtok(NULL, ".");
    }
    
    // IPv4 必须有4个部分
    return (dots == 4);
}


/**
 * @brief 更新静态的网络配置
 * 1. 下发的ip,sm,gw,dns均为合法的ipv4地址字符串
 * 2. 如果4个字段均为"0.0.0.0"，则表示清空之前的配置
 * 3. 如果其中有一个字段为"0.0.0.0"，则表示配置不完整，返回错误
 * 
 * @param net_type 网络类型
 * @param ip 静态ip地址
 * @param sm 子网掩码
 * @param gw 网关地址
 * @param dns DNS服务器地址
 * @return int 0成功，-1失败
 */
int update_LPrtSrv_config(int net_type,const char *ip,const char *sm,const char *gw,const char *dns)
{
	const char *ifname = net_ifname(net_type);
	if( !ifname ) {
        LogError("net_ifname(%d) failed", net_type);
        return -1;
    }

	char cmd[128], str[64];
	char old_ip[16] = {};
	char old_sm[16] = {};
	char old_gw[16] = {};
	char old_dns[16] = {};
    bool need_update_dns = false;
    bool need_reset_static_info = false;

    // 检查参数合法性
    if (!is_valid_ipv4(ip) || !is_valid_ipv4(sm) || !is_valid_ipv4(gw)) {
        LogError("invalid ip %s or sm %s or gw %s", ip, sm, gw);
        return -1;
    }

	/* Get the current setting */
	net_LPrtSrv_ini_get(ifname,"ip",old_ip,sizeof(old_ip));
	net_LPrtSrv_ini_get(ifname,"netmask",old_sm,sizeof(old_sm));
	net_LPrtSrv_ini_get(ifname,"gateway",old_gw,sizeof(old_gw));
	net_LPrtSrv_ini_get(ifname,"dns",old_dns,sizeof(old_dns));
    LogInfo("old %s config: ip %s, sm %s, gw %s, dns %s", ifname, old_ip, old_sm, old_gw, old_dns);

    // 如果4个字段都为0.0.0.0，则表示清空之前的配置
    if (!strcmp(ip, "0.0.0.0") && !strcmp(sm, "0.0.0.0")
        && !strcmp(gw, "0.0.0.0") && !strcmp(dns, "0.0.0.0")) {
        // 之前有配置静态ip，则需要清空静态ip
        if (strlen(old_ip) > 0 || strlen(old_sm) > 0
            || strlen(old_gw) > 0 || strlen(old_dns) > 0) {
            need_reset_static_info = true;
            LogError("clear %s static info due to all fields are 0.0.0.0", ifname);
        } else {
            LogError(" no previous config, current config all fields are 0.0.0.0, ignore", ifname);
            return 0;
        }
    } else {
        // 其中有一个配置为0.0.0.0，则表示配置不完整，返回错误
        if (!strcmp(ip, "0.0.0.0") || !strcmp(sm, "0.0.0.0")
            || !strcmp(gw, "0.0.0.0")) {
            LogError("%s invalid param: ip %s or sm %s or gw %s", ifname, ip, sm, gw);
            return -1;
        }
    } 

    // 如果之前存在0.0.0.0, 则清空静态ip对应的DNS信息, 因为历史版本会把0.0.0.0保存为静态ip配置
    if (!strcmp(old_ip, "0.0.0.0") || !strcmp(old_sm, "0.0.0.0")
        || !strcmp(old_gw, "0.0.0.0") || !strcmp(old_dns, "0.0.0.0")) {
		need_reset_static_info = true;
        LogInfo("clear %s static history invalid info: ip %s, sm %s, gw %s, dns %s", ifname, old_ip, old_sm, old_gw, old_dns);
    }

    if (need_reset_static_info) {
        need_update_dns = _set_static_dns_info(ifname, NULL);
        net_LPrtSrv_ini_set(ifname, "ip", "");
        net_LPrtSrv_ini_set(ifname, "netmask", "");
        net_LPrtSrv_ini_set(ifname, "gateway", "");
        net_LPrtSrv_ini_set(ifname, "dns", "");
    }

	/* Get the current setting */
	net_LPrtSrv_ini_get(ifname,"ip",old_ip,sizeof(old_ip));
	net_LPrtSrv_ini_get(ifname,"netmask",old_sm,sizeof(old_sm));
	net_LPrtSrv_ini_get(ifname,"gateway",old_gw,sizeof(old_gw));
	net_LPrtSrv_ini_get(ifname,"dns",old_dns,sizeof(old_dns));

    LogInfo("new %s config: ip %s, sm %s, gw %s, dns %s", ifname,
            ip ? ip : "(null)", sm ? sm : "(null)", gw ? gw : "(null)", dns ? dns : "(null)");

	/* If the new setting is the same as the current setting, do nothing */
	if( (ip && strlen(old_ip) > 0 && !strcmp(ip,old_ip)) &&
		(sm && strlen(old_sm) > 0 && !strcmp(sm,old_sm)) &&
		(gw && strlen(old_gw) > 0 && !strcmp(gw,old_gw)) &&
		(dns && strlen(old_dns) > 0 && !strcmp(dns,old_dns)) ) {
		LogInfo("no need to update %s config",ifname);
		return 0;
	}

	/* Keep the current setting if the corresponding field is NULL */
	if( ip && strcmp(ip,"0.0.0.0"))
		net_LPrtSrv_ini_set(ifname,"ip",ip);
	if( sm && strcmp(sm,"0.0.0.0"))
		net_LPrtSrv_ini_set(ifname,"netmask",sm);
	if( gw && strcmp(gw,"0.0.0.0"))
		net_LPrtSrv_ini_set(ifname,"gateway",gw);
	if( dns && strcmp(dns,"0.0.0.0")) {
        need_update_dns = _set_static_dns_info(ifname, dns);
		net_LPrtSrv_ini_set(ifname,"dns",dns);
	}

	if( net_LPrtSrv_ini_get(ifname,"ip",str,sizeof(str)) != 0 )
		return -1;
    snprintf(cmd,sizeof(cmd),"/home/scripts/lsrv_start.sh %s %d >/dev/null 2>&1",ifname,need_update_dns?1:0);
	LogDbg("cmd=%s",cmd);
	system(cmd);
	return 0;
}

