#ifndef __GLOBAL_VAL_H__
#define __GLOBAL_VAL_H__

#include <sys/mman.h>
#include <hwinfo.h>
enum {
	BOOT_MODE_USBDOWNLOADER,
	BOOT_MODE_NORMAL,
	BOOT_MODE_FACTORY,
};
#define UBOOT_VERSION "1.0.1"
#define GLOBAL_VAR_CFG_FILE "/global_var"
#define HWINFO_KERNEL_DEV	"/dev/hwinfo"
#define CONFIG_NODE         "/data/SYS/config"
#define CONSOLE_RX_DISABLE_TAG				"rx_disable"
#define CONSOLE_RX_ENABLE_TAG				"rx_enable"
#define CONSOLE_RX_NODE     				"/sys/class/sunmi/base/gpio_console_rx"
#define CONSOLE_RX_DISABLE_FLAG  			"**##C_DIS_321**##"
#define CONSOLE_RX_ENABLE_FLAG				"**##C_EN_123**##"
#define SYS_SN_STRING_LEN       			13   //the string len of SN
#define SYS_SN_CRC_STRING_LEN       		8    //the string len of CRC of SN
#define WNET_ICCID_MAX_LEN					20
#define WNET_IMSI_MAX_LEN					15
#define WNET_APN_MAX_LEN					64
#define SUNMI_USB_VID                      "324F"
#define SUNMI_USB_PID                      (!strcmp(sys_global_var()->project,"NT211")?"0002":"00D2")
#define SUNMI_BRAND_NAME                   "SUNMI"
#define SELF_TEST_STRING_YES               "Yes"
#define SELF_TEST_STRING_ENABLE            "Enable"
#define SELF_TEST_STRING_DISENABLE         "Disable"
#define SUNMI_PRINTER_RESOLUTION           "203*203dpi"
#define SUNMI_PRINTER_DEFAULT_CODE_PAGE    "Page 437"
#define SUNMI_PRINTER_DEFAULT_LANGUAGE     "GB18030"
#define SUNMI_PRINTER_USB_TYPE             "USB 2.0"
#define SUNMI_PRINTER_WIFI_TYPE            "WiFi 2.4G"
#define SUNMI_PRINTER_BLUETOOTH_TYPE       "BT BLE 4.2"
#define SUNMI_PRINTER_GPRS_OPERATOR        "China Mobile"
#define SUNMI_PRINTER_EXSAMPLE_LETTERS     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

#define SUNMI_COVER_OPEN_SENSOR		((!strcmp(sys_global_var()->project,"NT310")) || (!strcmp(sys_global_var()->project,"NT311"))?SELF_TEST_STRING_ENABLE:SELF_TEST_STRING_DISENABLE)
#define SUNMI_END_PAPER_SENSOR		(SELF_TEST_STRING_ENABLE)
#define SUNMI_NEAR_END_SENSOR		((!strcmp(sys_global_var()->project,"NT310"))|| (!strcmp(sys_global_var()->project,"NT311"))?SELF_TEST_STRING_ENABLE:SELF_TEST_STRING_DISENABLE)
#define SUNMI_TAKE_PAPER_SENSOR		((!strcmp(sys_global_var()->project,"NT310"))  || (!strcmp(sys_global_var()->project,"NT311"))?SELF_TEST_STRING_ENABLE:SELF_TEST_STRING_DISENABLE)
#define SUNMI_PAPER_BLOCK_SENSOR	((!strcmp(sys_global_var()->project,"NT310")) || (!strcmp(sys_global_var()->project,"NT311"))?SELF_TEST_STRING_ENABLE:SELF_TEST_STRING_DISENABLE)
#define SUNMI_AUTO_CUTTER_SENSOR	((!strcmp(sys_global_var()->project,"NT310")) || (!strcmp(sys_global_var()->project,"NT311"))?SELF_TEST_STRING_ENABLE:SELF_TEST_STRING_DISENABLE)
#define SUNMI_BLACK_MARK_MODE		((!strcmp(sys_global_var()->project,"NT310")) || (!strcmp(sys_global_var()->project,"NT311"))?SELF_TEST_STRING_ENABLE:SELF_TEST_STRING_DISENABLE)
#define SUNMI_LABEL_MARK_MODE		(SELF_TEST_STRING_DISENABLE)
#define SUNMI_ALARM_LIGHT			((!strcmp(sys_global_var()->project,"NT310")) || (!strcmp(sys_global_var()->project,"NT311"))?SELF_TEST_STRING_ENABLE:SELF_TEST_STRING_DISENABLE)

#define is_lbs_got sys_lbs_exist()

#define GLOBAL_CFG_MAGIC 0x4d637647 // "GvcM"

#define SUNMI_CPU_TYPE_X1021  "X1021"
#define SUNMI_CPU_TYPE_X1600  "X1600E"

typedef struct {
	unsigned magic;
	unsigned short prt_dots_per_line;
	char boot_mode;
	unsigned char boot_select:1;
	unsigned char disable_console_rx:1;
	unsigned char wifi_exist:1;
	unsigned char wnet_exist:1;
	unsigned char lan_exist:1;
	unsigned char data_report:1;
	char sn[24];
	char project[16];
	char model[16];
	char wifi_md[16];
	char wnet_md[16];
	char lan_md[16];
	char prt_md[16];
	char wifi_mac[18];
	char bt_mac[18];
	char lan_mac[18];
	char hw_ver[16];
	char hw_tag[16];
	char boot_ver[16];
	char fw_ver[24];
	char broker_addr[128];
	char broker_port[16];
	char broker_uname[32];
	char broker_pwd[48];
	char cloud_url[128];
	char cloud_token[32];
} global_var_t;

global_var_t *sys_global_var();
int HwinfoRead(char buf[HWINFO_WHOLE_LENS]);
void sync_global_var(const void *buf,int len);
void sys_get_lbs(char *lng,char *lat);
void sys_save_lbs(char *lbs);
bool sys_lbs_exist();

#endif
