#include <sys/types.h>
#include <dirent.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libcommon.h"
#include "printer.h"
#include "print_server.h"
#include "Instruct_Proc.h"
#include "ESC_Instruct.h"
#include "GS_Instruct.h"
#include "LineBuffer.h"
#include "PageBuffer.h"
#include "Util.h"
#include "Runtime_Data.h"

const uint8_t CODE_PAGE_MAPPINGS[64] = {
	0,   1,   2,   3,   4,   5,   255, 255,
	255, 255, 255, 6,   7,   8,   9,   10,
	11,  12,  13,  14,  15,  16,  255, 255,
	255, 255, 17,  255, 255, 255, 18,  19,
	20,  21,  22,  23,  24,  25,  26,  27,
	28,  255, 29,  255, 30,  31,  32,  33,
	34,  35,  36,  37,  38,  39,  255, 255,
	255, 255, 255, 255, 255, 255, 255, 255
};

uint8_t DataBuffer[DATA_BUFFER_SIZE];
Settings_t Settings;
print_log_config_t print_log_config;
Barcode_t Barcode;
PDF417_t PDF417;
QRCode_t QRCode;
downloaded_bit_image_t DownloadedBitImage;
uint8_t MemorySwitches[8] = {0, 0, 0, 0, 0, 0, 0, 0};
volatile uint32_t Runtime_Flag = 0;
unsigned char cutter_flag = 0;// 0-初始化状态 1-上电后切刀（半切） 2-开合盖后切刀（半切）  3-全切 4-半切 5-空闲
unsigned char print_chn_id_filter = 0; //对于追加头部和尾部功能，用于过滤掉PRINT_CHN_ID_INT和PRINT_CHN_ID_CMD

static void load_uint8(const char *key, uint8_t *pv, uint8_t def)
{
	int value = app_setting_get_int(PRINTER_SECTION, key, -1);

	if (value >= 0 && value <= 255) {
		*pv = (uint8_t)value;
	}
	else {
		*pv = def;
		app_setting_set_int(PRINTER_SECTION, key, def);
	}
}

static void load_uint16(const char *key, uint16_t *pv, uint16_t def)
{
	int value = app_setting_get_int(PRINTER_SECTION, key, -1);

	if (value != -1) {
		*pv = (uint16_t)value;
	}
	else {
		*pv = def;
		app_setting_set_int(PRINTER_SECTION, key, def);
	}
}

void load_uint32(const char *key, uint32_t *pv, uint32_t def)
{
	int value = app_setting_get_int(PRINTER_SECTION, key, -1);

	if (value != -1) {
		*pv = (uint32_t)value;
	}
	else {
		*pv = def;
		app_setting_set_int(PRINTER_SECTION, key, def);
	}
}

void load_int(const char *key, int *pv, int def)
{
	int value = app_setting_get_int(PRINTER_SECTION, key, -1);

	if (value != -1) {
		*pv = value;
	}
	else {
		*pv = def;
		app_setting_set_int(PRINTER_SECTION, key, def);
	}
}

static uint16_t leftMargin(void)
{
	if (Settings.Mode == PAGE_MODE)
		return 0;
	if (((1 == print_status()->paper_size) && (1 == Settings.PaperSizeAdaptive)) || (2 == Settings.PaperSizeAdaptive))
	{
		if(Settings.PrintHorizontalAccuracy == 0){
			Settings.FixedLeftMargin = 0;
		}else if(Settings.PrintHorizontalAccuracy == 1){
			Settings.FixedLeftMargin = 24;
		}
		return Settings.LeftMargin + 96 + Settings.FixedLeftMargin;
	}
	else
	{
		if(DOTS_PER_LINE == 576){
			if(Settings.PrintHorizontalAccuracy == 0){
				Settings.FixedLeftMargin = 0;
			}else if(Settings.PrintHorizontalAccuracy == 1){
				Settings.FixedLeftMargin = 36;
			}
		}else{
			if(Settings.PrintHorizontalAccuracy == 0){
				Settings.FixedLeftMargin = 0;
			}else if(Settings.PrintHorizontalAccuracy == 1){
				Settings.FixedLeftMargin = 24;
			}
		}
		return Settings.LeftMargin + Settings.FixedLeftMargin;
	}
}

