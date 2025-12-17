#include <libcommon.h>

int util_chk_json_format(const char *jsonStr)
{
	int iRet = -1;
	int i;
	int jFlgTotal = 0, jFlgMatch = 0;

	if (strlen(jsonStr) == 0)
		return iRet;

	for (i = 0; i < strlen(jsonStr); i++)
	{
		if (jsonStr[i] == '{')
		{
			jFlgTotal++;
			jFlgMatch++;
		}
		else if (jsonStr[i] == '}')
		{
			jFlgTotal++;
			jFlgMatch--;
		}
	}

	if ((jFlgTotal > 0) && (jFlgMatch == 0))
		iRet = 0;
	else
		LogError("invalid JSON string: %s", jsonStr);

	return iRet;
}

void util_set_default_time(void)
{
	time_t now = time(0);
	if( now < DEFAULT_SYSTEM_TIME ) {
		now = DEFAULT_SYSTEM_TIME;
		stime(&now);
		LogDbg("set default time 2020-01-01 00:00:00 GMT");
	}
}

int util_runcmd(const char *f,int line,const char *func,const char *fmt,...)
{
	char cmd[2048],*pos=cmd;
	va_list arg;
	va_start(arg, fmt);
	pos += vsnprintf(pos,sizeof(cmd)+cmd-pos,fmt,arg);
	va_end(arg);
	FILE *fp = popen(cmd,"r");
	if( !fp ) {
		LogWrite(LOG_LEVEL_WARN,f,line,func,"fail run \"%s\"",cmd);
		return -1;
	} else {
		pclose(fp);
	}
	return 0;
}
void util_msleep(int ms)
{
	if( ms<=0 ) return;
	struct timeval tv = {ms/1000,(ms%1000)*1000};
	while(select(0,0,0,0,&tv)<0 && errno == EINTR);
}

cJSON *util_file2cJSON(const char *fname)
{
	cJSON *root = NULL;
	char *buf = (char *)util_file2buf(fname,0);
	if( buf ) {
		root = cJSON_Parse(buf);
		free(buf);
	}
	return root;
}

void *util_file2buf(const char *fname,int *size)
{
	char *buf = NULL;
	if( size ) *size = 0;
	FILE *fp = fopen(fname,"rb");
	if( !fp ) return NULL;
	fseek(fp,0,SEEK_END);
	int sz = ftell(fp);
	rewind(fp);
	if( sz>0 ) {
		buf = (char *)malloc(sz+1);
		fread(buf,sz,1,fp);
		buf[sz] = 0;
	}
	fclose(fp);
	if( size ) *size = sz;
	return buf;
}

