#include <openssl/aes.h>
#include <sys/types.h>
#include <dirent.h>
#include "libcommon.h"
#include "printer.h"
#include "serial.h"
#include "tcp.h"
#include "usb.h"
#include "HarfBuzz.h"
#include "LineBuffer.h"
#include "Runtime_Data.h"
#include "Util.h"
#include "host_cmd_def.h"
#include "host_cmd.h"

typedef struct {
	FILE *file;
	char pathname[128];
	MD5_CTX context;
	uint32_t bytes_transmitted;
	uint32_t bytes_total;
} file_trans_context_t;

typedef struct {
	uint8_t data[512];
	uint32_t len;
	uint32_t remaining;
} rcv_buf_t;

static const uint8_t AES_CBC_KEY[16] = {
	0x69, 0xe3, 0xb4, 0xa7, 0x70, 0x2c, 0x93, 0x0b,
	0xfe, 0x02, 0x56, 0x4b, 0x95, 0x0d, 0xaa, 0x3e
};

static const uint8_t AES_CBC_IV[AES_BLOCK_SIZE] = {
	0xd5, 0xb0, 0x70, 0x1d, 0xdb, 0x2d, 0x9f, 0x8e,
	0x4e, 0xf1, 0x75, 0xea, 0xa0, 0x69, 0x50, 0xe3
};

static file_trans_context_t file_trans_context;
static rcv_buf_t rcv_buf;
static uint8_t key_with_sn[16];
static uint32_t rand_num = 0;

static void dump_bytes(const char *text, const uint8_t *data, uint32_t len)
{
	char *str;
	uint32_t i, l;

	if (rand_num > 86400)
		return;

	str = (char *)malloc(strlen(text) + len * 2 + 16);
	if (!str)
		return;

	l = sprintf(str, "%s", text);
	for (i = 0; i < len; i++)
		l += sprintf(str + l, "%02x", data[i]);
	sprintf(str + l, " (len=%u)", len);

	LogDbg("%s", str);

	free(str);
}

static uint32_t inet_addr_to_uint32(const char *addr)
{
	struct in_addr inaddr;

	if (inet_aton(addr, &inaddr))
		return inaddr.s_addr;
	return 0;
}

static int file_data_received(const uint8_t *data, uint32_t len)
{
	uint32_t rem;

	rem = file_trans_context.bytes_total - file_trans_context.bytes_transmitted;
	if (len > rem)
		len = rem;
	if (len > 0) {
		fwrite(data, 1, len, file_trans_context.file);
		fflush(file_trans_context.file);
		MD5_Update(&file_trans_context.context, data, len);
		file_trans_context.bytes_transmitted += len;
//		LogDbg("%u bytes received (%8u/%8u).", len,
//			file_trans_context.bytes_transmitted, file_trans_context.bytes_total);
	}
	return (int)len;
}

static void update_key(void)
{
	MD5_CTX ctx;
	char buf[20];

	memset(buf, 0, sizeof(buf));
	memcpy(buf, &rand_num, 4);
	strncpy(buf + 4, sys_global_var()->sn, 16);

	MD5_Init(&ctx);
	MD5_Update(&ctx, buf, 20);
	MD5_Final(key_with_sn, &ctx);
}

static uint32_t pkcs7_padding(uint8_t *buf, uint32_t len)
{
	uint8_t bytes_to_add;

	bytes_to_add = AES_BLOCK_SIZE - (len % AES_BLOCK_SIZE);
	memset(buf + len, bytes_to_add, bytes_to_add);
	return (len + bytes_to_add);
}

static void aes_cbc_encrypt(const uint8_t *inbuf, uint8_t *outbuf, uint32_t len, const uint8_t *key, const uint8_t *iv)
{
	AES_KEY aes_key;
	uint8_t ivec[AES_BLOCK_SIZE];

	AES_set_encrypt_key(key, 128, &aes_key);
	memcpy(ivec, iv, AES_BLOCK_SIZE);

	AES_cbc_encrypt(inbuf, outbuf, len, &aes_key, ivec, AES_ENCRYPT);
}

static void aes_cbc_decrypt(const uint8_t *inbuf, uint8_t *outbuf, uint32_t len, const uint8_t *key, const uint8_t *iv)
{
	AES_KEY aes_key;
	uint8_t ivec[AES_BLOCK_SIZE];

	AES_set_decrypt_key(key, 128, &aes_key);
	memcpy(ivec, iv, AES_BLOCK_SIZE);

	AES_cbc_encrypt(inbuf, outbuf, len, &aes_key, ivec, AES_DECRYPT);
}