static uint16_t printAreaWidth(void)
{
	if (Settings.Mode == PAGE_MODE)
		return Settings.rightMargin();
	if (((1 == print_status()->paper_size) && (1 == Settings.PaperSizeAdaptive)) || (2 == Settings.PaperSizeAdaptive))
	{
		if(Settings.PrintHorizontalAccuracy == 0){
			Settings.FixedPrintAreaWidth = 384;
		}else if(Settings.PrintHorizontalAccuracy == 1){
			Settings.FixedPrintAreaWidth = 336;
		}
		uint16_t print_area_width = (Settings.PrintAreaWidth > Settings.FixedPrintAreaWidth) ? Settings.FixedPrintAreaWidth : Settings.PrintAreaWidth;
		return (print_area_width > 384) ? 384 : print_area_width;
	}
	else
	{
		if(DOTS_PER_LINE == 576){
			if(Settings.PrintHorizontalAccuracy == 0){
				Settings.FixedPrintAreaWidth = 576;
			}else if(Settings.PrintHorizontalAccuracy == 1){
				Settings.FixedPrintAreaWidth = 504;
			}
		}else{
			if(Settings.PrintHorizontalAccuracy == 0){
				Settings.FixedPrintAreaWidth = 384;
			}else if(Settings.PrintHorizontalAccuracy == 1){
				Settings.FixedPrintAreaWidth = 336;
			}
		}
		uint16_t temp = (Settings.PrintAreaWidth > Settings.FixedPrintAreaWidth) ? Settings.FixedPrintAreaWidth : Settings.PrintAreaWidth;
		return (temp > DOTS_PER_LINE) ? DOTS_PER_LINE : temp;
	}
}

static uint16_t rightMargin(void)
{
	uint16_t rm;

	if (Settings.Mode == PAGE_MODE) {
		switch (pageBuffer->Direction) {
		case 1:
		case 3:
			return pageBuffer->PrintArea.h;
		default:
			return pageBuffer->PrintArea.w;
		}
	}

	rm = leftMargin() + printAreaWidth();
	if (((1 == print_status()->paper_size) && (1 == Settings.PaperSizeAdaptive)) || (2 == Settings.PaperSizeAdaptive))
	{
		return (rm > 480) ? 480 : rm;
	}
	else
	{
		return (rm > DOTS_PER_LINE) ? DOTS_PER_LINE : rm;
	}
}

static uint16_t lineSpacing(void)
{
	return (Settings.Mode == PAGE_MODE) ? Settings.LineSpacing[1]
										: Settings.LineSpacing[0];
}

static uint8_t rightCharSpacing(int char_type)
{
	return (Settings.Mode == PAGE_MODE) ? Settings.RightCharSpacing[1][char_type]
										: Settings.RightCharSpacing[0][char_type];
}

static bool isClockwiseRotation(void)
{
	return (Settings.Mode == STANDARD_MODE && Settings.ClockwiseRotationMode) ? true : false;
}

static bool isUpsideDown(void)
{
	return (Settings.Mode == STANDARD_MODE && Settings.UpsideDownMode) ? true : false;
}

typedef struct {
	uint8_t kc1;
	uint8_t kc2;
	uint32_t width;
	uint32_t height;
	int format;
	uint32_t pitch;
	uint32_t bytes_total;
	uint32_t bytes_remain;
	uint32_t bytes_received;
	uint8_t *buffer;
} nv_graphics_data_context_t;

static nv_graphics_data_context_t nv_graphics_data_context;