unsigned int util_crc32(const void *data,int size)
{
	unsigned int crc_tbl[256] = {
		0x00000000,0x77073096,0xee0e612c,0x990951ba,0x076dc419,0x706af48f,0xe963a535,0x9e6495a3,
		0x0edb8832,0x79dcb8a4,0xe0d5e91e,0x97d2d988,0x09b64c2b,0x7eb17cbd,0xe7b82d07,0x90bf1d91,
		0x1db71064,0x6ab020f2,0xf3b97148,0x84be41de,0x1adad47d,0x6ddde4eb,0xf4d4b551,0x83d385c7,
		0x136c9856,0x646ba8c0,0xfd62f97a,0x8a65c9ec,0x14015c4f,0x63066cd9,0xfa0f3d63,0x8d080df5,
		0x3b6e20c8,0x4c69105e,0xd56041e4,0xa2677172,0x3c03e4d1,0x4b04d447,0xd20d85fd,0xa50ab56b,
		0x35b5a8fa,0x42b2986c,0xdbbbc9d6,0xacbcf940,0x32d86ce3,0x45df5c75,0xdcd60dcf,0xabd13d59,
		0x26d930ac,0x51de003a,0xc8d75180,0xbfd06116,0x21b4f4b5,0x56b3c423,0xcfba9599,0xb8bda50f,
		0x2802b89e,0x5f058808,0xc60cd9b2,0xb10be924,0x2f6f7c87,0x58684c11,0xc1611dab,0xb6662d3d,
		0x76dc4190,0x01db7106,0x98d220bc,0xefd5102a,0x71b18589,0x06b6b51f,0x9fbfe4a5,0xe8b8d433,
		0x7807c9a2,0x0f00f934,0x9609a88e,0xe10e9818,0x7f6a0dbb,0x086d3d2d,0x91646c97,0xe6635c01,
		0x6b6b51f4,0x1c6c6162,0x856530d8,0xf262004e,0x6c0695ed,0x1b01a57b,0x8208f4c1,0xf50fc457,
		0x65b0d9c6,0x12b7e950,0x8bbeb8ea,0xfcb9887c,0x62dd1ddf,0x15da2d49,0x8cd37cf3,0xfbd44c65,
		0x4db26158,0x3ab551ce,0xa3bc0074,0xd4bb30e2,0x4adfa541,0x3dd895d7,0xa4d1c46d,0xd3d6f4fb,
		0x4369e96a,0x346ed9fc,0xad678846,0xda60b8d0,0x44042d73,0x33031de5,0xaa0a4c5f,0xdd0d7cc9,
		0x5005713c,0x270241aa,0xbe0b1010,0xc90c2086,0x5768b525,0x206f85b3,0xb966d409,0xce61e49f,
		0x5edef90e,0x29d9c998,0xb0d09822,0xc7d7a8b4,0x59b33d17,0x2eb40d81,0xb7bd5c3b,0xc0ba6cad,
		0xedb88320,0x9abfb3b6,0x03b6e20c,0x74b1d29a,0xead54739,0x9dd277af,0x04db2615,0x73dc1683,
		0xe3630b12,0x94643b84,0x0d6d6a3e,0x7a6a5aa8,0xe40ecf0b,0x9309ff9d,0x0a00ae27,0x7d079eb1,
		0xf00f9344,0x8708a3d2,0x1e01f268,0x6906c2fe,0xf762575d,0x806567cb,0x196c3671,0x6e6b06e7,
		0xfed41b76,0x89d32be0,0x10da7a5a,0x67dd4acc,0xf9b9df6f,0x8ebeeff9,0x17b7be43,0x60b08ed5,
		0xd6d6a3e8,0xa1d1937e,0x38d8c2c4,0x4fdff252,0xd1bb67f1,0xa6bc5767,0x3fb506dd,0x48b2364b,
		0xd80d2bda,0xaf0a1b4c,0x36034af6,0x41047a60,0xdf60efc3,0xa867df55,0x316e8eef,0x4669be79,
		0xcb61b38c,0xbc66831a,0x256fd2a0,0x5268e236,0xcc0c7795,0xbb0b4703,0x220216b9,0x5505262f,
		0xc5ba3bbe,0xb2bd0b28,0x2bb45a92,0x5cb36a04,0xc2d7ffa7,0xb5d0cf31,0x2cd99e8b,0x5bdeae1d,
		0x9b64c2b0,0xec63f226,0x756aa39c,0x026d930a,0x9c0906a9,0xeb0e363f,0x72076785,0x05005713,
		0x95bf4a82,0xe2b87a14,0x7bb12bae,0x0cb61b38,0x92d28e9b,0xe5d5be0d,0x7cdcefb7,0x0bdbdf21,
		0x86d3d2d4,0xf1d4e242,0x68ddb3f8,0x1fda836e,0x81be16cd,0xf6b9265b,0x6fb077e1,0x18b74777,
		0x88085ae6,0xff0f6a70,0x66063bca,0x11010b5c,0x8f659eff,0xf862ae69,0x616bffd3,0x166ccf45,
		0xa00ae278,0xd70dd2ee,0x4e048354,0x3903b3c2,0xa7672661,0xd06016f7,0x4969474d,0x3e6e77db,
		0xaed16a4a,0xd9d65adc,0x40df0b66,0x37d83bf0,0xa9bcae53,0xdebb9ec5,0x47b2cf7f,0x30b5ffe9,
		0xbdbdf21c,0xcabac28a,0x53b39330,0x24b4a3a6,0xbad03605,0xcdd70693,0x54de5729,0x23d967bf,
		0xb3667a2e,0xc4614ab8,0x5d681b02,0x2a6f2b94,0xb40bbe37,0xc30c8ea1,0x5a05df1b,0x2d02ef8d,
	};
    unsigned int i,crc = 0xFFFFFFFF;
	const unsigned char *buffer = (unsigned char *)data;

    for (i = 0; i < size; i++) {
        crc = crc_tbl[(crc ^ buffer[i]) & 0xff] ^ (crc >> 8);
    }

    return ~crc ;
}

void flush_mq(mqd_t mq)
{
	struct mq_attr attr = {};
	int oflag = fcntl(mq, F_GETFL, 0);
	if( oflag<0 ) return;
	int nflag = oflag | O_NONBLOCK;
	if(nflag != oflag && fcntl(mq,F_SETFL,nflag)<0) return;
	mq_getattr(mq, &attr);
	char buf[attr.mq_msgsize];
	while( mq_receive(mq,buf,attr.mq_msgsize,0)>0 );
	if( nflag != oflag )
		fcntl(mq,F_SETFL,oflag);
}

