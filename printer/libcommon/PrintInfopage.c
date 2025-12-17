#include <libcommon.h>

static const char *CODE_PAGE_NAMES[] = {
	"0) PC437: USA, Standard Europe",
	"1) Katakana",
	"2) PC850: Multilingual",
	"3) PC860: Portuguese",
	"4) PC863: Canadian-French",
	"5) PC865: Nordic",
	"6) NULL: NULL",
	"7) NULL: NULL",
	"8) NULL: NULL",
	"9) NULL: NULL",
	"10) NULL: NULL",
	"11) PC851: Greek",
	"12) PC853: Turkish",
	"13) PC857: Turkish",
	"14) PC737: Greek",
	"15) ISO8859-7: Greek",
	"16) PC1252",
	"17) PC866: Cyrillic #2",
	"18) PC852: Latin2",
	"19) PC858: Euro",
	"20) KU42: Thai",
	"21) TIS11: Thai",
	"22) NULL: NULL",
	"23) NULL: NULL",
	"24) NULL: NULL",
	"25) NULL: NULL",
	"26) TIS18: Thai",
	"27) NULL: NULL",
	"28) NULL: NULL",
	"29) NULL: NULL",
	"30) TCVN-3: Vietnamese",
	"31) TCVN-3: Vietnamese",
	"32) PC720: Arabic",
	"33) PC775: Baltic Rim",
	"34) PC855: Cyrillic",
	"35) PC861: Icelandic",
	"36) PC862: Hebrew",
	"37) PC864: Arabic",
	"38) PC869: Greek",
	"39) ISO8859-2: Latin2",
	"40) ISO8859-15: Latin9",
	"41) NULL: NULL",
	"42) PC1118: Lithuanian",
	"43) NULL: NULL",
	"44) PC1125: Ukrainian",
	"45) PC1250: Latin 2",
	"46) PC1251: Cyrillic",
	"47) PC1253: Greek",
	"48) PC1254: Turkish",
	"49) PC1255: Hebrew",
	"50) PC1256: Arabic",
	"51) PC1257: Baltic Rim",
	"52) PC1258: Vietnamese",
	"53) KZ1048: Kazakhstan"
};

static const unsigned char CODE_PAGE_NUM[] = {
	0,
	1,
	2,
	3,
	4,
	5,
	11,
	12,
	13,
	14,
	15,
	16,
	17,
	18,
	19,
	20,
	21,
	26,
	30,
	31,
	32,
	33,
	34,
	35,
	36,
	37,
	38,
	39,
	40,
	42,
	44,
	45,
	46,
	47,
	48,
	49,
	50,
	51,
	52,
	53
};

char *get_bt_devname(char *buf,int len)
{
	if( strlen(sys_global_var()->sn) == 13 )
		snprintf(buf,len,"CloudPrint_%s",sys_global_var()->sn+9);
	else
		snprintf(buf,len,"CloudPrint");
	return buf;
}


/* Attention !!!!!!!!!!!!!!!!!!!!!!!!
 *dont change much content of the info page, which has strong correlations with the temperature test of tph and ntc
 */