void DefineNVGraphicsData_Init(uint8_t kc1, uint8_t kc2, uint16_t width, uint16_t height, int format)
{
	if (nv_graphics_data_context.buffer)
		free(nv_graphics_data_context.buffer);

	memset(&nv_graphics_data_context, 0, sizeof(nv_graphics_data_context));
	nv_graphics_data_context.kc1 = kc1;
	nv_graphics_data_context.kc2 = kc2;
	nv_graphics_data_context.width = width;
	nv_graphics_data_context.height = height;
	nv_graphics_data_context.format = format;

	switch (format) {
	case NV_GRAPHICS_DATA_RASTER_FORMAT:
		nv_graphics_data_context.pitch = (width + 7) >> 3;
		nv_graphics_data_context.bytes_total = nv_graphics_data_context.pitch * height;
		break;
	case NV_GRAPHICS_DATA_COLUMN_FORMAT:
		nv_graphics_data_context.pitch = (height + 7) >> 3;
		nv_graphics_data_context.bytes_total = nv_graphics_data_context.pitch * width;
		break;
	default:
		return;
	}

	nv_graphics_data_context.bytes_remain = nv_graphics_data_context.bytes_total;
	nv_graphics_data_context.buffer = (uint8_t *)calloc(nv_graphics_data_context.bytes_total, 1);

	LogInfo("\"%c%c\",%u,%u,%u,%u", (char)kc1, (char)kc2, width, height,
		nv_graphics_data_context.pitch,
		nv_graphics_data_context.bytes_total);
}

void DefineNVGraphicsData_Append(uint8_t *data, uint32_t len)
{
	if (!nv_graphics_data_context.buffer ||
		nv_graphics_data_context.bytes_remain == 0 ||
		len == 0)
		return;

	if (len > nv_graphics_data_context.bytes_remain)
		len = nv_graphics_data_context.bytes_remain;
	memcpy(nv_graphics_data_context.buffer + nv_graphics_data_context.bytes_received, data, len);
	nv_graphics_data_context.bytes_remain -= len;
	nv_graphics_data_context.bytes_received += len;

	if (nv_graphics_data_context.bytes_remain > 0)
		return;

	if (nv_graphics_data_context.format == NV_GRAPHICS_DATA_COLUMN_FORMAT) {
		;
	}

	char pathname[64];
	uint8_t buf[16];
	int fd;

	system("/bin/mkdir -p " NV_GRAPHICS_DIR);
	sprintf(pathname, NV_GRAPHICS_DIR "%02x_%02x.bin",
		nv_graphics_data_context.kc1, nv_graphics_data_context.kc2);
	fd = open(pathname, O_CREAT|O_WRONLY|O_TRUNC, 0644);
	if (fd < 0) {
		LogError("Failed to create \"%s\".", pathname);
		goto _exit;
	}
	memset(buf, 0, sizeof(buf));
	*((uint16_t *)(buf    )) = nv_graphics_data_context.width;
	*((uint16_t *)(buf + 2)) = nv_graphics_data_context.height;
	write(fd, buf, 16);

	write(fd, nv_graphics_data_context.buffer, nv_graphics_data_context.bytes_total);
	fsync(fd);
	close(fd);

	LogInfo("File created (width=%u, height=%u).",
		nv_graphics_data_context.width, nv_graphics_data_context.height);

_exit:
	free(nv_graphics_data_context.buffer);
	memset(&nv_graphics_data_context, 0, sizeof(nv_graphics_data_context));
}

static uint8_t key_code_hexstr_to_value(char c1, char c2)
{
	uint8_t kc = 0;

	if (c1 >= '0' && c1 <= '9')
		kc = c1 - 48;
	else if (c1 >= 'a' && c1 <= 'f')
		kc = c1 - 87;
	kc <<= 4;
	if (c2 >= '0' && c2 <= '9')
		kc += c2 - 48;
	else if (c2 >= 'a' && c2 <= 'f')
		kc += c2 - 87;
	return kc;
}