static uint32_t decrypt_cmd(const uint8_t *data, uint32_t blocks, host_cmd_msg_t *cmd, const uint8_t *key)
{
	uint8_t i, padding_bytes, *p;
	uint32_t len;

	if (blocks == 0)
		return 0;

	len = blocks << 4;
	aes_cbc_decrypt(data, cmd->payload, len, key, AES_CBC_IV);
	padding_bytes = cmd->payload[len - 1];
	if (padding_bytes < 1 || padding_bytes > 16)
		return 0;
	if (padding_bytes >= len)
		return 0;
	for (i = 0, p = cmd->payload + len - 1; i < padding_bytes; i++) {
		if (*p-- != padding_bytes)
			return 0;
	}

	return (len - padding_bytes);
}

static void send_rsp(host_cmd_msg_t *rsp, uint32_t len, int key_id, int src)
{
	const uint8_t *key;
	uint8_t outbuf[HOST_CMD_MAX_PAYLOAD_SIZE];

	if (len == 0 || len > HOST_CMD_MAX_MSG_SIZE)
		return;

	rsp->magic[0] = 0x10;
	rsp->magic[1] = 0x01;
	rsp->magic[2] = 0x11;

	len = pkcs7_padding(rsp->payload, len);
	key = (key_id == 0) ? AES_CBC_KEY : key_with_sn;
	aes_cbc_encrypt(rsp->payload, outbuf, len, key, AES_CBC_IV);
	memcpy(rsp->payload, outbuf, len);
	rsp->blocks = (key_id == 0) ? 0 : len >> 4;

	if (src == HOST_CMD_SRC_USB)
		usb_send_data(rsp, len + 4);
	else if (src == HOST_CMD_SRC_TCP)
		tcp_send_data(rsp, len + 4);
}

static void get_wifi_config(char **ssid, char **type, char **key)
{
	cJSON *root, *data;
	char *p;

	root = net_load_config(NET_WIFI);
	if (!root)
		return;

	data = cJSON_GetObjectItem(root, "D");
	if (data) {
		if (ssid) {
			p = cJSON_GetValueString(data, "S", "");
			(*ssid) = (p && p[0]) ? strdup(p) : NULL;
		}
		if (type) {
			p = cJSON_GetValueString(data, "T", "");
			(*type) = (p && p[0]) ? strdup(p) : NULL;
		}
		if (key) {
			p = cJSON_GetValueString(data, "P", "");
			(*key)  = (p && p[0]) ? strdup(p) : NULL;
		}
	}

	cJSON_Delete(root);
}

static void set_fixed_ip(uint8_t *data, int net_type)
{
	host_cmd_ip_config_t *c = (host_cmd_ip_config_t *)data;
	char ip[32], netmask[32], gateway[32], dns[32];

	if (c->addr) {
		strcpy(ip, inet_ntoa(*((struct in_addr *)&c->addr)));
		strcpy(netmask, inet_ntoa(*((struct in_addr *)&c->netmask)));
		if (c->gateway)
			strcpy(gateway, inet_ntoa(*((struct in_addr *)&c->gateway)));
		else
			gateway[0] = 0;
		if (c->dns)
			strcpy(dns, inet_ntoa(*((struct in_addr *)&c->dns)));
		else
			dns[0] = 0;
	}
	else {
		ip[0] = 0;
		netmask[0] = 0;
		gateway[0] = 0;
		dns[0] = 0;
	}
	update_LPrtSrv_config(net_type, ip, netmask, gateway, dns);
}