int util_md5file(const char *file, char md5[33])
{
	int len = 0;
	char buf[1024 * 4];
	unsigned char md5buf[16]={};

	FILE *fp = fopen(file, "rb");
	if (!fp)
		return -1;
	MD5_CTX context;
	MD5_Init(&context);
	while ((len = fread(buf, 1, sizeof(buf), fp)) > 0)
		MD5_Update(&context, buf, len);
	MD5_Final(md5buf, &context);
	fclose(fp);
	for(len=0;len<16;len++)
		snprintf(md5+len*2,3,"%02x",md5buf[len]);
	return 0;
}

void util_md5sum(const void *buf,int len,char md5[33])
{
	unsigned char md5buf[16] ={};
	MD5_CTX context;
	MD5_Init(&context);
	MD5_Update(&context, buf, len);
	MD5_Final(md5buf, &context);
	for(len=0;len<16;len++)
		snprintf(md5+len*2,3,"%02x",md5buf[len]);
}

int util_read2buff(const char *filename, char *buff, int len)
{
	if( buff ) *buff = 0;
	if( !buff || len<=1 ) return 0;

	FILE *fp = fopen(filename, "rb");
	if (!fp) return -1;
	int ret = fread(buff, 1, len-1, fp);
	fclose(fp);
	if( ret>0 ) buff[ret] = 0;
	return ret;
}


int util_write_node(const char *node, const char *str)
{
    int fd, ret, len;

	LogDbg("node:%s, str:%s", node, str);

    do {
        fd = open(node, O_WRONLY|O_TRUNC|O_CLOEXEC|O_CREAT);
    } while (fd < 0 && errno == EINTR);

    if (fd < 0)
        return fd;

    len = strlen(str);
    do {
        ret = write(fd, str, len);
    } while (ret < 0 && errno == EINTR);

    close(fd);

    return ret;
}

unsigned int SysTick(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    return (unsigned int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void SysDelay(unsigned int timeoutMs)
{
    useconds_t t;
    int max_value = 1000;
    unsigned int t0 = SysTick() + timeoutMs;
    volatile unsigned int t1, count;
    int ret;
    count = timeoutMs;

    while (count > 0)
    {
        t1 = SysTick();
        if (t1 >= t0)
            return;

        t = (t0 - t1) > max_value ? max_value : (t0 - t1);
        ret = usleep(t * 1000);
        if (ret == 0)
            count -= t;
    }
}

void SysReboot(void)
{
	LogWarn("Reboot");
	system("sync");
	system("killall monitor");
	system("reboot");
	while (1) sleep(1);
}

int SystemWithResult(const char *cmd, char *result, int size)
{
	FILE *fp = popen(cmd, "r");
	if (result && size > 0)
		*result = 0;
	if (!fp) {
		LogError("exec cmd=%s fail,%s", cmd, strerror(errno));
		return -1;
	}
	int ret = fread(result, 1, size-1, fp);
	if (ret > 0)
		result[ret] = 0;

	pclose(fp);
	return ret;
}

int FileSize(const char *name)
{
	struct stat finfo;
	if (stat(name, &finfo) < 0)
		return -1;
	return (int)finfo.st_size;
}

int FileExistence(const char *name)
{
	struct stat finfo;
	if (!name)
		return -1;
	if (stat(name, &finfo) < 0)
		return -1;
	return 0;
}

#define MAX_ORDERCONTENT_ASCOUT_LENGTH  (1024*2) 

void SysBcd2Asc(unsigned char *Asc, unsigned char *Bcd, int inlen)
{
	if (inlen > MAX_ORDERCONTENT_ASCOUT_LENGTH)
		inlen = MAX_ORDERCONTENT_ASCOUT_LENGTH;

	int i;
	for (i = 0; i < inlen; i++)
	{
		Asc[i] = (i % 2) ? (Bcd[i / 2] & 0x0f) : ((Bcd[i / 2] >> 4) & 0x0f);
		Asc[i] += ((Asc[i] > 9) ? ('A' - 10) : '0');
	}
	return;
}

int util_get_ip(const char *ifname,uint32_t *ip,uint32_t *netmask)
{
	int ret = 0;
    struct ifreq ifr = {};

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if( fd<0 ) return -1;

	strcpy(ifr.ifr_name,ifname);
	if( ip ) {
	    ret = ioctl(fd, SIOCGIFADDR, &ifr);
	    if( ret<0 ) goto out;
		*ip = ((struct sockaddr_in*)&(ifr.ifr_addr))->sin_addr.s_addr;
	}
	if( netmask ) {
	    ret = ioctl(fd, SIOCGIFNETMASK, &ifr);
	    if( ret<0 ) goto out;
		*netmask = ((struct sockaddr_in*)&(ifr.ifr_addr))->sin_addr.s_addr;
	}
out:
	close(fd);
	return ret;
}

int util_get_str_ip(const char *ifname,char strIP[18],char strNetmask[18])
{
	unsigned ip=0,*p_ip=strIP?&ip:0;
	unsigned netmask=0,*p_nm = strNetmask?&netmask:0;
	if( util_get_ip(ifname,p_ip,p_nm)<0 ) return -1;
	if( strIP )
		snprintf(strIP,16,"%s",inet_ntoa(*((struct in_addr *)&ip)));
	if( strNetmask)
		snprintf(strNetmask,16,"%s",inet_ntoa(*((struct in_addr *)&netmask)));
	return 0;
}

char *util_get_str_gw(const char *ifname,char *gw,int len)
{
	char cmd[128],buf[128]={};

	*gw = 0;
	snprintf(cmd,sizeof(cmd),"iproute|grep \"default via \"%s %s",ifname?"|grep ":"",ifname?ifname:"");
	FILE *fp = popen(cmd,"r");
	if( fp && fgets(buf,sizeof(buf),fp) ) {
		char *p = buf+strlen("default via ");
		char *s = p;
		while(*s) { if( *s==' ' ) {*s=0;break;} s++;}
		snprintf(gw,len,"%s",p);
	}
	if( fp ) pclose(fp);
	return gw;
}

char *util_get_str_mac(const char *ifname,char *mac,int len)
{
	unsigned char bmac[6];

	struct ifreq ifr={};
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	*mac = 0;
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) != 0)
	{
		LogError("Error getting interface's MAC address:");
		close(sockfd);
		return mac;
	}

	memcpy(bmac, ifr.ifr_hwaddr.sa_data, 6);
	close(sockfd);
	snprintf(mac,len,"%02X:%02X:%02X:%02X:%02X:%02X",
		bmac[0],bmac[1],bmac[2],bmac[3],bmac[4],bmac[5]);
	return mac;
}