int GetNVGraphicsFileInfo(nv_graphics_file_t **files, uint32_t *count)
{
	char pathname[256];
	nv_graphics_file_t *p;
	DIR *dir;
	struct dirent *dirent;
	struct stat stbuf;
	regex_t regex;
	int ret = -1;

	(*files) = NULL;
	(*count) = 0;

	dir = opendir(NV_GRAPHICS_DIR);
	if (!dir)
		return -1;

	regcomp(&regex, "[0-9a-f]{2}_[0-9a-f]{2}.bin", REG_EXTENDED);

	while ((dirent = readdir(dir)) != NULL) {
		if (regexec(&regex, dirent->d_name, 0, NULL, 0))
			continue;
		snprintf(pathname, sizeof(pathname) - 1, "%s/%s", NV_GRAPHICS_DIR, dirent->d_name);
		if (stat(pathname, &stbuf))
			continue;
		if (!S_ISREG(stbuf.st_mode))
			continue;
		(*count)++;
	}

	if ((*count) > 0) {
		(*files) = (nv_graphics_file_t *)calloc((*count), sizeof(nv_graphics_file_t));
		if ((*files) != NULL) {
			rewinddir(dir);
			p = (*files);
			while ((dirent = readdir(dir)) != NULL) {
				if (regexec(&regex, dirent->d_name, 0, NULL, 0))
					continue;
				snprintf(pathname, sizeof(pathname) - 1, "%s/%s", NV_GRAPHICS_DIR, dirent->d_name);
				if (stat(pathname, &stbuf))
					continue;
				if (!S_ISREG(stbuf.st_mode))
					continue;
				p->kc1 = key_code_hexstr_to_value(dirent->d_name[0], dirent->d_name[1]);
				p->kc2 = key_code_hexstr_to_value(dirent->d_name[3], dirent->d_name[4]);
				p->file_size = stbuf.st_size;
				p++;
			}
			ret = 0;
		}
	}

	regfree(&regex);

	closedir(dir);

	return ret;
}

#define PRINT_TASK_BITMAP_PATH "/tmp/print_task_bitmap/"

typedef struct {
	char pathname[64];
	FILE *file;
	time_t last_time;
	int suffix;
	bool enabled;
} print_task_bitmap_context_t;

#pragma pack(1)
typedef struct {
	uint8_t depth;
	uint16_t bytes_per_line;
	uint8_t spare[13];
} print_task_bitmap_hdr_t;
#pragma pack()

static print_task_bitmap_context_t print_task_bitmap_context;

void SavePrintTaskBitmap_SetEnabledState(bool enabled)
{
	if (enabled)
		system("/bin/mkdir -p " PRINT_TASK_BITMAP_PATH);
	else
		SavePrintTaskBitmap_End();
	print_task_bitmap_context.enabled = enabled;
}

void SavePrintTaskBitmap_Begin(void)
{
	if (!print_task_bitmap_context.enabled)
		return;

	print_task_bitmap_hdr_t hdr;
	char seqnum[16];
	time_t t;

	t = time(NULL);
	if (t == print_task_bitmap_context.last_time)
		print_task_bitmap_context.suffix++;
	else
		print_task_bitmap_context.suffix = 0;
	print_task_bitmap_context.last_time = t;

	sprintf(seqnum, "%d%d", (int)t, print_task_bitmap_context.suffix);
	sprintf(print_task_bitmap_context.pathname, PRINT_TASK_BITMAP_PATH "%s.bin", seqnum);

	if (print_task_bitmap_context.file)
		fclose(print_task_bitmap_context.file);
	print_task_bitmap_context.file = fopen(print_task_bitmap_context.pathname, "wb");
	if (print_task_bitmap_context.file) {
		memset(&hdr, 0, sizeof(hdr));
		hdr.depth = (Settings.BitsPerDot == 0) ? 1 : 8;
		hdr.bytes_per_line = BYTES_PER_LINE * hdr.depth;
		fwrite(&hdr, sizeof(hdr), 1, print_task_bitmap_context.file);
		fflush(print_task_bitmap_context.file);
	}
	else
		print_task_bitmap_context.pathname[0] = 0;
}

void SavePrintTaskBitmap_Append(const void *data, uint32_t len)
{
	if (print_task_bitmap_context.enabled && print_task_bitmap_context.file && len > 0)
		fwrite(data, 1, len, print_task_bitmap_context.file);
}

void SavePrintTaskBitmap_End(void)
{
	if (print_task_bitmap_context.file) {
		int pos = ftell(print_task_bitmap_context.file);
		fflush(print_task_bitmap_context.file);
		fclose(print_task_bitmap_context.file);
		print_task_bitmap_context.file = NULL;
		if (pos == sizeof(print_task_bitmap_hdr_t))
			remove(print_task_bitmap_context.pathname);
		print_task_bitmap_context.pathname[0] = 0;
	}
}