static void handle_command(uint8_t *data, uint32_t len, int src)
{
#define COPY_STRING(d,s) do { strncpy(r->d, s, sizeof(r->d) - 1); } while (0)

	host_cmd_common_t *cmd = (host_cmd_common_t *)data;
	host_cmd_msg_t rsp;

	dump_bytes("command:", data, len);

	if (len < sizeof(host_cmd_common_t))
		return;

	switch (cmd->type) {
	case 0x01:
	{
		host_cmd_settings01_t *r = (host_cmd_settings01_t *)rsp.payload;

		memset(rsp.payload, 0, sizeof(rsp.payload));
		r->type = cmd->type;

		COPY_STRING(project    , sys_global_var()->project );
		COPY_STRING(model      , sys_global_var()->model   );
		COPY_STRING(sn         , sys_global_var()->sn      );
		COPY_STRING(hw_ver     , sys_global_var()->hw_ver  );
		COPY_STRING(boot_ver   , sys_global_var()->boot_ver);
		COPY_STRING(fw_ver     , sys_global_var()->fw_ver  );
		COPY_STRING(app0_ver   , APP0_VERSION              );
		COPY_STRING(app1_ver   , APP1_VERSION              );
		COPY_STRING(miniapp_ver, MINIAPP_VERSION           );
		COPY_STRING(tts_ver    , get_audio_cfg_mmap() ? get_audio_cfg_mmap()->tts.version : "");
		print_get_fontlib_version(r->font_ver);

		send_rsp(&rsp, sizeof(host_cmd_settings01_t), 1, src);
	}
		break;

	case 0x02:
	{
		host_cmd_settings02_t *r = (host_cmd_settings02_t *)rsp.payload;
		char str[64], *ssid = NULL;
		const char *ifname;

		memset(rsp.payload, 0, sizeof(rsp.payload));
		r->type = cmd->type;

		COPY_STRING(lan_mac , sys_global_var()->lan_mac );
		COPY_STRING(wifi_mac, sys_global_var()->wifi_mac);
		COPY_STRING(bt_mac  , sys_global_var()->bt_mac  );

		ifname = net_ifname(NET_LAN);
		if (ifname) {
			util_get_ip(ifname, &r->lan_addr_dhcp, &r->lan_netmask_dhcp);
			if (net_LPrtSrv_ini_get(ifname, "ip", str, sizeof(str)) == 0)
				r->lan_addr_fixed = inet_addr_to_uint32(str);
			if (net_LPrtSrv_ini_get(ifname, "netmask", str, sizeof(str)) == 0)
				r->lan_netmask_fixed = inet_addr_to_uint32(str);
		}

		ifname = net_ifname(NET_WIFI);
		if (ifname) {
			util_get_ip(ifname, &r->wifi_addr_dhcp, &r->wifi_netmask_dhcp);
			if (net_LPrtSrv_ini_get(ifname, "ip", str, sizeof(str)) == 0)
				r->wifi_addr_fixed = inet_addr_to_uint32(str);
			if (net_LPrtSrv_ini_get(ifname, "netmask", str, sizeof(str)) == 0)
				r->wifi_netmask_fixed = inet_addr_to_uint32(str);
		}

		get_wifi_config(&ssid, NULL, NULL);
		if (ssid) {
			COPY_STRING(wifi_ssid, ssid);
			free(ssid);
		}

		get_bt_devname(r->bt_name, sizeof(r->bt_name));

		send_rsp(&rsp, sizeof(host_cmd_settings02_t), 1, src);
	}
		break;

	case 0x03:
	{
		host_cmd_settings03_t *r = (host_cmd_settings03_t *)rsp.payload;

		memset(rsp.payload, 0, sizeof(rsp.payload));
		r->type = cmd->type;

#define P(n__,k__) do { r->n__ = app_setting_get_int(PRINTER_SECTION, k__, 0); } while (0)
		P(ascii_wordset               , "ASCII_WordSet"          );
		P(cjk_wordset                 , "CJK_WordSet"            );
		P(codepage                    , "CodePage"               );
		P(utf8_wordset                , "Utf8_WordSet"           );
		P(print_density               , "Density"                );
		P(print_maxspeed              , "MaxSpeed"               );
		P(paper_layout[0]             , "PaperLayout.sa"         );
		P(paper_layout[1]             , "PaperLayout.sb"         );
		P(paper_layout[2]             , "PaperLayout.sc"         );
		P(paper_layout[3]             , "PaperLayout.sd"         );
		P(paper_layout[4]             , "PaperLayout.se"         );
		P(paper_layout[5]             , "PaperLayout.sf"         );
		P(paper_layout[6]             , "PaperLayout.sg"         );
		P(paper_layout[7]             , "PaperLayout.sh"         );
		P(feed_and_cut_on_cover_closed, "FeedAndCutOnCoverClosed");
		P(black_mark_location         , "BlackMarkLocation"      );
#undef P
		r->paper_not_taken_actions = get_paper_not_taken_actions();

		send_rsp(&rsp, sizeof(host_cmd_settings03_t), 1, src);
	}
		break;

	case 0x04:
	{
		host_cmd_settings04_t *r = (host_cmd_settings04_t *)rsp.payload;

		memset(rsp.payload, 0, sizeof(rsp.payload));
		r->type = cmd->type;

		wnet_ini_get(WNET_KEY_VER  , r->version, sizeof(r->version));
		wnet_ini_get(WNET_KEY_IMEI , r->imei   , sizeof(r->imei   ));
		wnet_ini_get(WNET_KEY_ICCID, r->iccid  , sizeof(r->iccid  ));
		wnet_ini_get(WNET_KEY_IMSI , r->imsi   , sizeof(r->imsi   ));
		wnet_ini_get(WNET_KEY_MCC  , r->mcc    , sizeof(r->mcc    ));
		wnet_ini_get(WNET_KEY_MNC  , r->mnc    , sizeof(r->mnc    ));
		wnet_ini_get(WNET_KEY_ISP  , r->isp    , sizeof(r->isp    ));
		wnet_ini_get(WNET_KEY_LBS  , r->lbs    , sizeof(r->lbs    ));
		r->rssi   = wnet_ini_get_int(WNET_KEY_RSSI_DB, 255);
		r->status = wnet_ini_get_int(WNET_KEY_STATUS , 255);

		r->use_specific_apn = app_setting_get_int(APP_SETTING_SECTION_WNET, APP_SETTING_WNET_KEY_SWITCH, 255);
		app_setting_get(APP_SETTING_SECTION_WNET, APP_SETTING_WNET_KEY_APN, r->apn, sizeof(r->apn));
		app_setting_get(APP_SETTING_SECTION_WNET, APP_SETTING_WNET_KEY_DIAL_NUM, r->dial_number, sizeof(r->dial_number));

		send_rsp(&rsp, sizeof(host_cmd_settings04_t), 1, src);
	}
		break;

	case 0x05:
	{
		host_cmd_settings05_t *r = (host_cmd_settings05_t *)rsp.payload;

		memset(rsp.payload, 0, sizeof(rsp.payload));
		r->type = cmd->type;

		r->baudrate = serial_settings.baudrate;
		r->databits = serial_settings.databits;
		r->stopbits = serial_settings.stopbits;
		r->parity   = serial_settings.parity;

		send_rsp(&rsp, sizeof(host_cmd_settings05_t), 1, src);
	}
		break;

	case 0x06:
	{
		host_cmd_settings06_t *r = (host_cmd_settings06_t *)rsp.payload;

		memset(rsp.payload, 0, sizeof(rsp.payload));
		r->type = cmd->type;

		r->master = app_setting_get_audio_switches(APP_SETTING_MASTER_AUDIO_SWITCH);
		r->cloud = app_setting_get_audio_switches(APP_SETTING_CLOUD_AUDIO_SWITCH);
		r->warning = app_setting_get_audio_switches(APP_SETTING_WARNNING_AUDIO_SWITCH);
		r->notice = app_setting_get_audio_switches(APP_SETTING_NOTICE_AUDIO_SWITCH);
		r->paper_not_taken = app_setting_get_audio_switches(APP_SETTING_PAPER_TAKEN_AUDIO_SWITCH) << 1;
		r->paper_not_taken |= app_setting_get_audio_switches(APP_SETTING_PAPER_BEEP_AUDIO_SWITCH);

		send_rsp(&rsp, sizeof(host_cmd_settings06_t), 1, src);
	}
		break;

	case 0x07:
	{
		host_cmd_settings07_t *r = (host_cmd_settings07_t *)rsp.payload;
		char str[64];
		const char *ifname;

		memset(rsp.payload, 0, sizeof(rsp.payload));
		r->type = cmd->type;

		ifname = net_ifname(NET_LAN);
		if (ifname) {
			if (net_LPrtSrv_ini_get(ifname, "ip", str, sizeof(str)) == 0)
				r->lan_addr_fixed = inet_addr_to_uint32(str);
			if (net_LPrtSrv_ini_get(ifname, "netmask", str, sizeof(str)) == 0)
				r->lan_netmask_fixed = inet_addr_to_uint32(str);
			if (net_LPrtSrv_ini_get(ifname, "gateway", str, sizeof(str)) == 0)
				r->lan_gateway_fixed = inet_addr_to_uint32(str);
			if (net_LPrtSrv_ini_get(ifname, "dns", str, sizeof(str)) == 0)
				r->lan_dns_fixed = inet_addr_to_uint32(str);
		}

		ifname = net_ifname(NET_WIFI);
		if (ifname) {
			if (net_LPrtSrv_ini_get(ifname, "ip", str, sizeof(str)) == 0)
				r->wifi_addr_fixed = inet_addr_to_uint32(str);
			if (net_LPrtSrv_ini_get(ifname, "netmask", str, sizeof(str)) == 0)
				r->wifi_netmask_fixed = inet_addr_to_uint32(str);
			if (net_LPrtSrv_ini_get(ifname, "gateway", str, sizeof(str)) == 0)
				r->wifi_gateway_fixed = inet_addr_to_uint32(str);
			if (net_LPrtSrv_ini_get(ifname, "dns", str, sizeof(str)) == 0)
				r->wifi_dns_fixed = inet_addr_to_uint32(str);
		}

		send_rsp(&rsp, sizeof(host_cmd_settings07_t), 1, src);
	}
		break;

	case 0x08:
	{
		host_cmd_settings08_t *r = (host_cmd_settings08_t *)rsp.payload;

		memset(rsp.payload, 0, sizeof(rsp.payload));
		r->type = cmd->type;

		r->bits_per_dot = app_setting_get_int(PRINTER_SECTION, "BitsPerDot", 0);

		send_rsp(&rsp, sizeof(host_cmd_settings08_t), 1, src);
	}
		break;

	case 0x09:
	{
		host_cmd_settings09_t *r = (host_cmd_settings09_t *)rsp.payload;
		nv_graphics_file_t *files;
		uint32_t i, count;

		memset(rsp.payload, 0, sizeof(rsp.payload));
		r->type = cmd->type;

		r->ident = time(NULL);
		r->memory_size = NV_GRAPHICS_MEM_CAP;
		if (GetNVGraphicsFileInfo(&files, &count) == 0) {
			if (files) {
				for (i = 0; i < count; i++) {
					r->nv_graphics_files[r->file_count].kc1 = files[i].kc1;
					r->nv_graphics_files[r->file_count].kc2 = files[i].kc2;
					r->nv_graphics_files[r->file_count].file_size = files[i].file_size;
					if (++r->file_count == HOST_CMD_MAX_NV_GRAPHICS_FILES) {
						if (i + 1 < count) /* More files are present */
							r->more = 1;
						send_rsp(&rsp, sizeof(host_cmd_settings09_t), 1, src);
						memset(rsp.payload, 0, sizeof(rsp.payload));
						r->type = cmd->type;
						r->ident = time(NULL);
						r->memory_size = NV_GRAPHICS_MEM_CAP;
					}
				}
				if (r->file_count > 0)
					send_rsp(&rsp, sizeof(host_cmd_settings09_t), 1, src);
				free(files);
				break;
			}
		}

		send_rsp(&rsp, sizeof(host_cmd_settings09_t), 1, src);
	}
		break;

	case HOST_CMD_TYPE_SET_LAN_FIXED_IP:
	{
		set_fixed_ip(data, NET_LAN);
	}
		break;

	case HOST_CMD_TYPE_SET_WIFI_FIXED_IP:
	{
		set_fixed_ip(data, NET_WIFI);
	}
		break;

	case HOST_CMD_TYPE_SET_AUDIO_SWITCHES:
	{
		host_cmd_settings06_t *c = (host_cmd_settings06_t *)data;

		app_setting_set_audio_switches(APP_SETTING_MASTER_AUDIO_SWITCH, c->master & 0x01);
		app_setting_set_audio_switches(APP_SETTING_CLOUD_AUDIO_SWITCH, c->cloud & 0x01);
		app_setting_set_audio_switches(APP_SETTING_WARNNING_AUDIO_SWITCH, c->warning & 0x01);
		app_setting_set_audio_switches(APP_SETTING_NOTICE_AUDIO_SWITCH, c->notice & 0x01);
		app_setting_set_audio_switches(APP_SETTING_PAPER_TAKEN_AUDIO_SWITCH, (c->paper_not_taken >> 1) & 0x01);
		app_setting_set_audio_switches(APP_SETTING_PAPER_BEEP_AUDIO_SWITCH, c->paper_not_taken & 0x01);
	}
		break;

	case HOST_CMD_TYPE_CONFIGURE_WIFI:
	{
		host_cmd_wifi_config_t *c = (host_cmd_wifi_config_t *)data;
		char param[384];

		snprintf(param, sizeof(param),
			"{\"SETTING\":\"WIFI\",\"D\":{\"S\":\"%s\",\"T\":\"%s\",\"P\":\"%s\",\"D\":\"E\"}}",
			c->ssid,
			(c->password[0]) ? "WPA" : "N",
			(c->password[0]) ? c->password : "");
		net_update_config(NET_WIFI, param);
	}
		break;

	case HOST_CMD_TYPE_DELETE_WIFI_CONFIG:
	{
		remove("/data/SYS/config");
		system("sync");
		net_wifi_stop();
	}
		break;

	case HOST_CMD_TYPE_CONFIGURE_PRINTER:
	{
		host_cmd_settings03_t *c = (host_cmd_settings03_t *)data;

#define P(n__,k__) do { app_setting_set_int(PRINTER_SECTION, k__, c->n__); } while (0)
		P(ascii_wordset               , "ASCII_WordSet"          ); Settings.ASC_WordSet = c->ascii_wordset;
		P(cjk_wordset                 , "CJK_WordSet"            ); Settings.CJK_WordSet = c->cjk_wordset;
		P(codepage                    , "CodePage"               ); Settings.CodePage = c->codepage;
		P(utf8_wordset                , "Utf8_WordSet"           ); Settings.Utf8_WordSet = c->utf8_wordset;
		P(print_density               , "Density"                ); printer_set_density(c->print_density, 0);
		P(print_maxspeed              , "MaxSpeed"               ); printer_set_maxspeed(c->print_maxspeed, 0);
		P(paper_layout[0]             , "PaperLayout.sa"         ); Settings.PaperLayout.sa = c->paper_layout[0];
		P(paper_layout[1]             , "PaperLayout.sb"         ); Settings.PaperLayout.sb = c->paper_layout[1];
		P(paper_layout[2]             , "PaperLayout.sc"         ); Settings.PaperLayout.sc = c->paper_layout[2];
		P(paper_layout[3]             , "PaperLayout.sd"         ); Settings.PaperLayout.sd = c->paper_layout[3];
		P(paper_layout[4]             , "PaperLayout.se"         ); Settings.PaperLayout.se = c->paper_layout[4];
		P(paper_layout[5]             , "PaperLayout.sf"         ); Settings.PaperLayout.sf = c->paper_layout[5];
		P(paper_layout[6]             , "PaperLayout.sg"         ); Settings.PaperLayout.sg = c->paper_layout[6];
		P(paper_layout[7]             , "PaperLayout.sh"         ); Settings.PaperLayout.sh = c->paper_layout[7];
		P(feed_and_cut_on_cover_closed, "FeedAndCutOnCoverClosed"); Settings.FeedAndCutOnCoverClosed = c->feed_and_cut_on_cover_closed;
		P(black_mark_location         , "BlackMarkLocation"      ); Settings.BlackMarkLocation = c->black_mark_location;
#undef P
		set_paper_not_taken_actions(c->paper_not_taken_actions);
	}
		break;

	case HOST_CMD_TYPE_SET_BITS_PER_DOT:
	{
		host_cmd_settings08_t *c = (host_cmd_settings08_t *)data;

		app_setting_set_int(PRINTER_SECTION, "BitsPerDot", c->bits_per_dot);
		Settings.BitsPerDot = c->bits_per_dot;
		lineBuffer->updateLineStride();
		SendPrinterCommand1(0x0D, Settings.BitsPerDot);
	}
		break;

	case HOST_CMD_TYPE_SET_APN:
	{
		host_cmd_apn_config_t *c = (host_cmd_apn_config_t *)data;

		app_setting_set(APP_SETTING_SECTION_WNET, APP_SETTING_WNET_KEY_APN, c->apn);
		app_setting_set(APP_SETTING_SECTION_WNET, APP_SETTING_WNET_KEY_DIAL_NUM, c->dial_number);
		app_setting_set_int(APP_SETTING_SECTION_WNET, APP_SETTING_WNET_KEY_SWITCH, c->use_specific_apn);
	}
		break;

	case HOST_CMD_TYPE_RESTART_WNET:
	{
		net_wnet_restart();
	}
		break;

	case HOST_CMD_TYPE_SET_SERIAL_SETTINGS:
	{
		host_cmd_settings05_t *c = (host_cmd_settings05_t *)data;

		serial_settings.baudrate = c->baudrate;
		serial_settings.databits = c->databits;
		serial_settings.stopbits = c->stopbits;
		serial_settings.parity   = c->parity;
		serial_settings_changed();
	}
		break;

	case HOST_CMD_TYPE_GET_FONT_CONF_ITEMS:
	{
		host_cmd_font_conf_item_t *r = (host_cmd_font_conf_item_t *)rsp.payload;
		void *inst = NULL;

		memset(rsp.payload, 0, sizeof(rsp.payload));
		r->type = cmd->type;

		while ((inst = harfBuzz->getFontConfItem(inst, r)) != NULL) {
			send_rsp(&rsp, sizeof(host_cmd_font_conf_item_t), 1, src);
			memset(rsp.payload, 0, sizeof(rsp.payload));
			r->type = cmd->type;
		}
	}
		break;

	case HOST_CMD_TYPE_SET_FONT_CONF_ITEM_START:
	{
		harfBuzz->setFontConfItemStart();
	}
		break;

	case HOST_CMD_TYPE_SET_FONT_CONF_ITEM:
	{
		host_cmd_font_conf_item_t *c = (host_cmd_font_conf_item_t *)data;

		harfBuzz->appendFontConfItem(c);
	}
		break;

	case HOST_CMD_TYPE_SET_FONT_CONF_ITEM_END:
	{
		harfBuzz->setFontConfItemEnd();
		harfBuzz->loadConf();
	}
		break;

	case HOST_CMD_TYPE_RESET_FONT_CONF:
	{
		harfBuzz->resetFontConf();
		harfBuzz->loadConf();
	}
		break;

	case HOST_CMD_TYPE_LIST_FILES:
	{
		host_cmd_list_files_t *c = (host_cmd_list_files_t *)data;
		host_cmd_file_item_t *r = (host_cmd_file_item_t *)rsp.payload;
		char pathname[256];
		DIR *dir;
		struct dirent *dirent;
		struct stat stbuf;

		memset(rsp.payload, 0, sizeof(rsp.payload));
		r->type = cmd->type;

		dir = opendir(c->dir);
		if (!dir)
			break;

		while ((dirent = readdir(dir)) != NULL) {
			snprintf(pathname, sizeof(pathname) - 1, "%s/%s", c->dir, dirent->d_name);
			if (stat(pathname, &stbuf))
				continue;
			if (!S_ISREG(stbuf.st_mode))
				continue;
			strncpy(r->filename, dirent->d_name, sizeof(r->filename) - 1);
			r->size = stbuf.st_size;
			send_rsp(&rsp, sizeof(host_cmd_file_item_t), 1, src);
			memset(rsp.payload, 0, sizeof(rsp.payload));
			r->type = cmd->type;
		}

		closedir(dir);
	}
		break;

	case HOST_CMD_TYPE_SEND_FILE_START:
	{
		host_cmd_file_trans_info_t *c = (host_cmd_file_trans_info_t *)data;
		host_cmd_file_trans_info_t *r = (host_cmd_file_trans_info_t *)rsp.payload;

		memset(rsp.payload, 0, sizeof(rsp.payload));
		r->type = cmd->type;

		LogInfo("Start receiving file \"%s\", %u bytes.", c->pathname, c->size);

		if (file_trans_context.file)
			fclose(file_trans_context.file);
		memset(&file_trans_context, 0, sizeof(file_trans_context));

		if (c->size == 0) {
			LogError("File is empty.");
			r->result = 1;
			goto _reply_1;
		}

		file_trans_context.file = fopen(c->pathname, "wb+");
		if (!file_trans_context.file) {
			LogError("Failed to create file.");
			r->result = 2;
			goto _reply_1;
		}

		strcpy(file_trans_context.pathname, c->pathname);
		MD5_Init(&file_trans_context.context);
		file_trans_context.bytes_total = c->size;
		r->result = 0;

_reply_1:
		send_rsp(&rsp, sizeof(host_cmd_file_trans_info_t), 1, src);
	}
		break;

	case HOST_CMD_TYPE_SEND_FILE_END:
	{
		host_cmd_file_trans_info_t *c = (host_cmd_file_trans_info_t *)data;
		host_cmd_file_trans_info_t *r = (host_cmd_file_trans_info_t *)rsp.payload;

		memset(rsp.payload, 0, sizeof(rsp.payload));
		r->type = cmd->type;

		LogInfo("Finish receiving file \"%s\", %u bytes.", c->pathname, c->size);

		if (!file_trans_context.file) {
			LogError("File not created.");
			r->result = 1;
			goto _reply_2;
		}

		if (strcmp(c->pathname, file_trans_context.pathname) ||
			c->size != file_trans_context.bytes_total) {
			LogError("File information not matched.");
			r->result = 2;
			goto _reply_2;
		}

		fclose(file_trans_context.file);
		sync();

		r->size = file_trans_context.bytes_transmitted;
		MD5_Final(r->checksum, &file_trans_context.context);
		r->result = 0;

		LogInfo("%s: %u/%u bytes, checksum %smatched.",
			file_trans_context.pathname,
			file_trans_context.bytes_transmitted,
			file_trans_context.bytes_total,
			memcmp(c->checksum, r->checksum, 16) == 0 ? "" : "not ");

		memset(&file_trans_context, 0, sizeof(file_trans_context));

_reply_2:
		send_rsp(&rsp, sizeof(host_cmd_file_trans_info_t), 1, src);
	}
		break;

	case HOST_CMD_TYPE_DELETE_FILE:
	{
		host_cmd_delete_file_t *c = (host_cmd_delete_file_t *)data;

		remove(c->pathname);
		sync();
	}
		break;

	case HOST_CMD_TYPE_CLEAR_WEB_PASSWORD:
	{
		//host_cmd_delete_file_t *c = (host_cmd_delete_file_t *)data;
		ini_set_value_with_section("/data/www/login.ini", "login", "password", "");
		remove("/tmp/token.ini");
		sync();
	}
		break;

	default:
		break;
	}

#undef COPY_STRING
}