int GetInterfaceAddresses(const char *name, struct in_addr *addr, struct in_addr *netmask)
{
	return util_get_ip(name,(unsigned *)addr,(unsigned *)netmask);
}

int util_nslookup(char *ip,int ip_len,const char *domain,const char *nameserver)
{
	char buf[512]={},*p;
	in_addr_t inaddr;
	FILE *fp = NULL;
	int ret = -1;

	if( !domain || !domain[0] ) return -1;
	inaddr = inet_addr(domain);
	if( inaddr != INADDR_NONE && inaddr != INADDR_ANY ) {
		snprintf(ip,ip_len,"%s",domain);
		return 0;
	}
	snprintf(buf,sizeof(buf),"nslookup %s %s",domain,nameserver?nameserver:"");
	fp = popen(buf,"r");
	if( !fp ) return -1;
	ret = fread(buf,1,sizeof(buf)-1,fp);
	pclose(fp);

	if( ret<=0 ) return -1;
	buf[ret] = 0;
	p = strstr(buf,"Name:");
	if( !p ) return -1;
	p = strstr(p,"Address 1:");
	if( !p ) return -1;
	p += strlen("Address 1:");
	char addr[16]={};
	ret = sscanf(p,"%15s",addr);
	inaddr = inet_addr(addr);
	if( inaddr != INADDR_NONE && inaddr != INADDR_ANY ) {
		snprintf(ip,ip_len,"%s",addr);
		return 0;
	}
	return -1;
}

long long util_get_rx_bytes(const char *ifname)
{
	char rx_fname[128],buf[64]={};
	FILE *fp = NULL;

	snprintf(rx_fname,sizeof(rx_fname),"/sys/class/net/%s/statistics/rx_bytes",ifname);
	fp = fopen(rx_fname,"r");
	if( !fp ) return -1LL;
	int ret = fread(buf,1,sizeof(buf)-1,fp);
	fclose(fp);

	if( ret<0 ) return -1LL;
	return atoll(buf);
}

int util_random_str(char *str,int len)
{
    FILE *fp = fopen("/proc/sys/kernel/random/uuid","r");
    if( !fp || !fgets(str,len,fp) ) {
        srand(time(0));
        snprintf(str,len,"%08x-%d",(unsigned)rand(),getpid());
    }
    if( fp ) fclose(fp);
	return strlen(str);
}