int PrintInfoPage(void)
{
	LogDbg("init.");
	int ret = -1;
	char *PrtData = NULL;
	size_t PrtDataLen = 0;
	int i = 0, cnt = 0;
	unsigned int *bad_point_p = NULL;
	print_status_t status = {};
	int paper_size_adaptive = 0;

	char buf[128]={};
	int len = badpoint_info_from_file(buf,sizeof(buf),NULL);

	if (len<0) return ret;

	char *fmt = NULL;
	FILE *fp = open_memstream(&PrtData,&PrtDataLen);
	fwrite("\x1B\x40",1,2,fp);							//init printer
	fwrite("\x1D\x28\x45\x03\x00\x06\x00\x00",1,8,fp); //default ASCII 12X24
	/*low speed only for the test of ntc temperature*/
	if( sys_global_var()->prt_dots_per_line>384 )
	{	
		fwrite("\x1D\x28\x45\x02\x00\x08\x00",1,7,fp); //low speed
	}

	print_get_status(&status);

	/* 是否开启58纸仓自适应 */
	paper_size_adaptive = app_setting_get_int("printer", "PaperSizeAdaptive", 0);

	/* 80打印机装58mm打印纸，且未开启58纸仓自适应功能，需要设置左边距和打印宽度 */
	if ((sys_global_var()->prt_dots_per_line > 384) && (1 == status.paper_size) && (0 == paper_size_adaptive))
	{
		fwrite("\x1D\x4C\x60\x00",4,1,fp);	/* GS L nL nH 左边距96 */
		fwrite("\x1D\x57\x80\x01",4,1,fp);	/* GS W nL nH 打印宽度384 */
	}

	fputs("\x1B\x21\x18",fp);
	fputs("\x1B\x61\x31",fp); //center align
	fputs("\n** HWINFO PAGE **\n\n",fp);
	fwrite("\x1B\x21\x00",1,3,fp); //single printer mode
	fwrite("\x1B\x61\x30",1,3,fp); //left align

	fmt = "%-13s:%s\n";
	fputs("\n",fp);
	fprintf(fp,fmt,"Model",sys_global_var()->model);
	fprintf(fp,fmt,"Serial num",sys_global_var()->sn);
	fputs("\n",fp);

	fmt = "%-20s:%s\n";
	fprintf(fp,fmt,"HW version",sys_global_var()->hw_ver);
	fprintf(fp,fmt,"BootLoader version",sys_global_var()->boot_ver);
	fprintf(fp,fmt,"FW version",sys_global_var()->fw_ver);
	fprintf(fp,fmt,"SUNMI APP version",APP0_VERSION);
	fprintf(fp,fmt,"Partner APP version",APP1_VERSION);
	fprintf(fp,fmt,"MiniAPP version",MINIAPP_VERSION);

	/* Font Version */
	char fontVersion[16] = {};
	print_get_fontlib_version(fontVersion);
	fprintf(fp,fmt,"Font version",fontVersion);

	/* TTS Version */
	audio_cfg_t *audiocfg = get_audio_cfg_mmap();
	fprintf(fp, fmt,"TTS version",audiocfg?audiocfg->tts.version:"");

	fprintf(fp, fmt,"Drawer",SELF_TEST_STRING_YES);
	fprintf(fp, fmt, "Cover open sensor", SUNMI_COVER_OPEN_SENSOR);
	fprintf(fp, fmt, "End paper sensor", SUNMI_END_PAPER_SENSOR);
	fprintf(fp, fmt, "Near-end sensor", SUNMI_NEAR_END_SENSOR);
	fprintf(fp, fmt, "Take paper sensor", SUNMI_TAKE_PAPER_SENSOR);
	fprintf(fp, fmt, "Block sensor", SUNMI_PAPER_BLOCK_SENSOR);
	fprintf(fp, fmt, "Auto cutter", SUNMI_AUTO_CUTTER_SENSOR);
	fprintf(fp, fmt, "Black mark mode", SUNMI_BLACK_MARK_MODE);
	fprintf(fp, fmt, "Label mode", SUNMI_LABEL_MARK_MODE);
	fprintf(fp, fmt, "Alarm light", SUNMI_ALARM_LIGHT);

	fprintf(fp,fmt,"Print head dead dots","");
	cnt = len / 4; // cnt of 4byte
	bad_point_p = (unsigned int *)buf;
	/* 每行字符数sys_global_var()->prt_dots_per_line/12 每个int占9个字符"00000000 "*/
	int num_of_int_perline = sys_global_var()->prt_dots_per_line/12/9;
	for (i = 0; i < cnt; i++)
		fprintf(fp,"%08X%c",bad_point_p[i],((i%num_of_int_perline)==(num_of_int_perline-1))?'\n':' ');
	fputs("\n",fp);

	fmt = "%-13s:%s\n";
	char longitude[32] = {}, latitude[32] = {};
	sys_get_lbs(longitude, latitude);
	fprintf(fp, fmt, "Longitude", longitude);
	fprintf(fp, fmt, "Latitude", latitude);
	fputs("\n",fp);

	print_settings_t pring_settings;
	char encoding[32] = {}, codepage[32] = {};
	if (print_get_settings(&pring_settings) == 0) {
		if (pring_settings.Utf8_WordSet == 1)
			strcpy(encoding, "UTF-8");
		else {
			switch (pring_settings.CJK_WordSet) {
			case 1:   strcpy(encoding, "BIG5");      break;
			case 11:  strcpy(encoding, "Shift_JIS"); break;
			case 12:  strcpy(encoding, "JIS0208");   break;
			case 21:  strcpy(encoding, "KSC5601");   break;
			case 128: strcpy(encoding, "None");      break;
			default:  strcpy(encoding, "GB18030");   break;
			}
		}
		for (i = 0; i < 40; i++) {
			if (pring_settings.CodePage == CODE_PAGE_NUM[i])
				break;
		}
		if (i < 40) {
			char *s;
			s = strchr(CODE_PAGE_NAMES[pring_settings.CodePage], ' ');
			if (s) {
				strncpy(codepage, s + 1, sizeof(codepage) - 1);
				codepage[31] = 0;
				s = strchr(codepage, ':');
				if (s)
					(*s) = 0;
			}
		}
	}
	fprintf(fp, fmt, "Encoding", encoding);
	fprintf(fp, fmt, "Code page", codepage);
	fprintf(fp, "\n");

	fputs("## Interface ##\n",fp);
	fputs("[USB]\n",fp);
	fputs(SUNMI_PRINTER_USB_TYPE "\n",fp);
	fprintf(fp,"USB VID:%s\n",SUNMI_USB_VID);
	fprintf(fp,"USB PID:%s\n",SUNMI_USB_PID);
	fputs("\n",fp);

	if( sys_global_var()->lan_exist ) {
		char lan_ip[32]={},lan_gw[32]={},lan_netmask[32]={},lan_mac[32]={};

		fmt = "%-13s:%s\n";
		util_get_str_ip("eth0",lan_ip,lan_netmask);
		util_get_str_mac("eth0",lan_mac,sizeof(lan_mac));
		util_get_str_gw("eth0",lan_gw,sizeof(lan_gw));
		fprintf(fp,"[LAN]\n");
		fprintf(fp,fmt,"IP",lan_ip);
		fprintf(fp,fmt,"Netmask",lan_netmask);
		fprintf(fp,fmt,"GateWay",lan_gw);
		fprintf(fp,fmt,"MAC",lan_mac);
		fprintf(fp,fmt,"DHCP","Enable");

		char fixed_ip[32]={};
		net_LPrtSrv_ini_get("eth0","ip",fixed_ip,sizeof(fixed_ip));
		if( fixed_ip[0] ) {
			char fixed_netmask[32]={};
			net_LPrtSrv_ini_get("eth0","netmask",fixed_netmask,sizeof(fixed_netmask));
			fprintf(fp,"\n[LAN-Fixed]\n");
			fprintf(fp,fmt,"IP",fixed_ip);
			fprintf(fp,fmt,"Netmask",fixed_netmask);
		}
		fputs("\n",fp);
	}

	if( sys_global_var()->wifi_exist ) {
		char bt_name[64] = {},wifi_ip[24]={},wifi_nm[24]={},wifi_gw[24]={};
		fmt = "%-13s:%s\n";
		fputs("[Bluetooth]\n",fp);
		fputs(SUNMI_PRINTER_BLUETOOTH_TYPE "\n",fp);
		fprintf(fp,fmt,"BT Name",get_bt_devname(bt_name,sizeof(bt_name)));
		fprintf(fp,fmt,"BT MAC",sys_global_var()->bt_mac);
		fputs("\n",fp);

		fputs("[WiFi]\n",fp);
		fputs(SUNMI_PRINTER_WIFI_TYPE "\n",fp);
		util_get_str_ip("wlan0",wifi_ip,wifi_nm);
		util_get_str_gw("wlan0",wifi_gw,sizeof(wifi_gw));
		fprintf(fp,fmt,"IP",wifi_ip);
		fprintf(fp,fmt,"Netmask",wifi_nm);
		fprintf(fp,fmt,"Gateway",wifi_gw);
		fprintf(fp,fmt,"MAC",sys_global_var()->wifi_mac);
		fprintf(fp,fmt,"DHCP",SELF_TEST_STRING_ENABLE);
		fputs("\n",fp);

		char fixed_ip[32]={};
		net_LPrtSrv_ini_get("wlan0","ip",fixed_ip,sizeof(fixed_ip));
		if( fixed_ip[0] ) {
			char fixed_netmask[32]={};
			net_LPrtSrv_ini_get("wlan0","netmask",fixed_netmask,sizeof(fixed_netmask));
			fprintf(fp,"[WiFi-Fixed]\n");
			fprintf(fp,fmt,"IP",fixed_ip);
			fprintf(fp,fmt,"Netmask",fixed_netmask);
		}
		fputs("\n",fp);
	}

	if( sys_global_var()->wnet_exist ) {
		fmt = "%-8s:%s\n";
		fputs("[Mobile]\n",fp);
		char wnet_ip[32]={},isp_name[32]={};

		util_get_str_ip("ppp0",wnet_ip,0);
		fprintf(fp,fmt,"IP",wnet_ip);
		fprintf(fp,fmt,"Operator",wnet_ini_get(WNET_KEY_ISP,isp_name,sizeof(isp_name)));
		fputs("\n",fp);
	}

	fmt = "%-14s:%s\n";
	fputs("[Sound]\n",fp);
	char sys_vol[8] = {};
	if( audiocfg ) snprintf(sys_vol, sizeof(sys_vol),"%d",audiocfg->sys_volume);
	fprintf(fp,fmt,"Volume",sys_vol);
	fprintf(fp,fmt,"TTS Language",audiocfg?audiocfg->tts.lang:"");
	fputs("\n",fp);

	fmt = "%-13s:%s\n";
	fputs("[MQTT]\n",fp);
	fprintf(fp,fmt,"User Name",sys_global_var()->broker_uname);
	if (1) {
		char tmp[20];
		snprintf(tmp,sizeof(tmp),"******%.*s",6,sys_global_var()->broker_pwd+26);
		fprintf(fp,fmt,"Pasword",tmp);
	}
	fprintf(fp,fmt,"Address",sys_global_var()->broker_addr);
	fprintf(fp,fmt,"Port",sys_global_var()->broker_port);
	fprintf(fp,fmt,"Data",sys_global_var()->data_report?"On":"Off");
	ret = strlen(sys_global_var()->cloud_token);
	if (ret >= 4){
		char tmp[20];
		snprintf(tmp,sizeof(tmp),"%.*s******%.*s",2,sys_global_var()->cloud_token,2,sys_global_var()->cloud_token+ret-2);
		fprintf(fp,fmt,"Token",tmp);
	} else {
		fprintf(fp,fmt,"Token","");
	}
	fprintf(fp,fmt,"HTTP URL",sys_global_var()->cloud_url);
	T_PartnerParam param = {};
	GetPartnerParam(&param);
	if ( strlen(param.parterURL) ) {
		char tmp[sizeof(param.parterURL)+1] = {};
		snprintf(tmp,sizeof(tmp),"%s%s","\n",param.parterURL);
		fprintf(fp,fmt,"CALLBACK URL",tmp);
	}

	fputs("\n",fp);

	fwrite("\x1D\x28\x45\x03\x00\x06\x00\x00",1,8,fp); //back to default ASCII 12X24

	fputs("## Chinese Font ##\n",fp);
	fputs("GB18030(24x24)\n",fp);
	fwrite("\x1D\x28\x45\x03\x00\x06\x01\x00",1,8,fp); //GB18030(24x24)
	fwrite("\xB0\xA1\xB0\xA2\xB0\xA3\xB0\xA4\xB0\xA5\xB0\xA6\xB0\xA7\xB0\xA8\xB0\xA9\xB0\xAA",1,20,fp); //10 chinese characters
	fputs("\n",fp);

	fwrite("\x1D\x28\x45\x03\x00\x06\x00\x00",1,8,fp); //back to default ASCII 12X24
	fputs("\n",fp);

	fwrite("\x1D\x68\x30",1,3,fp); //bar code height  x30=48
	fwrite("\x1D\x77\x03",1,3,fp); //bar code module width
	fwrite("\x1D\x48\x32",1,3,fp); //below the barcode

	fputs("## Barcode Types ##",fp);

	fwrite("\x1B\x61\x30",1,3,fp); //left align
	fputs("\nEAN-13\n",fp);
	fwrite("\x1B\x61\x31",1,3,fp); //center align
	fwrite("\x1D\x6B\x02" "123456789012" "\x00",1,
				16,fp); //GS k m d1...dk NUL; m=2 is EAN-13

	fwrite("\x1B\x61\x30",1,3,fp); //left align
	fputs("\nQR Code\n",fp);
	fwrite("\x1B\x61\x31",1,3,fp);							//center align
	fwrite("\x1D\x28\x6B\x04\x00\x31\x41\x32\x00",1,9,fp); //GS ( k cn=49 fn=65
	fwrite("\x1D\x28\x6B\x03\x00\x31\x43\x08",1,8,fp);		//GS ( k cn=49 fn=67
	fwrite("\x1D\x28\x6B\x03\x00\x31\x45\x30",1,8,fp);		//GS ( k cn=49 fn=69
	char QRcontext[32] = {0};
	sprintf(QRcontext,"%s%c","\x1D\x28\x6B",(char)(strlen(sys_global_var()->sn)+3));
	memcpy(QRcontext+4,"\x00\x31\x50\x30",4); //GS ( k cn=49 fn=80
	strcpy(QRcontext+8,sys_global_var()->sn);
	fwrite(QRcontext,1,8+strlen(sys_global_var()->sn),fp);			
	fwrite("\x1D\x28\x6B\x03\x00\x31\x51\x30",1,8,fp); //GS ( k cn=49 fn=81

	/*only for the test of ntc temperature*/
	if( sys_global_var()->prt_dots_per_line>384 )
	{
		char buf[128] = {};
		int i = 0;
		int bytes_per_line = sys_global_var()->prt_dots_per_line/12; //修改字体宽度是需要同步修改这个
		memset(buf,'H',bytes_per_line);
		buf[bytes_per_line] = '\n';
		for(i=0; i<0; i++)    //0 gurantee over 2T (3-5 in different time) increasing in ntc, and over 2T (3-7 in different time) incresing in tph
		//for(i=0; i<20; i++)    //20 gurantee over 3T increasing in ntc, and over 5T incresing in tph
		//for(i=0; i<50; i++)  //50 gurantee over 4T increasing in ntc, and over 5T incresing in tph
		{
			if( i == 0 )
			{
				fprintf(fp,"\n\n");
			}
			fputs(buf,fp);
		}
	}

	fwrite("\x1B\x21\x18",1,3,fp);
	fwrite("\x1B\x61\x31",1,3,fp); //center align
	fputs("\n\n** COMPLETE **\n\n\n\n",fp);
	if( sys_global_var()->prt_dots_per_line>384 )
		fprintf(fp,"\n\n\n\n\n\n");

	/*only for the test of ntc temperature*/
	if( sys_global_var()->prt_dots_per_line>384 )
	{	
		fwrite("\x1D\x28\x45\x02\x00\x08\xFF",1,7,fp); //restore speed as default
	}
	
	fwrite("\x1D\x56\x31",1,3,fp); //partial cut
	fwrite("\x1B\x40",1,2,fp); //init printer
	fclose(fp);

	print_channel_send_data(PRINT_CHN_ID_INT, (unsigned char *)PrtData, PrtDataLen, 1);
	free(PrtData);

	return 0;
}