int host_cmd_handler(const uint8_t *data, uint32_t len, int src)
{
	host_cmd_msg_t cmd, rsp;
	uint32_t explen;

	if (file_trans_context.file &&
		file_trans_context.bytes_transmitted < file_trans_context.bytes_total)
		return file_data_received(data, len);

	if (rcv_buf.len > 0) {
		explen = (rcv_buf.remaining <= len) ? rcv_buf.remaining : len;
		memcpy(rcv_buf.data + rcv_buf.len, data, explen);
		rcv_buf.len += explen;
		rcv_buf.remaining -= explen;
		if (rcv_buf.remaining > 0)
			return explen;
		goto _decrypt;
	}

	if (len < 20)
		return 0;
	if (data[0] != 0x10 || data[1] != 0x01 || data[2] != 0x11 || data[3] > HOST_CMD_MAX_BLOCKS)
		return 0;

	if (data[3] == 0) {
		len = decrypt_cmd(data + 4, 1, &cmd, AES_CBC_KEY);
		if (len != sizeof(uint32_t))
			return 0;

		rand_num = *((uint32_t *)cmd.payload);
		LogDbg("rand_num=0x%08x", rand_num);

		if (rand_num != 0) {
			update_key();

			host_cmd_dev_info_t *devinfo = (host_cmd_dev_info_t *)rsp.payload;

			memset(devinfo, 0, sizeof(host_cmd_dev_info_t));
			strncpy(devinfo->project, sys_global_var()->project, sizeof(devinfo->project) - 1);
			strncpy(devinfo->model,   sys_global_var()->model,   sizeof(devinfo->model  ) - 1);
			strncpy(devinfo->sn,      sys_global_var()->sn,      sizeof(devinfo->sn     ) - 1);
			send_rsp(&rsp, sizeof(host_cmd_dev_info_t), 0, src);
		}

		return 20;
	}

	if (rand_num == 0)
		return 0;

	explen = (data[3] << 4) + 4;
	if (explen > len) {
		LogDbg("explen=%u, len=%u", explen, len);
		memcpy(rcv_buf.data, data, len);
		rcv_buf.len = len;
		rcv_buf.remaining = explen - len;
		return len;
	}

	memcpy(rcv_buf.data, data, explen);
	rcv_buf.len = explen;
	rcv_buf.remaining = 0;

_decrypt:
	len = decrypt_cmd(rcv_buf.data + 4, rcv_buf.data[3], &cmd, key_with_sn);
	if (len > 0)
		handle_command(cmd.payload, len, src);

	rcv_buf.len = 0;
	rcv_buf.remaining = 0;

	return explen;
}
