#ifndef __HOST_CMD_DEF_H__
#define __HOST_CMD_DEF_H__

#include <stdint.h>

#define HOST_CMD_MAX_BLOCKS						(31)
#define HOST_CMD_MAX_PAYLOAD_SIZE				(HOST_CMD_MAX_BLOCKS << 4)
#define HOST_CMD_MAX_MSG_SIZE					(HOST_CMD_MAX_PAYLOAD_SIZE - 16)

#define HOST_CMD_TYPE_SET_LAN_FIXED_IP			(0x0101)
#define HOST_CMD_TYPE_SET_WIFI_FIXED_IP			(0x0102)
#define HOST_CMD_TYPE_SET_AUDIO_SWITCHES		(0x0103)
#define HOST_CMD_TYPE_CONFIGURE_WIFI			(0x0111)
#define HOST_CMD_TYPE_DELETE_WIFI_CONFIG		(0x0112)
#define HOST_CMD_TYPE_CONFIGURE_PRINTER			(0x0121)
#define HOST_CMD_TYPE_SET_BITS_PER_DOT			(0x0122)
#define HOST_CMD_TYPE_SET_APN					(0x0131)
#define HOST_CMD_TYPE_RESTART_WNET				(0x0132)
#define HOST_CMD_TYPE_SET_SERIAL_SETTINGS		(0x0141)
#define HOST_CMD_TYPE_GET_FONT_CONF_ITEMS		(0x0151)
#define HOST_CMD_TYPE_SET_FONT_CONF_ITEM_START	(0x0152)
#define HOST_CMD_TYPE_SET_FONT_CONF_ITEM		(0x0153)
#define HOST_CMD_TYPE_SET_FONT_CONF_ITEM_END	(0x0154)
#define HOST_CMD_TYPE_RESET_FONT_CONF			(0x0155)
#define HOST_CMD_TYPE_LIST_FILES				(0x0161)
#define HOST_CMD_TYPE_SEND_FILE_START			(0x0162)
#define HOST_CMD_TYPE_SEND_FILE_END				(0x0163)
#define HOST_CMD_TYPE_DELETE_FILE				(0x0164)
#define HOST_CMD_TYPE_CLEAR_WEB_PASSWORD		(0x0165)


#pragma pack(1)

typedef struct {
	uint8_t magic[3];
	uint8_t blocks;
	uint8_t payload[HOST_CMD_MAX_PAYLOAD_SIZE];
} host_cmd_msg_t;

typedef struct {
	uint16_t type;
} host_cmd_common_t;

typedef struct {
	char project[16];
	char model[16];
	char sn[24];
} host_cmd_dev_info_t;

typedef struct {
	uint16_t type;
	char project[16];
	char model[16];
	char sn[24];
	char hw_ver[16];
	char boot_ver[16];
	char fw_ver[16];
	char app0_ver[16];
	char app1_ver[16];
	char miniapp_ver[16];
	char font_ver[16];
	char tts_ver[16];
} host_cmd_settings01_t;

typedef struct {
	uint16_t type;
	char lan_mac[18];
	uint32_t lan_addr_dhcp;
	uint32_t lan_netmask_dhcp;
	uint32_t lan_addr_fixed;
	uint32_t lan_netmask_fixed;
	char wifi_ssid[128];
	char wifi_mac[18];
	uint32_t wifi_addr_dhcp;
	uint32_t wifi_netmask_dhcp;
	uint32_t wifi_addr_fixed;
	uint32_t wifi_netmask_fixed;
	char bt_mac[18];
	char bt_name[32];
} host_cmd_settings02_t;

typedef struct {
	uint16_t type;
	uint8_t ascii_wordset;
	uint8_t cjk_wordset;
	uint8_t codepage;
	uint8_t utf8_wordset;
	uint8_t print_density;
	uint8_t print_maxspeed;
	int paper_layout[8];
	uint32_t feed_and_cut_on_cover_closed;
	uint8_t black_mark_location;
	uint8_t paper_not_taken_actions;
} host_cmd_settings03_t;

typedef struct {
	uint16_t type;
	char version[64];
	char imei[16];
	char iccid[24];
	char imsi[16];
	char mcc[4];
	char mnc[4];
	char isp[64];
	char lbs[64];
	short rssi;
	short status;
	short use_specific_apn;
	char apn[64];
	char dial_number[16];
} host_cmd_settings04_t;

typedef struct {
	uint16_t type;
	int baudrate;
	int databits;
	int stopbits;
	int parity;
} host_cmd_settings05_t;

typedef struct {
	uint16_t type;
	uint8_t master;
	uint8_t cloud;
	uint8_t warning;
	uint8_t notice;
	uint8_t paper_not_taken;
} host_cmd_settings06_t;

typedef struct {
	uint16_t type;
	uint32_t lan_addr_fixed;
	uint32_t lan_netmask_fixed;
	uint32_t lan_gateway_fixed;
	uint32_t lan_dns_fixed;
	uint32_t wifi_addr_fixed;
	uint32_t wifi_netmask_fixed;
	uint32_t wifi_gateway_fixed;
	uint32_t wifi_dns_fixed;
} host_cmd_settings07_t;

typedef struct {
	uint16_t type;
	uint8_t bits_per_dot;
} host_cmd_settings08_t;

#define HOST_CMD_MAX_NV_GRAPHICS_FILES 64
typedef struct {
	uint16_t type;
	uint32_t ident;
	uint32_t memory_size;
	uint16_t more;
	uint16_t file_count;
	struct {
		uint8_t kc1;
		uint8_t kc2;
		uint32_t file_size;
	} nv_graphics_files[HOST_CMD_MAX_NV_GRAPHICS_FILES];
} host_cmd_settings09_t;

typedef struct {
	uint16_t type;
	uint32_t addr;
	uint32_t netmask;
	uint32_t gateway;
	uint32_t dns;
} host_cmd_ip_config_t;

typedef struct {
	uint16_t type;
	char ssid[128];
	char password[128];
} host_cmd_wifi_config_t;

typedef struct {
	uint16_t type;
	short use_specific_apn;
	char apn[64];
	char dial_number[16];
} host_cmd_apn_config_t;

typedef struct {
	uint16_t type;
	bool enabled;
	uint8_t ct;
	uint8_t px;
	uint8_t ad;
	char script[32];
	char language[4];
	char font[64];
	int faceindex;
	uint16_t threshold;
	uint16_t pixelsize_w;
	uint16_t pixelsize_h;
	uint16_t advance_x;
	uint32_t ranges[40][2];
} host_cmd_font_conf_item_t;

typedef struct {
	uint16_t type;
	char dir[128];
} host_cmd_list_files_t;

typedef struct {
	uint16_t type;
	char filename[128];
	uint32_t size;
} host_cmd_file_item_t;

typedef struct {
	uint16_t type;
	uint16_t result;
	char pathname[128];
	uint32_t size;
	uint8_t checksum[16];
} host_cmd_file_trans_info_t;

typedef struct {
	uint16_t type;
	char pathname[128];
} host_cmd_delete_file_t;

#pragma pack()

#endif /* __HOST_CMD_DEF_H__ */