int CheckImage()
{
#define P (Settings.HeaderFooterImage.Header)
	char pathname[64];
	struct stat buffer;
	if(P.type == 1){
		uint8_t kc1 = P.kc1_n;
		uint8_t kc2 = P.kc2_m;
		if (kc1 < 32 || kc1 > 126)
			return -1;
		if (kc2 < 32 || kc2 > 126)
			return -1;
		sprintf(pathname, NV_GRAPHICS_DIR "%02x_%02x.bin", kc1, kc2);
		if (stat(pathname, &buffer) != 0){
			LogError("\"%s\" does not exist.", pathname);
			return -1;
		}
	}else if(P.type == 2){
		uint8_t n = P.kc1_n;
		if (n == 0)
			return -1;
		sprintf(pathname, "/data/PUB/image_%03u.bin", n);
		if (stat(pathname, &buffer) != 0){
			LogError("\"%s\" does not exist.", pathname);
			return -1;
		}
	}
	return 0;
#undef P
}

int PrintHeaderLogo(unsigned char motor_back)
{
#define P (Settings.HeaderFooterImage.Header)

	uint8_t alignment;

	if (P.type == 1 || P.type == 2) {
		/* Feed backwards */
		if (motor_back > 0 && P.dots_back > 0){
			ReverseFeed(P.dots_back);
		}
		if (CheckImage() == 0){ /*存在目标头图*/
			/* Set alignment */
			alignment = Settings.Alignment;
			Settings.Alignment = P.alignment;
			if (P.type == 1) { /* NV mono image */
				PrintNVMonoImage(
					P.kc1_n,
					P.kc2_m,
					1, 1);
			}
			else if (P.type == 2) { /* NV grayscale image */
				//灰阶图片添加一个0a走纸，否则步进电机会打滑
				feed_one_line();
				PrintNVGrayscaleImage(
					P.kc1_n,
					0, 100,
					P.kc2_m);
			}
			/* Restore alignment */
			Settings.Alignment = alignment;
			return 0;
		}

	}

	return -1;

#undef P
}

int PrintFooterLogo(void)
{
#define P (Settings.HeaderFooterImage.Footer)

	uint8_t alignment;

	if (P.type == 1 || P.type == 2) {
		/* Set alignment */
		alignment = Settings.Alignment;
		Settings.Alignment = P.alignment;
		if (P.type == 1) { /* NV mono image */
			PrintNVMonoImage(
				P.kc1_n,
				P.kc2_m,
				1, 1);
		}
		else if (P.type == 2) { /* NV grayscale image */
			PrintNVGrayscaleImage(
				P.kc1_n,
				0, 100,
				P.kc2_m);
		}
		/* Restore alignment */
		Settings.Alignment = alignment;
		return 0;
	}

	return -1;

#undef P
}

void RuntimeDataInit(void)
{
	uint32_t i, pos;

	memset(DataBuffer, 0, DATA_BUFFER_SIZE);

	Settings.LeftMargin = 0;							/* GS L */
	Settings.PrintAreaWidth = DOTS_PER_LINE;			/* GS W */
	Settings.LineSpacing[0] = 30;						/* ESC 2, ESC 3 */
	Settings.LineSpacing[1] = 30;

	Settings.LeftCharSpacing[0] = 0;
	Settings.RightCharSpacing[0][0] = 0;				/* ESC SP */
	Settings.RightCharSpacing[1][0] = 0;
	Settings.CharHSize[0] = 1;							/* ESC !, GS ! */
	Settings.CharVSize[0] = 1;							/* ESC !, GS ! */
	Settings.UnderlineMode[0] = 0;						/* ESC !, ESC - */
	Settings.EmphasizedMode[0] = 0;						/* ESC !, ESC E, ESC G */
	Settings.UserDefEnabled[0] = 0;						/* ESC % */

	Settings.LeftCharSpacing[1] = 0;					/* FS S */
	Settings.RightCharSpacing[0][1] = 0;				/* FS S */
	Settings.RightCharSpacing[1][1] = 0;
	Settings.CharHSize[1] = 1;							/* ESC !, GS !, FS !, FS W */
	Settings.CharVSize[1] = 1;							/* ESC !, GS !, FS !, FS W */
	Settings.UnderlineMode[1] = 0;						/* ESC !, ESC -, FS - */
	Settings.EmphasizedMode[1] = 0;						/* ESC !, ESC E, ESC G */
	Settings.UserDefEnabled[1] = 0;

	Settings.InternationalCharSet = 0;					/* ESC R */
	Settings.CJKMode = 1;								/* FS &, FS . */

	Settings.Mode = STANDARD_MODE;
	Settings.Alignment = 0;								/* ESC a */
	Settings.ClockwiseRotationMode = 0;					/* ESC V */
	Settings.UpsideDownMode = 0;						/* ESC { */
	Settings.BlackWhiteReverseMode = 0;					/* GS B */
	Settings.RightToLeftMode = 0;
	Settings.TSPLMode = 0;
	Settings.Color = 0;									/* ESC r */

	memset(Settings.TabPos, 0, sizeof(Settings.TabPos));
	for (i = 0; i < 32; i++) {
		pos = (i + 1) * 12 * 8;
		if (pos > DOTS_PER_LINE)
			break;
		Settings.TabPos[i] = pos;
	}

	Settings.leftMargin = leftMargin;
	Settings.printAreaWidth = printAreaWidth;
	Settings.rightMargin = rightMargin;
	Settings.lineSpacing = lineSpacing;
	Settings.rightCharSpacing = rightCharSpacing;
	Settings.isClockwiseRotation = isClockwiseRotation;
	Settings.isUpsideDown = isUpsideDown;

	memset(&Barcode, 0, sizeof(Barcode));
	Barcode.ModuleSize = 3;
	Barcode.Height = 162;
	Barcode.HRIPos = 2;
	Barcode.HRIFont = 0;
	Barcode.SymbolWidth = 0;

	memset(&PDF417, 0, sizeof(PDF417));
	PDF417.ModuleSize = 3;
	PDF417.Columns = 0;
	PDF417.Rows = 0;
	PDF417.RowHeight = 6;
	PDF417.ECLevel = 50;
	PDF417.Options = 0;
	PDF417.SymbolRows = 0;
	PDF417.SymbolWidth = 0;

	memset(&QRCode, 0, sizeof(QRCode));
	QRCode.ModuleSize = 4;
	QRCode.Model = 0;
	QRCode.ECLevel = 0;
	QRCode.SymbolSize = 0;

	memset(&DownloadedBitImage, 0, sizeof(DownloadedBitImage));

	lineBuffer->clear();
	lineBuffer->updateLineStride();

	pageBuffer->destroy();
	pageBuffer->resetPrintArea();
	pageBuffer->resetDirection();

	ResetNextFun();
}

void PersistentDataInit(void)
{
	uint8_t density, maxspeed, temp;
	uint8_t buf[4];

	load_uint8("ASCII_WordSet", &Settings.ASC_WordSet,  0);
	load_uint8("CJK_WordSet",   &Settings.CJK_WordSet,  0);
	load_uint8("CodePage",      &Settings.CodePage,     0);
	load_uint8("Utf8_WordSet",  &Settings.Utf8_WordSet, 0);
	load_uint8("Locale",        &Settings.Locale,       0);
	load_uint8("ASCII_WordSet", &print_log_config.asc_wordset,  0);
	load_uint8("CJK_WordSet",   &print_log_config.cjk_wordset,  0);
	load_uint8("CodePage",      &print_log_config.code_page,    0);
	load_uint8("Utf8_WordSet",  &print_log_config.utf8_wordset, 0);
	load_uint8("Locale",        &print_log_config.locale,       0);
	load_uint8("BitsPerDot", &temp, 0);
	print_log_config.fine_mode = temp;

	if (Settings.CodePage >= 64 || CODE_PAGE_MAPPINGS[Settings.CodePage] == 255)
		Settings.CodePage = 0;

	load_uint8("Density", &density, 100);
	if (density != 100)
		printer_set_density(density, 0);

	load_uint8("MaxSpeed", &maxspeed, 255);
	if (maxspeed != 255)
		printer_set_maxspeed(maxspeed, 0);

	load_int("PaperLayout.sa", &Settings.PaperLayout.sa, 48);
	load_int("PaperLayout.sb", &Settings.PaperLayout.sb, 0);
	load_int("PaperLayout.sc", &Settings.PaperLayout.sc, 0);
	load_int("PaperLayout.sd", &Settings.PaperLayout.sd, 0);
	load_int("PaperLayout.se", &Settings.PaperLayout.se, 0);
	load_int("PaperLayout.sf", &Settings.PaperLayout.sf, 0);
	load_int("PaperLayout.sg", &Settings.PaperLayout.sg, 0);
	load_int("PaperLayout.sh", &Settings.PaperLayout.sh, 0);

	load_int("FeedAndCutOnCoverClosed", &Settings.FeedAndCutOnCoverClosed, 160);
	load_uint8("BlackMarkLocation", &Settings.BlackMarkLocation, 1);

	load_int("FeedAndCutOnElectrify", &Settings.FeedAndCutOnElectrify, 0);
	load_int("CuttingAutoLogo", &Settings.CuttingAutoLogo, 0);
	
	load_uint8("CutOption", &temp, 0);
	Settings.CutOption = temp;
	load_uint8("BitsPerDot", &temp, 0);
	Settings.BitsPerDot = temp;
	lineBuffer->updateLineStride();
	if (Settings.BitsPerDot > 0)
		SendPrinterCommand1(0x0D, Settings.BitsPerDot);

	load_uint8("PaperSizeAdaptive", &Settings.PaperSizeAdaptive, 0);
	load_uint16("PrintHorizontalAccuracy", &Settings.PrintHorizontalAccuracy, 0);
	if(Settings.PrintHorizontalAccuracy == 0){
		Settings.FixedLeftMargin = 0;
		Settings.FixedPrintAreaWidth = DOTS_PER_LINE;
	}else if(Settings.PrintHorizontalAccuracy == 1){
		Settings.FixedLeftMargin = DOTS_PER_LINE / 16;
		Settings.FixedPrintAreaWidth = DOTS_PER_LINE - DOTS_PER_LINE / 8;
	}
	load_uint8("PrintVerticalAccuracy", &temp, 0);
	switch (temp) {
		case 1:
			Settings.PrintVerticalAccuracy = 180;
			break;
		case 2:
			Settings.PrintVerticalAccuracy = 360;
			break;
		case 3:
			Settings.PrintVerticalAccuracy = 406;
			break;
		default:
			Settings.PrintVerticalAccuracy = 203;
			break;
	}
	load_uint8("DualColor.Enabled", &Settings.DualColor.Enabled, 0);
	load_uint16("DualColor.StdPrtEng", &Settings.DualColor.StdPrtEng, 1200);
	load_uint16("DualColor.BlackRatio", &Settings.DualColor.BlackRatio, 340);
	if (Settings.DualColor.Enabled) {
		/* 双色模式必须打开精细模式 */
		Settings.BitsPerDot = 2;
		/* 打印驱动使用4阶灰阶打印 */
		buf[0] = 0x0D;
		buf[1] = 0x03;
		printer_send_command(buf, 2);
		/* 打印驱动打开双色模式 */
		buf[0] = 0x0E;
		buf[1] = 0x01;
		printer_send_command(buf, 2);
		/* 打印驱动设置标准打印能量 */
		buf[0] = 0x82;
		*((uint16_t *)(buf + 1)) = Settings.DualColor.StdPrtEng;
		printer_send_command(buf, 3);
		/* 打印驱动设置黑色比例 */
		buf[0] = 0x0F;
		*((uint16_t *)(buf + 1)) = Settings.DualColor.BlackRatio;
		printer_send_command(buf, 3);
	}

	load_uint32("HeaderFooterImage.Header", (uint32_t *)&Settings.HeaderFooterImage.Header, 0);
	load_uint32("HeaderFooterImage.Footer", (uint32_t *)&Settings.HeaderFooterImage.Footer, 0);
}
