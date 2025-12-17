#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zint.h>
#include "libcommon.h"
#include "printer.h"
#include "print_queue.h"
#include "Config.h"
#include "HarfBuzz.h"
#include "Instruct_Proc.h"
#include "LineBuffer.h"
#include "PageBuffer.h"
#include "Runtime_Data.h"
#include "Util.h"
#include "WordSet.h"
#include "GS_Instruct.h"

#define IMG_HEAD_LEN  16
#define IMG_MAX_LEN  583680    //sigle img(include img head) max size: 778240/4*3 byte = 570KB

typedef struct {
	uint32_t src_offset;
	uint32_t dst_offset;
	uint32_t copy_size;
	uint32_t offset;
} shift_copy_context_t;

static uint8_t linebuf[DOTS_PER_LINE_MAX];
static uint8_t *imagedata = NULL;
static shift_copy_context_t shift_copy_context;
static int Fd = -1;

/*----------------------------------------------*
 | GS !                                         |
 *----------------------------------------------*/
static void SetCharSize(void)
{
	Settings.CharHSize[0] = ((Cmd_Data & 0x70) >> 4) + 1;
	Settings.CharHSize[1] = Settings.CharHSize[0];
	Settings.CharVSize[0] = ((Cmd_Data & 0x07)     ) + 1;
	Settings.CharVSize[1] = Settings.CharVSize[0];

	ResetNextFun();
}

/*----------------------------------------------*
 | GS $                                         |
 *----------------------------------------------*/
static void SetPageModeAbsoluteVerticalPrintPosition(void)
{
	switch (Cmd_List0++) {
	case 0:
		Cmd_List1  = Cmd_Data;
		return;
	case 1:
		Cmd_List1 |= Cmd_Data << 8;
		uint32_t dotsToFeed = (203 * Cmd_List1) / Settings.PrintVerticalAccuracy;
		if (Settings.Mode == PAGE_MODE) {
			pageBuffer->setDirection(pageBuffer->Direction);
			pageBuffer->feed(dotsToFeed);
		}
		break;
	default:
		break;
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( A pL pH n m                             |
 *----------------------------------------------*/
static void HexDumpMode(void)
{
	static uint8_t ascii[16];
	uint32_t h, l, i, c;

	h = (Cmd_Data >> 4) & 0x0F;
	if (h < 10)
		h += 48;
	else
		h += 55;

	l = (Cmd_Data     ) & 0x0F;
	if (l < 10)
		l += 48;
	else
		l += 55;

	ASC_Proc(h);
	ASC_Proc(l);
	ASC_Proc(0x20);

	if (Cmd_Data >= 0x20 && Cmd_Data < 0x7F)
		ascii[Cmd_List0] = Cmd_Data;
	else
		ascii[Cmd_List0] = '.';

	c = (DOTS_PER_LINE / 12) >> 2; /* 一点行可以容纳的字节数 */

	if (++Cmd_List0 == c) {
		for (i = 0; i < c; i++)
			ASC_Proc(ascii[i]);
		Cmd_List0 = 0;
		lineBuffer->printLine();
	}
}

static void PrintTestPage(void)
{
	send_key_mqmsg(VIRTUAL_KEY_TESTPAGE, 2);
}

static void GS_LP_A_m(void)
{
	uint8_t buf[64];
	uint32_t i, start, count;

	if (Settings.Mode == STANDARD_MODE && lineBuffer->isEmpty()) {
		if (Cmd_Data == 1 || Cmd_Data == 49) {
			RuntimeDataInit();
			Cmd_List0 = 0;
			pNextFun = HexDumpMode;
			return;
		}
		if (Cmd_Data == 2 || Cmd_Data == 50) {
			PrintTestPage();
		}
		else if (Cmd_Data == 3 || Cmd_Data == 51) {
			start = 0x20;
			count = DOTS_PER_LINE / 12;
			while ((start + count) < 128) {
				for (i = 0; i < count; i++)
					buf[i] = start + i;
				print_channel_send_data(PRINT_CHN_ID_INT, buf, i, 0);
				start++;
			};
			for (i = 0; i < 6; i++)
				buf[i] = 0x0A;
			print_channel_send_data(PRINT_CHN_ID_INT, buf, i, 0);
		}
	}

	ResetNextFun();
}

static void GS_LP_A_n(void)
{
	Cmd_List3 = Cmd_Data;

	pNextFun = GS_LP_A_m;
}

static void GS_LP_A_pH(void)
{
	Cmd_List2 |= Cmd_Data << 8;

	pNextFun = GS_LP_A_n;
}

static void GS_LP_A_pL(void)
{
	Cmd_List2 = Cmd_Data;

	pNextFun = GS_LP_A_pH;
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 1>                 |
 *----------------------------------------------*/
static void GS_LP_E_01_dx(void)
{
	Cmd_List0 |= Cmd_Data << (Cmd_List1 << 3);
	if (++Cmd_List1 < Cmd_Len)
		return;

	if (Settings.Mode == STANDARD_MODE && lineBuffer->isEmpty() && Cmd_List0 == 0x4E49) {
		Settings.Mode = USER_SETTING_MODE;
		LogInfo("Enter user setting mode");
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 2>                 |
 *----------------------------------------------*/
static void GS_LP_E_02_dx(void)
{
	Cmd_List0 |= Cmd_Data << (Cmd_List1 << 3);
	if (++Cmd_List1 < Cmd_Len)
		return;

	if (Settings.Mode == USER_SETTING_MODE && Cmd_List0 == 0x54554F) {
		LogInfo("Exit user setting mode");
		SysDelay(500);
		system("reboot");
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 5>                 |
 *----------------------------------------------*/
static void SetCustomizedSettingValues(void)
{
	uint32_t i = 0;

	while (Cmd_Len >= 3) {
		switch (DataBuffer[i]) {
		case 5:
			printer_set_density(DataBuffer[i + 1], 1);
			break;
		case 6:
			printer_set_maxspeed(DataBuffer[i + 1], 1);
			break;
		case 65:
			if (DataBuffer[i + 1] == 0)
				app_setting_set_int(PRINTER_SECTION, "ASCII_WordSet", DataBuffer[i + 2]);
			else if (DataBuffer[i + 1] == 1)
				app_setting_set_int(PRINTER_SECTION, "CJK_WordSet",   DataBuffer[i + 2]);
			else if (DataBuffer[i + 1] == 2)
				app_setting_set_int(PRINTER_SECTION, "CodePage",      DataBuffer[i + 2]);
			else if (DataBuffer[i + 1] == 3)
				app_setting_set_int(PRINTER_SECTION, "Utf8_WordSet",  DataBuffer[i + 2]);
			else if (DataBuffer[i + 1] == 4)
				app_setting_set_int(PRINTER_SECTION, "BitsPerDot",    DataBuffer[i + 2]);
			break;
		default:
			break;
		}
		i += 3;
		Cmd_Len -= 3;
	}
}

static void GS_LP_E_05_data(void)
{
	DataBuffer[Cmd_List0++] = Cmd_Data;

	if (Cmd_List0 < Cmd_Len)
		return;

	SetCustomizedSettingValues();

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 6>                 |
 *----------------------------------------------*/
static void GS_LP_E_06_n(void)
{
	switch (Cmd_List1) {
	case 0:
		if (Cmd_Data <= 1)
			Settings.ASC_WordSet = Cmd_Data;
		else if (Cmd_Data == 255)
			Settings.ASC_WordSet = app_setting_get_int(PRINTER_SECTION, "ASC_WordSet", 0);
		break;
	case 1:
		if (Cmd_Data == 0  || Cmd_Data == 1  ||
			Cmd_Data == 11 || Cmd_Data == 12 ||
			Cmd_Data == 21 || Cmd_Data == 128)
			Settings.CJK_WordSet = Cmd_Data;
		else if (Cmd_Data == 255)
			Settings.CJK_WordSet = app_setting_get_int(PRINTER_SECTION, "CJK_WordSet", 0);
		break;
	case 2:
		if (Cmd_Data < 64 && CODE_PAGE_MAPPINGS[Cmd_Data] != 255)
			Settings.CodePage = Cmd_Data;
		else if (Cmd_Data == 255)
			Settings.CodePage = app_setting_get_int(PRINTER_SECTION, "CodePage", 0);
		break;
	case 3:
		if (Cmd_Data <= 1)
			Settings.Utf8_WordSet = Cmd_Data;
		else if (Cmd_Data == 255)
			Settings.Utf8_WordSet = app_setting_get_int(PRINTER_SECTION, "Utf8_WordSet", 0);
		break;
	case 4:
		if (!lineBuffer->isEmpty())
			break;
		if (Cmd_Data <= 7)
			Settings.BitsPerDot = Cmd_Data;
		else if (Cmd_Data == 255)
			Settings.BitsPerDot = app_setting_get_int(PRINTER_SECTION, "BitsPerDot", 0);
		else
			break;
		lineBuffer->updateLineStride();
		SendPrinterCommand1(0x0D, Settings.BitsPerDot);
		break;
	case 5:
		if (!lineBuffer->isEmpty())
			break;
		Settings.RightToLeftMode = (Cmd_Data & 1);
		if (Settings.RightToLeftMode == 0)
			lineBuffer->clearFlag(LINE_BUFFER_FLAG_LINE_RTL);
		else
			lineBuffer->setFlag(LINE_BUFFER_FLAG_LINE_RTL);
		break;
	case 6:
		if (Cmd_Data <= 2)
			Settings.PaperSizeAdaptive = Cmd_Data;
		else if (Cmd_Data == 255)
			Settings.PaperSizeAdaptive = app_setting_get_int(PRINTER_SECTION, "PaperSizeAdaptive", 0);
		break;
	case 7:
		if (Cmd_Data <= 1)
			Settings.PrintHorizontalAccuracy = Cmd_Data;
		if (Cmd_Data == 0){
			Settings.FixedLeftMargin = 0;
			Settings.FixedPrintAreaWidth = DOTS_PER_LINE;
		}else if (Cmd_Data == 1){
			Settings.FixedLeftMargin = DOTS_PER_LINE / 16;
			Settings.FixedPrintAreaWidth = DOTS_PER_LINE - DOTS_PER_LINE / 8;
		}
		break;
	case 8:
		switch (Cmd_Data) {
			case 0:
				Settings.PrintVerticalAccuracy = 203;
				break;
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
				break;
		}
		break;
	case 10:
	case 138:
		if (Cmd_Data < 4)
			Cmd_Data = 4;
		harfBuzz->setAsciiCharSize(Cmd_Data);
		if (Cmd_List1 > 128)
			harfBuzz->saveConf();
		break;
	case 11:
	case 139:
		if (Cmd_Data < 4)
			Cmd_Data = 4;
		harfBuzz->setCjkCharSize(Cmd_Data);
		if (Cmd_List1 > 128)
			harfBuzz->saveConf();
		break;
	case 12:
	case 140:
		if (Cmd_Data < 4)
			Cmd_Data = 4;
		harfBuzz->setOtherCharSize(Cmd_Data);
		if (Cmd_List1 > 128)
			harfBuzz->saveConf();
		break;
	case 18:
	case 146:
		Cmd_Data &= 0xFE;
		if (Cmd_Data < 6)
			Cmd_Data = 6;
		else if (Cmd_Data > 160)
			Cmd_Data = 160;
		harfBuzz->setAsciiCharSize(Cmd_Data >> 1);
		harfBuzz->setCjkCharSize(Cmd_Data);
		harfBuzz->setOtherCharSize(Cmd_Data);
		if (Cmd_List1 > 128)
			harfBuzz->saveConf();
		break;
	case 20:
	case 148:
		harfBuzz->setAsciiCharEnabledState((Cmd_Data > 0) ? true : false);
		if (Cmd_List1 > 128)
			harfBuzz->saveConf();
		if (Cmd_Data >= 128)
			harfBuzz->enableThirdPartyFace(0, Cmd_Data - 128);
		else
			harfBuzz->disableThirdPartyFace(0);
		break;
	case 21:
	case 149:
		harfBuzz->setCjkCharEnabledState((Cmd_Data > 0) ? true : false);
		if (Cmd_List1 > 128)
			harfBuzz->saveConf();
		if (Cmd_Data >= 128)
			harfBuzz->enableThirdPartyFace(1, Cmd_Data - 128);
		else
			harfBuzz->disableThirdPartyFace(1);
		break;
	case 22:
	case 150:
		if (Cmd_Data >= 128)
			harfBuzz->enableThirdPartyFace(2, Cmd_Data - 128);
		else
			harfBuzz->disableThirdPartyFace(2);
		break;
	case 30:
	case 158:
		Settings.Locale = Cmd_Data;
		if (Cmd_List1 > 128)
			app_setting_set_int(PRINTER_SECTION, "Locale", Settings.Locale);
		harfBuzz->loadConf();
		break;
	case 255:
		if (Cmd_Data == 255) {
			harfBuzz->resetFontConf();
			harfBuzz->loadConf();
		}
		break;
	default:
		break;
	}

	ResetNextFun();
}

static void GS_LP_E_06_m(void)
{
	Cmd_List1 = Cmd_Data;

	pNextFun = GS_LP_E_06_n;
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 7>                 |
 *----------------------------------------------*/
static void GS_LP_E_07_n(void)
{
	if (Cmd_Data == 255)
		Cmd_Data = app_setting_get_int(PRINTER_SECTION, "Density", 100);
	printer_set_density(Cmd_Data, 0);

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 8>                 |
 *----------------------------------------------*/
static void GS_LP_E_08_n(void)
{
	if (Cmd_Data == 255)
		Cmd_Data = app_setting_get_int(PRINTER_SECTION, "MaxSpeed", 255);
	printer_set_maxspeed(Cmd_Data, 0);

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 9>                 |
 *----------------------------------------------*/
/*
 * Cmd_List0: count
 * Cmd_List1: size
 */
static void GS_LP_E_09_data(void)
{
	DataBuffer[Cmd_List0++] = Cmd_Data;
	Cmd_List1--;

	if (Cmd_List0 == DATA_BUFFER_SIZE || (Cmd_List1 == 0 && Cmd_List0 > 0)) {
		if (Fd > 0) {
			write(Fd, DataBuffer, Cmd_List0);
			fsync(Fd);
		}
		Cmd_List0 = 0;
	}

	if (Cmd_List1 == 0) {
		if (Fd > 0) {
			close(Fd);
			Fd = -1;
			LogInfo("File closed.");
		}
		ResetNextFun();
	}
}

/*
 * Cmd_List0: n
 * Cmd_List1: x
 * Cmd_List2: y
 */
static void GS_LP_E_09_yH(void)
{
	char pathname[64];
	uint32_t size;

	Cmd_List2 |= Cmd_Data << 8;
	size = Cmd_List1 * Cmd_List2;

	if (size > 0) {
		sprintf(pathname, "/data/PUB/grayscale_%03u.bin", Cmd_List0);
		Fd = open(pathname, O_CREAT|O_WRONLY|O_TRUNC, 0644);
		if (Fd >= 0) {
			LogInfo("File opened (width=%u, height=%u).", Cmd_List1, Cmd_List2);
			write(Fd, &Cmd_List1, 2); /* width */
			write(Fd, &Cmd_List2, 2); /* height */
		}
		Cmd_List0 = 0;
		Cmd_List1 = size;
		pNextFun = GS_LP_E_09_data;
		print_task_set_flag(0, TASK_FLAG_DONT_REPRINT);
	}
	else
		ResetNextFun();
}

static void GS_LP_E_09_yL(void)
{
	Cmd_List2 = Cmd_Data;

	pNextFun = GS_LP_E_09_yH;
}

static void GS_LP_E_09_xH(void)
{
	Cmd_List1 |= Cmd_Data << 8;

	pNextFun = GS_LP_E_09_yL;
}

static void GS_LP_E_09_xL(void)
{
	Cmd_List1 = Cmd_Data;

	pNextFun = GS_LP_E_09_xH;
}

static void GS_LP_E_09_n(void)
{
	Cmd_List0 = Cmd_Data;

	pNextFun = GS_LP_E_09_xL;
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 10>                |
 *----------------------------------------------*/
/*
 * Cmd_List0: count
 * Cmd_List1: size
 * Cmd_List2: x
 */
static void GS_LP_E_0A_data(void)
{
	linebuf[Cmd_List0++] = Cmd_Data;
	Cmd_List1--;

	if (Cmd_List0 < Cmd_List2 && Cmd_List1 > 0)
		return;
	if (Cmd_List0 == Cmd_List2)
		Cmd_List0 = 0;

	memcpy(DataBuffer + shift_copy_context.offset + shift_copy_context.dst_offset,
		linebuf + shift_copy_context.src_offset,
		shift_copy_context.copy_size);
	shift_copy_context.offset += DOTS_PER_LINE;
	if (shift_copy_context.offset == BYTES_PER_CHAR_ROW || Cmd_List1 == 0) {
		PrnDotLine(DataBuffer, shift_copy_context.offset, PRN_DATA_GRAYSCALE);
		shift_copy_context.offset = 0;
		memset(DataBuffer, (Cmd_List4 > 0) ? 0xFF : 0, sizeof(DataBuffer));
	}

	if (Cmd_List1 == 0) {
		if (Cmd_List4 > 0) /* 退出灰阶模式 */
			SendPrinterCommand1(0x04, 0);
		LogDbg("Complete.");
		ResetNextFun();
	}
}

/*
 * Cmd_List0: n(=0)
 * Cmd_List1: s
 * Cmd_List2: x
 * Cmd_List3: y
 * Cmd_List4: m
 */
static void GS_LP_E_0A_yH(void)
{
	uint32_t size, i;

	Cmd_List3 |= Cmd_Data << 8;
	size = Cmd_List2 * Cmd_List3;

	if (size == 0) {
		ResetNextFun();
		return;
	}
#define P (Settings.HeaderFooterImage.Header)
	if (P.dots_back > 0 || Settings.lineSpacing() == 0)
		lineBuffer->print(1); /*防止回退后走纸方向未改变，导致反向打印*/
#undef P
	if (Cmd_List4 > 0) /* 进入灰阶模式 */
		SendPrinterCommand1(0x04, Cmd_List4);

	shift_copy_context.src_offset = 0;
	shift_copy_context.dst_offset = 0;
	shift_copy_context.copy_size = Cmd_List2;
	shift_copy_context.offset = 0;

	memset(DataBuffer, (Cmd_List4 > 0) ? 0xFF : 0, sizeof(DataBuffer));

	i = (Cmd_List1 & 0x7FFF);
	if (i > 0 && i < DOTS_PER_LINE) {
		if (Cmd_List1 & 0x8000) { /* Shift left */
			shift_copy_context.src_offset = i;
			shift_copy_context.dst_offset = 0;
			shift_copy_context.copy_size = (Cmd_List2 - i);
		}
		else { /* Shift right */
			shift_copy_context.src_offset = 0;
			shift_copy_context.dst_offset = i;
			shift_copy_context.copy_size = (Cmd_List2 < (DOTS_PER_LINE - i)) ? Cmd_List1 : (DOTS_PER_LINE - i);
		}
	}

	Cmd_List0 = 0;
	Cmd_List1 = size;
	pNextFun = GS_LP_E_0A_data;
	print_task_set_flag(0, TASK_FLAG_DONT_REPRINT);
}

static void GS_LP_E_0A_yL(void)
{
	Cmd_List3 = Cmd_Data;

	pNextFun = GS_LP_E_0A_yH;
}

static void GS_LP_E_0A_xH(void)
{
	Cmd_List2 |= Cmd_Data << 8;

	pNextFun = GS_LP_E_0A_yL;
}

static void GS_LP_E_0A_xL(void)
{
	Cmd_List2 = Cmd_Data;

	pNextFun = GS_LP_E_0A_xH;
}

/*
 * Cmd_List0: n
 * Cmd_List1: s
 */
static void GS_LP_E_0A_m(void)
{
	char pathname[64];
	uint8_t buf[4];
	uint32_t i, width, height, offset;
	uint32_t src_offset, dst_offset, copy_size;
	int fd = -1, ret;

	if (Cmd_Data > 8)
		goto _exit;

	if (Cmd_List0 == 0) {
		Cmd_List4 = Cmd_Data;
		pNextFun = GS_LP_E_0A_xL;
		return;
	}

	sprintf(pathname, "/data/PUB/grayscale_%03u.bin", Cmd_List0);
	fd = open(pathname, O_RDONLY);
	if (fd < 0) {
		LogError("Failed to open \"%s\".", pathname);
		goto _exit;
	}

	ret = read(fd, buf, 4);
	if (ret != 4) {
		LogError("Reading file failed.");
		goto _exit;
	}
#define P (Settings.HeaderFooterImage.Header)
	if (P.dots_back > 0 || Settings.lineSpacing() == 0)
		lineBuffer->print(1); /*防止回退后走纸方向未改变，导致反向打印*/
#undef P
	/* 进入灰阶模式 */
	SendPrinterCommand1(0x04, Cmd_Data);

	width  = buf[1] << 8 | buf[0];
	height = buf[3] << 8 | buf[2];

	src_offset = 0;
	dst_offset = 0;
	copy_size = width;

	i = (Cmd_List1 & 0x7FFF);
	if (i > 0 && i < DOTS_PER_LINE) {
		if (Cmd_List1 & 0x8000) { /* Shift left */
			src_offset = i;
			dst_offset = 0;
			copy_size = (width - i);
		}
		else { /* Shift right */
			src_offset = 0;
			dst_offset = i;
			copy_size = (width < (DOTS_PER_LINE - i)) ? width : (DOTS_PER_LINE - i);
		}
	}

	offset = 0;
	memset(DataBuffer, 0xFF, sizeof(DataBuffer));

	for (i = 0; i < height; i++) {
		read(fd, linebuf, width);
		memcpy(DataBuffer + offset + dst_offset, linebuf + src_offset, copy_size);
		offset += DOTS_PER_LINE;
		if (offset == BYTES_PER_CHAR_ROW) {
			PrnDotLine(DataBuffer, offset, PRN_DATA_GRAYSCALE);
			offset = 0;
			memset(DataBuffer, 0xFF, sizeof(DataBuffer));
		}
	}
	if (offset > 0)
		PrnDotLine(DataBuffer, offset, PRN_DATA_GRAYSCALE);

	/* 退出灰阶模式 */
	SendPrinterCommand1(0x04, 0);
	LogDbg("Complete.");

_exit:
	if (fd > 0)
		close(fd);
	ResetNextFun();
}

static void GS_LP_E_0A_sH(void)
{
	Cmd_List1 |= Cmd_Data << 8;

	pNextFun = GS_LP_E_0A_m;
}

static void GS_LP_E_0A_sL(void)
{
	Cmd_List1 = Cmd_Data;

	pNextFun = GS_LP_E_0A_sH;
}

static void GS_LP_E_0A_n(void)
{
	Cmd_List0 = Cmd_Data;

	pNextFun = GS_LP_E_0A_sL;
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 11>                |
 *----------------------------------------------*/
static void GS_LP_E_0B_n(void)
{
	uint8_t buf[8];

	if (Cmd_Data == 1) {
		Settings.BitsPerDot = 2;
		app_setting_set_int(PRINTER_SECTION, "BitsPerDot", 2);

		buf[0] = 0x0D;
		buf[1] = 0x03;
		printer_send_command(buf, 2);

		buf[0] = 0x0E;
		buf[1] = 0x01;
		printer_send_command(buf, 2);
		app_setting_set_int(PRINTER_SECTION, "DualColor.Enabled", 1);
	}
	else {
		buf[0] = 0x0D;
		buf[1] = (Settings.BitsPerDot > 0) ? 0x02 : 0x00;
		printer_send_command(buf, 2);

		buf[0] = 0x0E;
		buf[1] = 0x00;
		printer_send_command(buf, 2);
		app_setting_set_int(PRINTER_SECTION, "DualColor.Enabled", 0);
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 12>                |
 *----------------------------------------------*/
static void GS_LP_E_0C_n(void)
{
	uint8_t buf[4];

	if (Cmd_List0++ == 0) {
		Cmd_List1 = Cmd_Data;
		return;
	}

	buf[0] = 0x82;
	*((uint16_t *)(buf + 1)) = (Cmd_Data << 8) | Cmd_List1;
	printer_send_command(buf, 3);
	app_setting_set_int(PRINTER_SECTION, "DualColor.StdPrtEng", *((uint16_t *)(buf + 1)));

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 13>                |
 *----------------------------------------------*/
static void GS_LP_E_0D_n(void)
{
	uint8_t buf[4];

	if (Cmd_List0++ == 0) {
		Cmd_List1 = Cmd_Data;
		return;
	}

	buf[0] = 0x0F;
	*((uint16_t *)(buf + 1)) = (Cmd_Data << 8) | Cmd_List1;
	printer_send_command(buf, 3);
	app_setting_set_int(PRINTER_SECTION, "DualColor.BlackRatio", *((uint16_t *)(buf + 1)));

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 14>                |
 *----------------------------------------------*/
/*
 * Cmd_List0: n
 * Cmd_List1: count
 * Cmd_List2: size
 * Cmd_List3: width
 * Cmd_List4: height
 */
static void GS_LP_E_0E_data(void)
{
	char pathname[64];
	int fd;
	uint32_t remain;
	uint32_t ret = 0;
#define PER_TX_LEN  (1024 * 100)

	if (imagedata)
		imagedata[Cmd_List1++] = Cmd_Data;

	if (--Cmd_List2 > 0)
		return;

	if (!imagedata)
		goto _exit;

	AutoLevel(imagedata + 16, Cmd_List3, Cmd_List4);
	AutoContrast(imagedata + 16, Cmd_List3, Cmd_List4);

	sprintf(pathname, "/data/PUB/image_%03u.bin", Cmd_List0);
	fd = open(pathname, O_CREAT|O_WRONLY|O_TRUNC, 0644);
	if (fd < 0)
		goto _exit;
	remain = Cmd_List1;
	while (remain > 0) {
		ret = write(fd, imagedata + (Cmd_List1 - remain), remain > PER_TX_LEN ? PER_TX_LEN : remain);
		remain -= ret;
		fsync(fd);
	}
	close(fd);

	free(imagedata);
	imagedata = NULL;

	LogInfo("File %s written (size=%u).", pathname, Cmd_List1);

_exit:
	ResetNextFun();
}

static void GS_LP_E_0E_head(void)
{
	if (Cmd_List5 == 0xFFFFFFFF)    //Receiving image data len > IMG_MAX_LEN - IMG_HEAD_LEN
	{
		if (--Cmd_List2 == 0)
			ResetNextFun();
		return;
	}

	linebuf[Cmd_List1++] = Cmd_Data;

	if (Cmd_List1 < 16)
		return;

	Cmd_List3 = (uint16_t)linebuf[1] << 8 | (uint16_t)linebuf[0];
	Cmd_List4 = (uint16_t)linebuf[3] << 8 | (uint16_t)linebuf[2];
	Cmd_List2 = Cmd_List3 * Cmd_List4;

	if (Cmd_List2 > 0) {
		if(Cmd_List2 > IMG_MAX_LEN - IMG_HEAD_LEN)
		{
			LogInfo("Receiving image (width=%u, height=%u, len=%u > max_len=%u Will be discarded!).", Cmd_List3, Cmd_List4, Cmd_List2, IMG_MAX_LEN - IMG_HEAD_LEN);
			Cmd_List5 = 0xFFFFFFFF;
			print_task_set_flag(0, TASK_FLAG_DONT_REPRINT);
			return;
		}
		imagedata = (uint8_t *)malloc(Cmd_List2 + 16);
		if (imagedata) {
			LogInfo("Receiving image (width=%u, height=%u, depth=%u).", Cmd_List3, Cmd_List4, linebuf[4]);
			memcpy(imagedata, linebuf, 16);
			Cmd_List1 = 16;
		}
		pNextFun = GS_LP_E_0E_data;
		print_task_set_flag(0, TASK_FLAG_DONT_REPRINT);
	}
	else
		ResetNextFun();
}

static void GS_LP_E_0E_n(void)
{
	Cmd_List0 = Cmd_Data;
	Cmd_List1 = 0;

	pNextFun = GS_LP_E_0E_head;
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 15>                |
 *----------------------------------------------*/
/*
 * Cmd_List3: n
 * Cmd_List4: s
 * Cmd_List5: g
 * Cmd_Data: m
 */
static void GS_LP_E_0F_m(void)
{
	PrintNVGrayscaleImage(Cmd_List3, Cmd_List4, Cmd_List5, Cmd_Data);
	ResetNextFun();
}

static void GS_LP_E_0F_g(void)
{
	Cmd_List5 = Cmd_Data;

	pNextFun = GS_LP_E_0F_m;
}

static void GS_LP_E_0F_sH(void)
{
	Cmd_List4 |= Cmd_Data << 8;

	pNextFun = GS_LP_E_0F_g;
}

static void GS_LP_E_0F_sL(void)
{
	Cmd_List4 = Cmd_Data;

	pNextFun = GS_LP_E_0F_sH;
}

static void GS_LP_E_0F_n(void)
{
	Cmd_List3 = Cmd_Data;

	pNextFun = GS_LP_E_0F_sL;
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 16>                |
 *----------------------------------------------*/
static void GS_LP_E_10_n(void)
{
	if (Cmd_Data == 255)
		Cmd_Data = app_setting_get_int(PRINTER_SECTION, "CutOption", 0);
	if (Cmd_Data <= 3)
		Settings.CutOption = Cmd_Data;

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 17>                |
 *----------------------------------------------*/
static void GS_LP_E_11_n(void)
{
	switch (Cmd_Data) {
	case 0:
		SavePrintTaskBitmap_SetEnabledState(false);
		break;
	case 1:
		SavePrintTaskBitmap_SetEnabledState(true);
		break;
	default:
		break;
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 18>                |
 *----------------------------------------------*/
static void GS_LP_E_12_text(void)
{
	if (Cmd_Data) {
		if (Cmd_List0 < DATA_BUFFER_SIZE)
			DataBuffer[Cmd_List0++] = Cmd_Data;
		return;
	}

	if (Cmd_List0 == DATA_BUFFER_SIZE)
		Cmd_List0--;
	DataBuffer[Cmd_List0] = 0;
	AudioPlayText((char *)DataBuffer, Cmd_List1, Cmd_List2);

	ResetNextFun();
}

static void GS_LP_E_12_param(void)
{
	DataBuffer[Cmd_List0++] = Cmd_Data;
	if (Cmd_List0 < Cmd_Len)
		return;

	Cmd_List1 = *((uint16_t *)(DataBuffer    ));
	Cmd_List2 = *((uint16_t *)(DataBuffer + 2));

	Cmd_List0 = 0;
	pNextFun = GS_LP_E_12_text;
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 19>                |
 *----------------------------------------------*/
/*
 * Cmd_List0: count
 * Cmd_List1: size
 * Cmd_List2: x
 * Cmd_List3: y
 * Cmd_List4: m
 */
static void GS_LP_E_13_data(void)
{
	linebuf[Cmd_List4 + Cmd_List0++] = Cmd_Data;

	if (Cmd_List0 < Cmd_List2)
		return;
	Cmd_List0 = 0;
	Cmd_List1++;

	if (Settings.BitsPerDot > 0) {
		if (PrnDotLine(linebuf, DOTS_PER_LINE, PRN_DATA_GRAYSCALE) == PRN_BUF_FULL)
			RUNTIME_FLAG_SET(RTF_KERN_BUF_FULL);
	}
	memset(linebuf, 0, sizeof(linebuf));

	if (Cmd_List1 == Cmd_List3) {
		LogDbg("Complete.");
		ResetNextFun();
	}
}

/*
 * Cmd_List0: n(=0)
 * Cmd_List1: s(=0)
 * Cmd_List2: x
 * Cmd_List3: y
 * Cmd_List4: m
 */
static void GS_LP_E_13_yH(void)
{
	Cmd_List3 |= Cmd_Data << 8;

	if (Cmd_List2 == 0 || Cmd_List3 == 0) {
		ResetNextFun();
		return;
	}

	if (Cmd_List2 > DOTS_PER_LINE)
		Cmd_List2 = DOTS_PER_LINE;

	Cmd_List0 = 0; // 一行已接收的字节数
	Cmd_List1 = 0; // 已接收的行数
	Cmd_List4 = 0; // 一行数据的偏移
	if (Cmd_List2 < DOTS_PER_LINE) {
		if (Settings.Alignment == 1)
			Cmd_List4 = (DOTS_PER_LINE - Cmd_List2) / 2;
		else if (Settings.Alignment == 2)
			Cmd_List4 = (DOTS_PER_LINE - Cmd_List2);
	}
	memset(linebuf, 0, sizeof(linebuf));

	pNextFun = GS_LP_E_13_data;
	print_task_set_flag(0, TASK_FLAG_DONT_REPRINT);
}

static void GS_LP_E_13_yL(void)
{
	Cmd_List3 = Cmd_Data;

	pNextFun = GS_LP_E_13_yH;
}

static void GS_LP_E_13_xH(void)
{
	Cmd_List2 |= Cmd_Data << 8;

	pNextFun = GS_LP_E_13_yL;
}

static void GS_LP_E_13_xL(void)
{
	Cmd_List2 = Cmd_Data;

	pNextFun = GS_LP_E_13_xH;
}

static void GS_LP_E_13_m(void)
{
	Cmd_List4 = Cmd_Data;

	pNextFun = GS_LP_E_13_xL;
}

static void GS_LP_E_13_sH(void)
{
	Cmd_List1 |= Cmd_Data << 8;

	pNextFun = GS_LP_E_13_m;
}

static void GS_LP_E_13_sL(void)
{
	Cmd_List1 = Cmd_Data;

	pNextFun = GS_LP_E_13_sH;
}

static void GS_LP_E_13_n(void)
{
	Cmd_List0 = Cmd_Data;

	pNextFun = GS_LP_E_13_sL;
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 48>                |
 *----------------------------------------------*/
static void DeletePaperLayout(void)
{
	if (Settings.Mode != USER_SETTING_MODE) {
		LogError("Not in user setting mode.");
		return;
	}

#define P(name,val) \
do { \
	Settings.PaperLayout.name = val; \
	app_setting_set_int(PRINTER_SECTION, "PaperLayout." #name, Settings.PaperLayout.name); \
} while (0)

	P(sa, 48);
	P(sb, 0 );
	P(sc, 0 );
	P(sd, 0 );
	P(se, 0 );
	P(sf, 0 );
	P(sg, 0 );
	P(sh, 0 );

#undef P
}

static void GS_LP_E_30_dx(void)
{
	Cmd_List0 |= Cmd_Data << (Cmd_List1 << 3);
	if (++Cmd_List1 < Cmd_Len)
		return;

	if (Cmd_List0 == 0x524C43)
		DeletePaperLayout();

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 49>                |
 *----------------------------------------------*/
static void ParsePaperLayout(char *str)
{
	int i, v, s[9];
	char *p, *begin = str;
	bool ok;

	LogInfo("ParsePaperLayout: \"%s\"", str);

	if (Settings.Mode != USER_SETTING_MODE) {
		LogError("Not in user setting mode.");
		return;
	}

	s[8] = 65535; /* 第9个为可选的扩展参数 */

	for (i = 0; i < 9; i++) {
		p = strchr(begin, ';');
		if (!p)
			break;
		*p = '\0';
		if (begin[0]) {
			v = str_to_int(begin, 10, &ok);
			if (!ok) { /* 无效数值 */
				LogError("Invalid number \"%s\".", begin);
				break;
			}
			s[i] = v;
		}
		else
			s[i] = 65535; /* 不改变 */
		begin = p + 1;
	}
	if (i < 8)
		return;

#define P(name,ind) \
do { \
	if (s[ind] != 65535) { \
		LogInfo(#name ": %4d -> %-4d", Settings.PaperLayout.name, s[ind]); \
		Settings.PaperLayout.name = s[ind]; \
		app_setting_set_int(PRINTER_SECTION, "PaperLayout." #name, s[ind]); \
	} \
	else \
		LogInfo(#name ": (Not Changed)"); \
} while (0)

	P(sa, 0);
	P(sb, 1);
	P(sc, 2);
	P(sd, 3);
	P(se, 4);
	P(sf, 5);
	P(sg, 6);
	P(sh, 7);

#undef P

	if (s[8] != 65535) {
		if (s[0] == 48 && s[8] != Settings.FeedAndCutOnCoverClosed) {
			LogInfo("FeedAndCutOnCoverClosed: %4d -> %-4d", Settings.FeedAndCutOnCoverClosed, s[8]);
			Settings.FeedAndCutOnCoverClosed = s[8];
			app_setting_set_int(PRINTER_SECTION, "FeedAndCutOnCoverClosed", s[8]);
		}
		else if (s[0] == 49 && s[8] != Settings.BlackMarkLocation) {
			if (s[8] >= 1 && s[8] <= 2) {
				LogInfo("BlackMarkLocation: %u -> %u", Settings.BlackMarkLocation, s[8]);
				Settings.BlackMarkLocation = s[8];
				app_setting_set_int(PRINTER_SECTION, "BlackMarkLocation", s[8]);
			}
		}
	}
}

static void GS_LP_E_31_data(void)
{
	if (Cmd_List0 < DATA_BUFFER_SIZE)
		DataBuffer[Cmd_List0++] = Cmd_Data;

	if (Cmd_List0 < Cmd_Len)
		return;

	if (Cmd_List0 == DATA_BUFFER_SIZE)
		Cmd_List0--;
	DataBuffer[Cmd_List0] = 0;
	ParsePaperLayout((char *)DataBuffer);

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 201>               |
 *----------------------------------------------*/
static void GS_LP_E_C9_dH(void)
{
	uint8_t buf[8];

	Cmd_List1 |= Cmd_Data << 8;

	buf[0] = 0xC9;
	buf[1] = Cmd_List0;
	*((uint32_t *)(buf + 2)) = Cmd_List1 << 3;
	printer_send_command(buf, 6);

	ResetNextFun();
}

static void GS_LP_E_C9_dL(void)
{
	Cmd_List1 = Cmd_Data;
	pNextFun = GS_LP_E_C9_dH;
}

static void GS_LP_E_C9_n(void)
{
	Cmd_List0 = Cmd_Data;
	pNextFun = GS_LP_E_C9_dL;
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 202>               |
 *----------------------------------------------*/
static void GS_LP_E_CA_lenH(void)
{
	uint8_t buf[8];

	Cmd_List1 |= Cmd_Data << 8;

	buf[0] = 0xCA;
	*((uint32_t *)(buf + 1)) = Cmd_List1 << 3;
	printer_send_command(buf, 5);

	ResetNextFun();
}

static void GS_LP_E_CA_lenL(void)
{
	Cmd_List1 = Cmd_Data;
	pNextFun = GS_LP_E_CA_lenH;
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 203>               |
 *----------------------------------------------*/
static void GS_LP_E_CB(void)
{
	uint8_t buf[4];

	buf[0] = 0xCB;
	printer_send_command(buf, 1);

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 204>               |
 *----------------------------------------------*/
static void GS_LP_E_CC(void)
{
	uint8_t buf[4];

	buf[0] = 0xCC;
	printer_send_command(buf, 1);

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 205>               |
 *----------------------------------------------*/
static void GS_LP_E_CD(void)
{
	Settings.TSPLMode = 1;
	LogInfo("TSPLMode = 1");

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn <function 206>               |
 *----------------------------------------------*/
static void GS_LP_E_CE(void)
{
	unsigned int n = Cmd_Data;
	load_uint32("HeaderFooterImage.Header", (uint32_t *)&Settings.HeaderFooterImage.Header, 0);
	load_uint32("HeaderFooterImage.Footer", (uint32_t *)&Settings.HeaderFooterImage.Footer, 0);
	load_int("FeedAndCutOnCoverClosed", &Settings.FeedAndCutOnCoverClosed, 160);
	load_int("FeedAndCutOnElectrify", &Settings.FeedAndCutOnElectrify, 160);
	load_int("CuttingAutoLogo", &Settings.CuttingAutoLogo, 1);

	LogInfo("GS_LP_E_CE <n = %d><cutter_flag = %d>\n", n, cutter_flag);
	if(n == 0){
		cutter_flag = 0;//恢复为上电的初始状态
	}else{
		if(cutter_flag == 5){
			cutter_flag = 1; //需要在头部追加logo
		}
	}
	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( E pL pH fn                              |
 *----------------------------------------------*/
static void GS_LP_E_fn(void)
{
	Cmd_Len--;

	switch (Cmd_Data) {
	case 1: /* Change into the user setting mode */
		if (Cmd_Len != 2)
			SkipNBytes(Cmd_Len);
		else {
			Cmd_List0 = 0;
			Cmd_List1 = 0;
			pNextFun = GS_LP_E_01_dx;
		}
		break;

	case 2: /* End the user setting mode session */
		if (Cmd_Len != 3)
			SkipNBytes(Cmd_Len);
		else {
			Cmd_List0 = 0;
			Cmd_List1 = 0;
			pNextFun = GS_LP_E_02_dx;
		}
		break;

	case 5:
		if (Cmd_Len < 3 || Cmd_Len > DATA_BUFFER_SIZE)
			SkipNBytes(Cmd_Len);
		else {
			Cmd_List0 = 0;
			pNextFun = GS_LP_E_05_data;
		}
		break;

	case 6:
		if (Cmd_Len != 2)
			SkipNBytes(Cmd_Len);
		else
			pNextFun = GS_LP_E_06_m;
		break;

	case 7:
		if (Cmd_Len != 1)
			SkipNBytes(Cmd_Len);
		else
			pNextFun = GS_LP_E_07_n;
		break;

	case 8:
		if (Cmd_Len != 1)
			SkipNBytes(Cmd_Len);
		else
			pNextFun = GS_LP_E_08_n;
		break;

	case 9:
		if (Cmd_Len != 5)
			SkipNBytes(Cmd_Len);
		else
			pNextFun = GS_LP_E_09_n;
		break;

	case 10:
		if (Cmd_Len != 4 && Cmd_Len != 8)
			SkipNBytes(Cmd_Len);
		else
			pNextFun = GS_LP_E_0A_n;
		break;

	case 11:
		if (Cmd_Len != 1)
			SkipNBytes(Cmd_Len);
		else
			pNextFun = GS_LP_E_0B_n;
		break;

	case 12:
		if (Cmd_Len != 2)
			SkipNBytes(Cmd_Len);
		else {
			Cmd_List0 = 0;
			pNextFun = GS_LP_E_0C_n;
		}
		break;

	case 13:
		if (Cmd_Len != 2)
			SkipNBytes(Cmd_Len);
		else {
			Cmd_List0 = 0;
			pNextFun = GS_LP_E_0D_n;
		}
		break;

	case 14:
		if (Cmd_Len != 17)
			SkipNBytes(Cmd_Len);
		else
			pNextFun = GS_LP_E_0E_n;
		break;

	case 15:
		if (Cmd_Len != 5)
			SkipNBytes(Cmd_Len);
		else
			pNextFun = GS_LP_E_0F_n;
		break;

	case 16:
		if (Cmd_Len != 1)
			SkipNBytes(Cmd_Len);
		else
			pNextFun = GS_LP_E_10_n;
		break;

	case 17:
		if (Cmd_Len != 1)
			SkipNBytes(Cmd_Len);
		else
			pNextFun = GS_LP_E_11_n;
		break;

	case 18:
		if (Cmd_Len != 4)
			SkipNBytes(Cmd_Len);
		else {
			Cmd_List0 = 0;
			pNextFun = GS_LP_E_12_param;
		}
		break;

	case 19:
		if (Cmd_Len != 4 && Cmd_Len != 8)
			SkipNBytes(Cmd_Len);
		else
			pNextFun = GS_LP_E_13_n;
		break;

	case 48: /* Delete the paper layout */
		if (Cmd_Len != 3)
			SkipNBytes(Cmd_Len);
		else {
			Cmd_List0 = 0;
			Cmd_List1 = 0;
			pNextFun = GS_LP_E_30_dx;
		}
		break;

	case 49: /* Set the paper layout */
		if (Cmd_Len < 8)
			SkipNBytes(Cmd_Len);
		else {
			Cmd_List0 = 0;
			pNextFun = GS_LP_E_31_data;
		}
		break;

	case 201: /* 按指定打印密度打印 */
		pNextFun = GS_LP_E_C9_n;
		break;
	case 202: /* 打印走纸积累误差样张 */
		pNextFun = GS_LP_E_CA_lenL;
		break;
	case 203: /* 打印区域样张 */
		GS_LP_E_CB();
		break;
	case 204: /* 打印分辨率样张 */
		GS_LP_E_CC();
		break;
	case 205: /* 设置TSPL模式 */
		GS_LP_E_CD();
		break;
	case 206: /*web 界面配置切刀选项，更新对应的设置项*/
		GS_LP_E_CE();
		break;

	default:
		SkipNBytes(Cmd_Len);
		break;
	}
}

static void GS_LP_E_pH(void)
{
	Cmd_Len |= Cmd_Data << 8;

	if (Cmd_Len == 0)
		ResetNextFun();
	else
		pNextFun = GS_LP_E_fn;
}

static void GS_LP_E_pL(void)
{
	Cmd_Len = Cmd_Data;

	pNextFun = GS_LP_E_pH;
}

/*----------------------------------------------*
 | GS ( L pL pH m fn <function 48>              |
 *----------------------------------------------*/
static void GS_LP_L_TransmitNVGraphicsMemoryCapacity(void)
{
	uint8_t buf[16];
	int len = 0;

	buf[len++] = 0x37;
	buf[len++] = 0x30;
	len += snprintf((char *)(buf + len), sizeof(buf) - 3, "%u", NV_GRAPHICS_MEM_CAP);
	len++; /* The NULL byte */
	ReplyToHost(buf, len, -1);
}

/*----------------------------------------------*
 | GS ( L pL pH m fn <function 51>              |
 *----------------------------------------------*/
static void GS_LP_L_TransmitNVGraphicsRemainingMemoryCapacity(void)
{
	nv_graphics_file_t *files;
	uint8_t buf[16];
	uint32_t i, count, size = 0;
	int len = 0;

	if (GetNVGraphicsFileInfo(&files, &count) == 0) {
		if (files) {
			for (i = 0; i < count; i++)
				size += files[i].file_size;
			free(files);
		}
	}

	size = (size > NV_GRAPHICS_MEM_CAP) ? 0 : NV_GRAPHICS_MEM_CAP - size;
	buf[len++] = 0x37;
	buf[len++] = 0x30;
	len += snprintf((char *)(buf + len), sizeof(buf) - 3, "%u", size);
	len++; /* The NULL byte */
	ReplyToHost(buf, len, -1);
}

/*----------------------------------------------*
 | GS ( L pL pH m fn <function 64>              |
 *----------------------------------------------*/
static void GS_LP_L_TransmitKeyCodeList(uint8_t *data, uint32_t len)
{
	nv_graphics_file_t *files;
	uint8_t buf[96];
	uint32_t i, count;
	int n;

	if (len != 2)
		return;
	if (data[0] != 75 || data[1] != 67)
		return;

	n = 0;
	buf[n++] = 0x37;
	buf[n++] = 0x72;
	buf[n++] = 0x40;

	if (GetNVGraphicsFileInfo(&files, &count) == 0) {
		if (files) {
			for (i = 0; i < count; i++) {
				buf[n++] = files[i].kc1;
				buf[n++] = files[i].kc2;
				if (n == 83) {
					if (i + 1 < count) /* More key code pairs are present */
						buf[2] = 0x41;
					buf[n++] = 0;
					ReplyToHost(buf, n, -1);
					n = 0;
					buf[n++] = 0x37;
					buf[n++] = 0x72;
					buf[n++] = 0x40;
				}
			}
			if (n > 3) {
				buf[n++] = 0;
				ReplyToHost(buf, n, -1);
			}
			free(files);
			return;
		}
	}

	buf[n++] = 0;
	ReplyToHost(buf, n, -1);
}

/*----------------------------------------------*
 | GS ( L pL pH m fn <function 65>              |
 *----------------------------------------------*/
static void GS_LP_L_DeleteAllNVGraphicsData(uint8_t *data, uint32_t len)
{
	if (len != 3)
		return;
	if (data[0] != 67 || data[1] != 76 || data[2] != 82)
		return;
	if (Settings.Mode != STANDARD_MODE)
		return;
	if (!lineBuffer->isEmpty())
		return;

	if (access(NV_GRAPHICS_DIR, F_OK) == 0) {
		system("rm -rf " NV_GRAPHICS_DIR "*");
		system("sync");
	}
}

/*----------------------------------------------*
 | GS ( L pL pH m fn <function 66>              |
 *----------------------------------------------*/
static void GS_LP_L_DeleteSpecifiedNVGraphicsData(uint8_t *data, uint32_t len)
{
	char cmd[64];

	if (len != 2)
		return;
	if (data[0] < 32 || data[0] > 126)
		return;
	if (data[1] < 32 || data[1] > 126)
		return;
	if (Settings.Mode != STANDARD_MODE)
		return;
	if (!lineBuffer->isEmpty())
		return;

	if (access(NV_GRAPHICS_DIR, F_OK) == 0) {
		sprintf(cmd, "rm -f " NV_GRAPHICS_DIR "%02x_%02x.bin", data[0], data[1]);
		system(cmd);
		system("sync");
	}
}

/*----------------------------------------------*
 | GS ( L pL pH m fn <function 67,68>           |
 *----------------------------------------------*/
static void GS_LP_L_DefineNVGraphicsData(uint8_t *data, uint32_t len, int format)
{
	uint32_t data_offset, data_len;

	LogDbg("len=%u", len);
	if (Settings.Mode != STANDARD_MODE)
		goto _exit;
	if (!lineBuffer->isEmpty())
		goto _exit;
	if (Cmd_List4 > IMG_MAX_LEN - IMG_HEAD_LEN)
		goto _exit;

	if (Cmd_List3 == 0) { /* The first parameter packet */
		if (len < 10)
			goto _exit;
		if (data[0] != 48) /* a */
			goto _exit;
		if (data[1] < 32 || data[1] > 126) /* kc1 */
			goto _exit;
		if (data[2] < 32 || data[2] > 126) /* kc2 */
			goto _exit;
		if (data[3] != 1) /* b */
			goto _exit;

		uint32_t w, h, size;

		w = (data[5] << 8) | data[4]; /* x */
		h = (data[7] << 8) | data[6]; /* y */
		LogDbg("w=%u, h=%u", w, h);
		if (w < 1 || w > DOTS_PER_LINE_MAX)
			goto _exit;
		if (h < 1 || h > 2304)
			goto _exit;
		if (data[8] != 49)
			goto _exit;
		switch (format) {
		case NV_GRAPHICS_DATA_RASTER_FORMAT:
			size = ((w + 7) >> 3) * h;
			break;
		case NV_GRAPHICS_DATA_COLUMN_FORMAT:
			size = ((h + 7) >> 3) * w;
			break;
		default:
			goto _exit;
		}
		LogDbg("size=%u", size);
		if (Cmd_Len != size + 9)
			goto _exit;
		if(size > IMG_MAX_LEN - IMG_HEAD_LEN)
		{
			LogDbg("Receiving nv image len=%u > max_len=%u Will be discarded!", size, IMG_MAX_LEN - IMG_HEAD_LEN);
			Cmd_List4 = size;
			goto _exit;
		}

		DefineNVGraphicsData_Init(data[1], data[2], w, h, format);

		data_offset = 9;
		data_len = len - 9;
	}
	else {
		data_offset = 0;
		data_len = len;
	}

	if (data_len > 0)
		DefineNVGraphicsData_Append(DataBuffer + data_offset, data_len);

_exit:
	Cmd_List3 += len;
}

/*----------------------------------------------*
 | GS ( L pL pH m fn <function 69>              |
 *----------------------------------------------*/
static void GS_LP_L_PrintNVGraphicsData(uint8_t *data, uint32_t len)
{
	if (len == 4)
		PrintNVMonoImage(data[0], data[1], data[2], data[3]);
}

/*
 * Cmd_List0: fn
 * Cmd_List1: bytes to receive
 * Cmd_List2: bytes received
 * Cmd_List3: bytes handled
 */
static void GS_LP_L_ReceiveData(void)
{
	DataBuffer[Cmd_List2++] = Cmd_Data;

	if (Cmd_List2 < Cmd_List1)
		return;

	switch (Cmd_List0) { /* fn */
	case 64: /* Transmit the key code list for defined NV graphics */
		GS_LP_L_TransmitKeyCodeList(DataBuffer, Cmd_List1);
		break;
	case 65: /* Delete all NV graphics data */
		GS_LP_L_DeleteAllNVGraphicsData(DataBuffer, Cmd_List1);
		break;
	case 66: /* Delete the specified NV graphics data */
		GS_LP_L_DeleteSpecifiedNVGraphicsData(DataBuffer, Cmd_List1);
		break;
	case 67: /* Define the NV graphics data (raster format) */
		GS_LP_L_DefineNVGraphicsData(DataBuffer, Cmd_List1, NV_GRAPHICS_DATA_RASTER_FORMAT);
		break;
	case 68: /* Define the NV graphics data (column format) */
		GS_LP_L_DefineNVGraphicsData(DataBuffer, Cmd_List1, NV_GRAPHICS_DATA_COLUMN_FORMAT);
		break;
	case 69: /* Print the specified NV graphics data */
		GS_LP_L_PrintNVGraphicsData(DataBuffer, Cmd_List1);
		break;
	default:
		break;
	}

	Cmd_Len -= Cmd_List1;
	if (Cmd_Len > 0) {
		switch (Cmd_List0) { /* fn */
		/*
		 * These functions may have more data to receive.
		 */
		case 67:  /* Define the NV graphics data (raster format) */
		case 68:  /* Define the NV graphics data (column format) */
		case 83:  /* Define the downloaded graphics data (raster format) */
		case 84:  /* Define the downloaded graphics data (column format) */
		case 112: /* Store the graphics data in the print buffer (raster format) */
		case 113: /* Store the graphics data in the print buffer (column format) */
			Cmd_List1 = (Cmd_Len > DATA_BUFFER_SIZE) ? DATA_BUFFER_SIZE : Cmd_Len;
			Cmd_List2 = 0;
			break;
		default:
			SkipNBytes(Cmd_Len);
			break;
		}
		return;
	}

	ResetNextFun();
}

static void GS_LP_L_fn(void)
{
	Cmd_Len--;

	if (Cmd_Len == 0) {
		switch (Cmd_Data) {
		case 0:
		case 48: /* Transmit the NV graphics memory capacity */
			GS_LP_L_TransmitNVGraphicsMemoryCapacity();
			break;
		case 50: /* Print the graphics data in the print buffer */
			break;
		case 3:
		case 51: /* Transmit the remaining capacity of the NV graphics memory */
			GS_LP_L_TransmitNVGraphicsRemainingMemoryCapacity();
			break;
		case 52: /* Transmit the remaining capacity of the download graphics memory */
			break;
		default:
			break;
		}
		ResetNextFun();
	}
	else {
		Cmd_List0 = Cmd_Data;
		Cmd_List1 = (Cmd_Len > DATA_BUFFER_SIZE) ? DATA_BUFFER_SIZE : Cmd_Len;
		Cmd_List2 = 0;
		Cmd_List3 = 0;
		Cmd_List4 = 0;
		pNextFun = GS_LP_L_ReceiveData;
	}
}

static void GS_LP_L_m(void)
{
	Cmd_Len--;

	if (Cmd_Data != 48)
		SkipNBytes(Cmd_Len);
	else
		pNextFun = GS_LP_L_fn;
}

static void GS_LP_L_pH(void)
{
	Cmd_Len |= Cmd_Data << 8;

	if (Cmd_Len < 2)
		SkipNBytes(Cmd_Len);
	else
		pNextFun = GS_LP_L_m;
}

static void GS_LP_L_pL(void)
{
	Cmd_Len = Cmd_Data;

	pNextFun = GS_LP_L_pH;
}

/*----------------------------------------------*
 | GS ( T pL pH fn <function 3>                 |
 *----------------------------------------------*/
static void GS_LP_T_03_dx(void)
{
	const print_shm_t *shm;
	const print_task_t *task;
	uint8_t status;

	Cmd_List0 |= Cmd_Data << (Cmd_List1 << 3);
	if (++Cmd_List1 < Cmd_Len)
		return;

	shm = print_shm();
	status = 0;
	if (shm) {
		if (print_task_is_complete(Cmd_List0))
			status = 3;
		else {
			task = task_at(shm->task_queue.head);
			if (task && task->id == (int)Cmd_List0)
				status = 2;
			else if ((int)Cmd_List0 < shm->task_pool.next_task_id)
				status = 1;
		}
	}
	LogDbg("status=%u", status);
	ReplyToHost(&status, sizeof(status), -1);

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( T pL pH fn <function 5>                 |
 *----------------------------------------------*/
static void GS_LP_T_05_dx(void)
{
	const print_shm_t *shm;
	uint8_t status = 0;

	Cmd_List0 |= Cmd_Data << (Cmd_List1 << 3);
	if (++Cmd_List1 < Cmd_Len)
		return;

	shm = print_shm();
	if (shm) {
		status = print_task_delete(Cmd_List0);
	}
	LogDbg("status=%u", status);
	ReplyToHost(&status, sizeof(status), -1);

	ResetNextFun();
}

/*----------------------------------------------*
 | GS ( T pL pH fn                              |
 *----------------------------------------------*/
static void GS_LP_T_fn(void)
{
	Cmd_Len--;

	switch (Cmd_Data) {
	case 1: /* 查询打印机状态 */
		if (Cmd_Len != 0)
			SkipNBytes(Cmd_Len);
		else {
			uint8_t status = *((const uint8_t *)print_status());

			LogDbg("status=%02X", status);
			ReplyToHost(&status, sizeof(status), -1);

			ResetNextFun();
		}
		break;

	case 2: /* 获取最近一个打印任务的编号 */
		if (Cmd_Len != 0)
			SkipNBytes(Cmd_Len);
		else {
			const print_shm_t *shm = print_shm();
			int task_id = 0;

			if (shm) {
				switch (Cmd_ChnId) {
				case PRINT_CHN_ID_USB:
				case PRINT_CHN_ID_BLE:
				case PRINT_CHN_ID_SPP ... PRINT_CHN_ID_SPP4:
				case PRINT_CHN_ID_TCP_BEGIN ... PRINT_CHN_ID_TCP_END:
					task_id = shm->channels[Cmd_ChnId].last_task_id;
					break;
				default:
					break;
				}
			}
			LogDbg("task_id=%d", task_id);
			ReplyToHost(&task_id, sizeof(task_id), -1);

			ResetNextFun();
		}
		break;

	case 3: /* 查询打印任务状态 */
		if (Cmd_Len != 4)
			SkipNBytes(Cmd_Len);
		else {
			Cmd_List0 = 0;
			Cmd_List1 = 0;
			pNextFun = GS_LP_T_03_dx;
		}
		break;

	case 4: /* 清除未取纸状态 */
		if (Cmd_Len != 0)
			SkipNBytes(Cmd_Len);
		else {
			PrnIoctl(PRN_CMD_CLEAR_PAPER_NOT_TAKEN, NULL, 0, NULL, NULL);
			ResetNextFun();
		}
		break;

	case 5: /* 清除打印任务缓冲数据 */
		if (Cmd_Len != 4)
			SkipNBytes(Cmd_Len);
		else {
			Cmd_List0 = 0;
			Cmd_List1 = 0;
			pNextFun = GS_LP_T_05_dx;
		}
		break;

	default:
		SkipNBytes(Cmd_Len);
		break;
	}
}

static void GS_LP_T_pH(void)
{
	Cmd_Len |= Cmd_Data << 8;

	if (Cmd_Len == 0)
		ResetNextFun();
	else
		pNextFun = GS_LP_T_fn;
}

static void GS_LP_T_pL(void)
{
	Cmd_Len = Cmd_Data;

	pNextFun = GS_LP_T_pH;
}

/*----------------------------------------------*
 | GS ( k                                       |
 *----------------------------------------------*/
static void PDF417SetColumns(uint8_t *data, uint32_t len)
{
	if (len != 1)
		goto _exit;

	if (data[0] <= 30)
		PDF417.Columns = data[0];

_exit:
	ResetNextFun();
}

static void PDF417SetRows(uint8_t *data, uint32_t len)
{
	if (len != 1)
		goto _exit;

	if ((data[0] == 0) || (data[0] >= 3 && data[0] <= 90))
		PDF417.Rows = data[0];

_exit:
	ResetNextFun();
}

static void PDF417SetModuleSize(uint8_t *data, uint32_t len)
{
	if (len != 1)
		goto _exit;

	if (data[0] >= 1 && data[0] <= 16)
		PDF417.ModuleSize = data[0];

_exit:
	ResetNextFun();
}

static void PDF417SetRowHeight(uint8_t *data, uint32_t len)
{
	if (len != 1)
		goto _exit;

	if (data[0] >= 1 && data[0] <= 16)
		PDF417.RowHeight = data[0];

_exit:
	ResetNextFun();
}

static void PDF417SetErrorCorrectionLevel(uint8_t *data, uint32_t len)
{
	if (len != 2)
		goto _exit;

	if (data[0] == 48 && (data[1] >= 48 && data[1] <= 56)) /* Set error correction by "level" */
		PDF417.ECLevel = data[1];
	else if (data[0] == 49 && (data[1] >= 1 && data[1] <= 40)) /* Set error correction by "ratio" */
		PDF417.ECLevel = data[1];

_exit:
	ResetNextFun();
}

static void PDF417SetOptions(uint8_t *data, uint32_t len)
{
	if (len != 1)
		goto _exit;

	if (data[0] <= 1)
		PDF417.Options = data[0];

_exit:
	ResetNextFun();
}

static void PDF417Generate(uint8_t *data, uint32_t len)
{
	struct zint_symbol *symbol = NULL;
	int i, j, k, p, x_size, y_size;
	uint32_t rows, columns, offset, bytes_per_row;
	uint8_t mask;

	if (len < 2)
		goto _exit;

	if (data[0] != 48)
		goto _exit;
	data++;
	len--;

	PDF417.SymbolRows = 0;
	PDF417.SymbolWidth = 0;

	symbol = ZBarcode_Create();
	if (symbol == NULL)
		goto _exit;

	symbol->symbology = (PDF417.Options == 0) ? BARCODE_PDF417 : BARCODE_PDF417TRUNC;

	if (PDF417.ECLevel >= 48 && PDF417.ECLevel <= 56) {
		/* Set error correction by "level" */
		symbol->option_1 = PDF417.ECLevel - 48;
	}
	else {
		/* Set error correction by "ratio" */
		i = len * PDF417.ECLevel / 10;
		if (i < 4)
			symbol->option_1 = 1;
		else if (i < 11)
			symbol->option_1 = 2;
		else if (i < 21)
			symbol->option_1 = 3;
		else if (i < 46)
			symbol->option_1 = 4;
		else if (i < 101)
			symbol->option_1 = 5;
		else if (i < 201)
			symbol->option_1 = 6;
		else if (i < 401)
			symbol->option_1 = 7;
		else
			symbol->option_1 = 8;
	}
	if (PDF417.Columns == 0) {
		if (PDF417.Rows != 0)
			symbol->option_2 = (len - 1) / PDF417.Rows + 1;
		else
			symbol->option_2 = 0;
	}
	else
		symbol->option_2 = PDF417.Columns;

	if (ZBarcode_Encode_and_Buffer(symbol, data, len, 0) > ZINT_WARN_INVALID_OPTION)
		goto _exit;

	if (symbol->rows == 0 || symbol->width == 0)
		goto _exit;

	rows = symbol->rows;
	columns = symbol->width + ((symbol->symbology == BARCODE_PDF417TRUNC) ? 1 : 0);
	bytes_per_row = (columns - 1) / 8 + 1;
	LogInfo("rows=%u, columns=%u", rows, columns);

	if (rows * bytes_per_row > sizeof(PDF417.Bitmap.Modules))
		goto _exit;

	PDF417.SymbolRows = rows;
	PDF417.SymbolWidth = columns;

	p = 0;
	x_size = symbol->bitmap_width / symbol->width;
	y_size = symbol->bitmap_height / symbol->rows;

	memset(&PDF417.Bitmap, 0, sizeof(PDF417.Bitmap));

	offset = 0;
	for (i = 0; i < symbol->rows; i++) {
		for (j = 0, k = 0, mask = 0x80; j < symbol->width; j++) {
			if (symbol->bitmap[p] == 0) /* 有效黑点 */
				PDF417.Bitmap.Modules[offset + k] |= mask;
			mask >>= 1;
			if (mask == 0) {
				k++;
				mask = 0x80;
			}
			p += (3 * x_size);
		}
		if (symbol->symbology == BARCODE_PDF417TRUNC)
			PDF417.Bitmap.Modules[offset + k] |= mask;
		p += (3 * symbol->bitmap_width * (y_size - 1));
		offset += bytes_per_row;
	}

_exit:
	if (symbol)
		ZBarcode_Delete(symbol);
	ResetNextFun();
}

static void PDF417Print(uint8_t *data, uint32_t len)
{
	if (len != 1)
		goto _exit;

	if (data[0] == 48)
		lineBuffer->printPDF417();

_exit:
	ResetNextFun();
}

static void QRCodeSelectModel(uint8_t *data, uint32_t len)
{
	if (len != 2)
		goto _exit;

	if (data[0] >= 0x30 && data[0] <= 0x39)
		data[0] -= 0x30;

	QRCode.Model = data[0];

_exit:
	ResetNextFun();
}

static void QRCodeSetModuleSize(uint8_t *data, uint32_t len)
{
	if (len != 1)
		goto _exit;

	if (data[0] >= 1 && data[0] <= 16)
		QRCode.ModuleSize = data[0];

_exit:
	ResetNextFun();
}

static void QRCodeSelectErrorCorrectionLevel(uint8_t *data, uint32_t len)
{
	if (len != 1)
		goto _exit;

	if (data[0] >= 48 && data[0] <= 51)
		QRCode.ECLevel = data[0] - 48;

_exit:
	ResetNextFun();
}

static void QRCodeGenerate(uint8_t *data, uint32_t len)
{
	struct zint_symbol *symbol;
	int i, j, k, p, x_size, y_size;
	uint8_t mask;

	if (len < 2)
		goto _exit;

	if (data[0] != 48)
		goto _exit;
	data++;
	len--;

	QRCode.SymbolSize = 0;

	symbol = ZBarcode_Create();
	if (symbol == NULL)
		goto _exit;

	symbol->symbology = BARCODE_QRCODE;

	symbol->option_1 = QRCode.ECLevel + 1; /* Error correction level, 1-4 */
	symbol->option_2 = QRCode.Model;

	if (ZBarcode_Encode_and_Buffer(symbol, data, len, 0) > ZINT_WARN_INVALID_OPTION) {
		ZBarcode_Delete(symbol);
		goto _exit;
	}

	p = 0;
	x_size = symbol->bitmap_width / symbol->width;
	y_size = symbol->bitmap_height / symbol->rows;

	memset(&QRCode.Bitmap, 0, sizeof(QRCode.Bitmap));

	for (i = 0; i < symbol->rows; i++) {
		for (j = 0, k = 0, mask = 0x80; j < symbol->width; j++) {
			if (symbol->bitmap[p] == 0) /* 有效黑点 */
				QRCode.Bitmap.Modules[i][k] |= mask;
			mask >>= 1;
			if (mask == 0) {
				k++;
				mask = 0x80;
			}
			p += (3 * x_size);
		}
		p += (3 * symbol->bitmap_width * (y_size - 1));
	}
	QRCode.SymbolSize = symbol->rows;

	ZBarcode_Delete(symbol);

_exit:
	ResetNextFun();
}

static void QRCodePrint(uint8_t *data, uint32_t len)
{
	if (len != 1)
		goto _exit;

	if (data[0] == 48)
		lineBuffer->printQRCode();

_exit:
	ResetNextFun();
}

static void GS_LP_k_ReceiveData(void)
{
	DataBuffer[Cmd_List0++] = Cmd_Data;

	if (Cmd_List0 < Cmd_Len)
		return;

	if (Cmd_Len < 2)
		goto _exit;

	Cmd_Len -= 2;

	switch (DataBuffer[0]) {
	case 48: /* PDF417 */
		switch (DataBuffer[1]) {
		case 65:
			PDF417SetColumns(DataBuffer + 2, Cmd_Len);
			break;
		case 66:
			PDF417SetRows(DataBuffer + 2, Cmd_Len);
			break;
		case 67:
			PDF417SetModuleSize(DataBuffer + 2, Cmd_Len);
			break;
		case 68:
			PDF417SetRowHeight(DataBuffer + 2, Cmd_Len);
			break;
		case 69:
			PDF417SetErrorCorrectionLevel(DataBuffer + 2, Cmd_Len);
			break;
		case 70:
			PDF417SetOptions(DataBuffer + 2, Cmd_Len);
			break;
		case 80:
			PDF417Generate(DataBuffer + 2, Cmd_Len);
			break;
		case 81:
			PDF417Print(DataBuffer + 2, Cmd_Len);
			break;
		default:
			break;
		}
		break;

	case 49: /* QR code */
		switch (DataBuffer[1]) {
		case 65:
			QRCodeSelectModel(DataBuffer + 2, Cmd_Len);
			break;
		case 67:
			QRCodeSetModuleSize(DataBuffer + 2, Cmd_Len);
			break;
		case 69:
			QRCodeSelectErrorCorrectionLevel(DataBuffer + 2, Cmd_Len);
			break;
		case 80:
			QRCodeGenerate(DataBuffer + 2, Cmd_Len);
			break;
		case 81:
			QRCodePrint(DataBuffer + 2, Cmd_Len);
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}

_exit:
	ResetNextFun();
}

static void GS_LP_k_pH(void)
{
	Cmd_Len |= (Cmd_Data << 8) & 0xFF00;

	if (Cmd_Len == 0)
		ResetNextFun();
	else if (Cmd_Len > DATA_BUFFER_SIZE)
		SkipNBytes(Cmd_Len);
	else {
		Cmd_List0 = 0;
		pNextFun = GS_LP_k_ReceiveData;
	}
}

static void GS_LP_k_pL(void)
{
	Cmd_Len = Cmd_Data;

	pNextFun = GS_LP_k_pH;
}

static void GS_LP(void)
{
	switch (Cmd_Data) {
	case 0x41: /* [GS ( A] Execute test print */
		pNextFun = GS_LP_A_pL;
		break;
	case 0x45: /* [GS ( E] Set user setup commands */
		pNextFun = GS_LP_E_pL;
		break;
	case 0x4C: /* [GS ( L] Set graphics data */
		pNextFun = GS_LP_L_pL;
		break;
	case 0x54: /* [GS ( T] Print task management commands */
		pNextFun = GS_LP_T_pL;
		break;
	case 0x6B: /* [GS ( k] Set up and print the symbol */
		pNextFun = GS_LP_k_pL;
		break;
	default:
		ResetNextFun();
		break;
	}
}

/*----------------------------------------------*
 | GS *                                         |
 *----------------------------------------------*/
static void DefineDownloadedBitImage_data(void)
{
	if (Cmd_List2 < Cmd_List1)
		DownloadedBitImage.data[Cmd_List2] = Cmd_Data;
	Cmd_List2++;
	if (Cmd_List2 == Cmd_List0)
		ResetNextFun();
}

static void DefineDownloadedBitImage_y(void)
{
	DownloadedBitImage.y = (uint8_t)Cmd_Data;
	Cmd_List0 = DownloadedBitImage.x * DownloadedBitImage.y * 8; /* bytes to receive */
	if (Cmd_List0 == 0) {
		DownloadedBitImage.x = 0;
		DownloadedBitImage.y = 0;
		ResetNextFun();
	}
	else {
		if (DownloadedBitImage.x > BYTES_PER_LINE)
			DownloadedBitImage.x = BYTES_PER_LINE;
		Cmd_List1 = DownloadedBitImage.x * DownloadedBitImage.y * 8; /* bytes of the image */
		Cmd_List2 = 0; /* bytes received */
		pNextFun = DefineDownloadedBitImage_data;
	}
}

static void DefineDownloadedBitImage_x(void)
{
	DownloadedBitImage.x = (uint8_t)Cmd_Data;
	pNextFun = DefineDownloadedBitImage_y;
}

/*----------------------------------------------*
 | GS /                                         |
 *----------------------------------------------*/
static void PrintDownloadedBitImage_m(void)
{
	uint32_t i, j, k, width, height, scale_w, scale_h;
	uint32_t row, bitext;
	uint8_t mask;
	const uint8_t *d;

	if (Cmd_Data >= 48 && Cmd_Data <= 51)
		Cmd_Data -= 48;
	if (Cmd_Data > 3 || DownloadedBitImage.x == 0 || DownloadedBitImage.y == 0) {
		ResetNextFun();
		return;
	}

	if (!lineBuffer->isEmpty())
		lineBuffer->printLine();

	scale_w = (Cmd_Data     ) & 1;
	scale_h = (Cmd_Data >> 1) & 1;

	width = DownloadedBitImage.x * 8;
	if (width > (Settings.printAreaWidth() >> scale_w))
		width = (Settings.printAreaWidth() >> scale_w);
	height = 8 << scale_h;

	bitext = (scale_w) ? 0xC0000000 : 0x80000000;

	for (i = 0; i < DownloadedBitImage.y; i++) {
		d = DownloadedBitImage.data + i;
		for (j = 0; j < width; j++) {
			mask = 0x80;
			row = MAX_LINE_HEIGHT - height;
			for (k = 0; k < 8; k++) {
				if ((*d) & mask) {
					Dotline_SetDots(lineBuffer->dotlineAt(row), lineBuffer->EndPos, bitext, 0xC0000000, false);
					if (scale_h) /* 倍高 */
						Dotline_SetDots(lineBuffer->dotlineAt(row + 1), lineBuffer->EndPos, bitext, 0xC0000000, false);
				}
				mask >>= 1;
				row += (1 << scale_h);
			}
			d += DownloadedBitImage.y;
			lineBuffer->EndPos += (1 << scale_w);
		}
		lineBuffer->setHeight(height);
		lineBuffer->print(0);
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | GS B                                         |
 *----------------------------------------------*/
static void SetBlackWhiteReverseMode(void)
{
	Settings.BlackWhiteReverseMode = (Cmd_Data & 0x01);

	ResetNextFun();
}

/*----------------------------------------------*
 | GS H                                         |
 *----------------------------------------------*/
static void SetBarcodeHRIPos(void)
{
	if (Cmd_Data >= 48 && Cmd_Data <= 51)
		Cmd_Data -= 48;
	if (Cmd_Data <= 3)
		Barcode.HRIPos = Cmd_Data;

	ResetNextFun();
}

/*----------------------------------------------*
 | GS I                                         |
 *----------------------------------------------*/
static void TransmitCustomizedID(uint8_t index)
{
	char pathname[64];
	uint8_t buf[64];
	FILE *fp;
	int len;

	memset(buf, 0, sizeof(buf));
	buf[0] = 0x5F;
	len = 2;

	sprintf(pathname, "/data/SYS/customized_id%u", index);
	fp = fopen(pathname, "rb");
	if (!fp)
		goto _exit;
	len = fread(buf + 1, 1, sizeof(buf) - 2, fp);
	fclose(fp);
	if (len < 0)
		len = 0;
	buf[len + 1] = 0;
	len = 1;
	while (buf[len++])
		;
_exit:
	ReplyToHost(buf, len, -1);
}

static void TransmitPrinterID(void)
{
	uint8_t buf[32];
	int len;

	switch (Cmd_Data) {
	/* Transmit one byte of printer ID */
	case 1:
	case 49:
	case 2:
	case 50:
	case 3:
	case 51:
		buf[0] = 0;
		ReplyToHost(buf, 1, -1);
		break;

#define P(...) \
do { \
	buf[0] = 0x5F; \
	len = snprintf((char *)(buf + 1), sizeof(buf) - 2, __VA_ARGS__); \
	buf[len + 1] = 0; \
	ReplyToHost(buf, len + 2, -1); \
} while (0)
	/* Transmit specified printer information B */
	case 65: /* Firmware version */
		P("%s/%s", sys_global_var()->fw_ver, APP0_VERSION);
		break;
	case 66: /* Maker name */
		P("SUNMI");
		break;
	case 67: /* Printer model */
		P("%s", sys_global_var()->model);
		break;
	case 68: /* Serial No */
		P("%s", sys_global_var()->sn);
		break;
	case 69: /* Font of language for each country */
		if (Settings.Utf8_WordSet > 0)
			P("UTF-8");
		else {
			switch (Settings.CJK_WordSet) {
			case CJK_WORDSET_GB18030:   P("GB18030");   break;
			case CJK_WORDSET_BIG5:      P("BIG5");      break;
			case CJK_WORDSET_SHIFT_JIS: P("SHIFT JIS"); break;
			case CJK_WORDSET_JIS0208:   P("JIS 0208");  break;
			case CJK_WORDSET_KSC5601:   P("KS C 5601"); break;
			default:                    P("N.A.");      break;
			}
		}
		break;
	case 70 ... 72:
		TransmitCustomizedID(Cmd_Data - 70);
		break;
	case 112:
		P("N.A.");
		break;
#undef P

	default:
		break;
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | GS L                                         |
 *----------------------------------------------*/
static void SetLeftMargin_nH(void)
{
	Settings.LeftMargin = (Cmd_Data << 8) | Cmd_List0;
	if (Settings.LeftMargin + 24 > DOTS_PER_LINE)
		Settings.LeftMargin = DOTS_PER_LINE - 24;
	if (Settings.LeftMargin + Settings.PrintAreaWidth > DOTS_PER_LINE)
		Settings.PrintAreaWidth = DOTS_PER_LINE - Settings.LeftMargin;

	ResetNextFun();
}

static void SetLeftMargin_nL(void)
{
	Cmd_List0 = Cmd_Data;

	pNextFun = SetLeftMargin_nH;
}

/*----------------------------------------------*
 | GS T                                         |
 *----------------------------------------------*/
static void MoveToBegin(void)
{
	if (Settings.Mode != STANDARD_MODE)
		goto _exit;
	if (lineBuffer->EndPos == Settings.leftMargin())
		goto _exit;
	if (Cmd_Data >= 48 && Cmd_Data <= 49)
		Cmd_Data -= 48;
	if (Cmd_Data == 0)
		lineBuffer->clear();
	else if (Cmd_Data == 1)
		lineBuffer->printLine();

_exit:
	ResetNextFun();
}

/*----------------------------------------------*
 | GS V                                         |
 *----------------------------------------------*/
static void CutPaper_n(void)
{
	uint8_t buf[16];

	memset(buf, 0, sizeof(buf));

	switch (Cmd_List0) {
	/* Function C */
	case 97:
		buf[0] = 0x22;
		buf[1] = 2;
		*((uint32_t *)(buf + 2)) = Cmd_Data;
		break;
	case 98:
		buf[0] = 0x22;
		buf[1] = 1;
		*((uint32_t *)(buf + 2)) = Cmd_Data;
		break;
	/* Function B,D */
	case 65:
	case 103:
		buf[0] = 0x21;
		buf[1] = 2;
		*((uint32_t *)(buf + 2)) = 144 + Cmd_Data;
		break;
	case 66:
	case 104:
		buf[0] = 0x21;
		buf[1] = 1;
		*((uint32_t *)(buf + 2)) = 144 + Cmd_Data;
		break;
	default:
		goto _exit;
	}

	MakeCutCmdId((uint64_t *)(buf + 6));
	printer_send_command(buf, 14);

_exit:
	ResetNextFun();
}

static void CutPaper(void)
{
	uint8_t buf[16];

	if (Cmd_Data <= 1)
		Cmd_Data += 48;

	memset(buf, 0, sizeof(buf));

	switch (Cmd_Data) {
	/* Function A */
	case 48:
		buf[1] = 2;
		break;
	case 49:
		buf[1] = 1;
		break;
	/* Function B */
	case 65:
	case 66:
	/* Function C */
	case 97:
	case 98:
	/* Function D */
	case 103:
	case 104:
		Cmd_List0 = Cmd_Data;
		pNextFun = CutPaper_n;
		return;
	default:
		goto _exit;
	}

	buf[0] = 0x21;
	MakeCutCmdId((uint64_t *)(buf + 6));
	printer_send_command(buf, 14);

_exit:
	ResetNextFun();
}

/*----------------------------------------------*
 | GS W                                         |
 *----------------------------------------------*/
static void SetPrintAreaWidth_nH(void)
{
	Settings.PrintAreaWidth = (Cmd_Data << 8) | Cmd_List0;
	if (Settings.LeftMargin + Settings.PrintAreaWidth > DOTS_PER_LINE)
		Settings.PrintAreaWidth = DOTS_PER_LINE - Settings.LeftMargin;

	ResetNextFun();
}

static void SetPrintAreaWidth_nL(void)
{
	Cmd_List0 = Cmd_Data;

	pNextFun = SetPrintAreaWidth_nH;
}

/*----------------------------------------------*
 | GS \                                         |
 *----------------------------------------------*/
static void SetPageModeRelativeVerticalPrintPosition(void)
{
	switch (Cmd_List0++) {
	case 0:
		Cmd_List1  = Cmd_Data;
		return;
	case 1:
		Cmd_List1 |= Cmd_Data << 8;
		uint32_t dotsToFeed = (203 * Cmd_List1) / Settings.PrintVerticalAccuracy;
		if (Settings.Mode == PAGE_MODE)
			pageBuffer->feed((short)dotsToFeed);
		break;
	default:
		break;
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | GS f                                         |
 *----------------------------------------------*/
static void SetBarcodeHRIFont(void)
{
	if (Cmd_Data >= 48 && Cmd_Data <= 49)
		Cmd_Data -= 48;
	if (Cmd_Data <= 1)
		Barcode.HRIFont = Cmd_Data;

	ResetNextFun();
}

/*----------------------------------------------*
 | GS h                                         |
 *----------------------------------------------*/
static void SetBarcodeHeight(void)
{
	if (Cmd_Data >= 1 && Cmd_Data <= 255)
		Barcode.Height = Cmd_Data;

	ResetNextFun();
}

/*----------------------------------------------*
 | GS k                                         |
 *----------------------------------------------*/
/*
 * ceil_div == 0: 10/3=3, 10/2=5
 * ceil_div != 0: 10/3=4, 10/2=5
 */
static uint32_t IntegerDiv(uint32_t integer1, uint32_t integer2, uint32_t divisor, uint32_t ceil_div)
{
	if(divisor == 0){
		LogError("The denominator is 0!");
		return -1;
	}

	uint32_t dividend = integer1 - integer2;
	if(dividend > integer1){
		memset(Barcode.Bitmap.HRI, 0, sizeof(Barcode.Bitmap.HRI));
		LogError("The length of the HRI characters exceeds the width of the barcode!");
		return -1;
	}

	if(dividend == 0 && ceil_div != 0)
		return 0;
	else
		return (ceil_div) ? ((dividend - 1) / divisor + 1) : (dividend / divisor);
}

static uint32_t HRIfontColumns(void)
{
	switch (Barcode.HRIFont) {
	case 1:  return 9;
	default: break;
	}
	return 12;
}

static void DrawHRIfont(uint32_t bit_pos, uint8_t c)
{
	uint32_t i, ascii_buf[24], rc, rows, columns, mask;

	switch (Barcode.HRIFont) {
	case 1:  rc = ReadWordSet_ASCII_9x17(ascii_buf, c);  break;
	default: rc = ReadWordSet_ASCII_12x24(ascii_buf, c); break;
	}
	columns = ((rc      ) & 0xFFFF);
	rows    = ((rc >> 16) & 0xFFFF);
	for (i = 1, mask = 0x80000000; i < columns; i++)
		mask |= (mask >> 1);
	for (i = 24 - rows; i < 24; i++)
		Dotline_CopyU32(Barcode.Bitmap.HRI + i * (lineBuffer->dotsPerLine() >> 5),
			bit_pos + Settings.leftMargin(), ascii_buf[i], mask, false);
}

/*ret < 0 failed, else return new datalen*/
static int BarcodeCode128Adapter(uint8_t *data, uint32_t datalen, uint32_t databufsize)
{
    int i = 0, j = 0, CODE128C = 0, ret = 0;
    uint8_t *adapter = NULL;

    if(data == NULL || datalen == 0 || databufsize < datalen)
        return -1;
    adapter = (uint8_t *)malloc(datalen*2);
    if(adapter == NULL)
        return -1;
    for(i=0; i<datalen; i++)
    {
        if(data[i] == 123 && i+1 < datalen)
        {
            if(data[i + 1] == 67) //CODE128C
            {
                CODE128C = 1;
            }
            else
            {
                CODE128C = 0;
                if(data[i+1] == 123)  //"{{" -> "{"
                {
                    adapter[j++] = data[i+1];
                }
            }
            i+=1;
        }
        else
        {
            if(CODE128C == 0)
                adapter[j++] = data[i];
            else
            {
                if(data[i] < 100)  //change CODE128C, sample: 12,34 ->49,50,51,52 
                {
                    adapter[j++] = 48 + data[i]/10;
                    adapter[j++] = 48 + data[i]%10;
                }
                else
                {
                    ret = -1;
                    break;
                }
            }
        }            
    }   
    if(ret == 0)
    {
        if(j > 0 && j <= databufsize)
        {
            memcpy(data, adapter, j);
            ret = j;
        }
        else
        {
            ret = -1;
        }
    }
    free(adapter);
    
    return ret;
}

static int GenerateBarcode(void)
{
	struct zint_symbol *symbol;
	int type, column, i, j, x_size, text_length;
	uint32_t module, module_size, bit_pos, font_width, white_space, max_dots, bit_pos_temp;

	switch (Cmd_List0) {
	case 0:
	case 65: type = BARCODE_UPCA;        break; /* UPC-A */
	case 1:
	case 66: type = BARCODE_UPCE;        break; /* UPC-E */
	case 2:
	case 67: type = BARCODE_EANX;        break; /* EAN13 */
	case 3:
	case 68: type = BARCODE_EANX;        break; /* EAN8 */
	case 4:
	case 69: type = BARCODE_CODE39;      break; /* CODE39 */
	case 5:
	case 70: type = BARCODE_C25INTER;    break; /* Interleaved 2 of 5 */
	case 6:
	case 71: type = BARCODE_CODABAR;     break; /* CODABAR */
	case 72: type = BARCODE_CODE93;      break; /* CODE93 */
	case 73: type = BARCODE_CODE128;     break; /* CODE128 */
	case 81: type = BARCODE_C25MATRIX;   break; /* Matrix 2 of 5 */
	case 82: type = BARCODE_C25IND;      break; /* Industrial 2 of 5 */
	case 83: type = BARCODE_C25IATA;     break; /* IATA 2 of 5 */
	case 84: type = BARCODE_C25LOGIC;    break; /* Datalogic 2 of 5 */
	case 85: type = BARCODE_CODE11;      break; /* CODE11 */
	case 86: type = BARCODE_EXCODE39;    break; /* CODE39 Extended */
	case 87: type = BARCODE_RSS14;       break; /* GS1 DataBar */
	case 88: type = BARCODE_RSS_EXP;     break; /* GS1 DataBar Expanded */
	case 89: type = BARCODE_MSI_PLESSEY; break; /* MSI Plessey */
	default:
		return 0;
	}

	if (type == BARCODE_CODE128) {
        i = BarcodeCode128Adapter(DataBuffer, Cmd_List2, sizeof(DataBuffer));
        if(i <= 0)
        {
            Cmd_List2 = 0;
            return 0;
        }
        else
            Cmd_List2 = i;
	}

	/* Check data length */
	switch (type) {
	case BARCODE_UPCA:
		if (Cmd_List2 == 11 || Cmd_List2 == 12) {
			Cmd_List2 = 11;
			break;
		}
		return 0;
	case BARCODE_UPCE:
		if (Cmd_List2 == 6)
			break;
		if (Cmd_List2 == 7 || Cmd_List2 == 8) {
			if (DataBuffer[0] != '0')
				return 0;
			Cmd_List2 = 6;
			memmove(DataBuffer, DataBuffer + 1, Cmd_List2);
			break;
		}
		return 0;
	case BARCODE_EANX:
		if (Cmd_List2 == 7 || Cmd_List2 == 8) {
			Cmd_List2 = 7;
			break;
		}
		if (Cmd_List2 == 12 || Cmd_List2 == 13) {
			Cmd_List2 = 12;
			break;
		}
		return 0;
	default:
		break;
	}

	switch (type) {
	case BARCODE_UPCA:
	case BARCODE_UPCE:
	case BARCODE_EANX:
		white_space = 20;
		break;
	default:
		white_space = 0;
		break;
	}

	symbol = ZBarcode_Create();
	if (symbol == NULL)
		return 0;

	symbol->symbology = type;

	if (ZBarcode_Encode_and_Buffer(symbol, DataBuffer, Cmd_List2, 0) != 0) {
		ZBarcode_Delete(symbol);
		return 0;
	}

	memset(&Barcode.Bitmap, 0, sizeof(Barcode.Bitmap));

	max_dots = Settings.printAreaWidth();
	module_size = Barcode.ModuleSize;
	while ((symbol->width * module_size + white_space * 2) > max_dots && module_size > 1)
		module_size--;

	for (i = 1, module = 0x80000000; i < (int)module_size; i++)
		module |= (module >> 1);

	x_size = symbol->bitmap_width / (symbol->width + symbol->whitespace_width * 2);

	/*
	 * Body
	 */
	i = symbol->whitespace_width * (3 * x_size);
	for (column = 0, bit_pos = white_space + Settings.leftMargin(); column < symbol->width; column++, bit_pos += module_size) {
		if (symbol->bitmap[i] == 0)
			Dotline_CopyU32(Barcode.Bitmap.Body, bit_pos, module, module, false);
		i += (3 * x_size);
	}

	/*
	 * Extended part
	 */
	if (white_space > 0) {
		i = (symbol->bitmap_width * symbol->height + symbol->whitespace_width) * (3 * x_size);
		for (column = 0, bit_pos = white_space + Settings.leftMargin(); column < symbol->width; column++, bit_pos += module_size) {
			if (symbol->bitmap[i] == 0)
				Dotline_CopyU32(Barcode.Bitmap.Ext, bit_pos, module, module, false);
			i += (3 * x_size);
		}
	}

	Barcode.SymbolWidth = symbol->width * module_size + white_space * 2;

	/*
	 * HRI characters
	 */

	font_width = HRIfontColumns();

	if (type == BARCODE_UPCA) {
		/*
		 * UPC-A layout:
		 *
		 * white space|start| S |XXXXX|mid|XXXXX| C | end |white space
		 * -----------+-----+---+-----+---+-----+---+-----+-----------
		 * WHITE_SPACE  (3)  (7) (5*7) (5) (5*7) (7)  (3)  WHITE_SPACE
		 */

		bit_pos_temp = IntegerDiv(white_space, font_width, 2, 0);
		if(bit_pos_temp != -1){
			bit_pos = bit_pos_temp;
			DrawHRIfont(bit_pos, symbol->text[0]);
		}else{
			goto delete_syb;
		}

		bit_pos_temp = IntegerDiv(35 * module_size, 5 * font_width, 2, 0);
		if(bit_pos_temp != -1){
			bit_pos = white_space + 10 * module_size + bit_pos_temp;
			for (j = 0; j < 5; j++)
				DrawHRIfont(bit_pos + font_width * j, symbol->text[j + 1]);
		}else{
			goto delete_syb;
		}

		bit_pos_temp = IntegerDiv(35 * module_size, 5 * font_width, 2, 1);
		if(bit_pos_temp != -1){
			bit_pos = white_space + 50 * module_size + bit_pos_temp;
			for (j = 0; j < 5; j++)
				DrawHRIfont(bit_pos + font_width * j, symbol->text[j + 6]);
		}else{
			goto delete_syb;
		}

		bit_pos_temp = IntegerDiv(white_space, font_width, 2, 1);
		if(bit_pos_temp != -1){
			bit_pos = white_space + 95 * module_size + bit_pos_temp;
			DrawHRIfont(bit_pos, symbol->text[11]);
		}else{
			goto delete_syb;
		}
	}

	else if (type == BARCODE_UPCE) {
		/*
		 * UPC-E layout:
		 *
		 * white space|start|XXXXXX| end |white space
		 * -----------+-----+------+-----+-----------
		 * WHITE_SPACE  (3)   (6*7)  (6)  WHITE_SPACE
		 */

		bit_pos_temp = IntegerDiv(white_space, font_width, 2, 0);
		if(bit_pos_temp != -1){
			bit_pos = bit_pos_temp;
			DrawHRIfont(bit_pos, symbol->text[0]);
		}else{
			goto delete_syb;
		}

		bit_pos_temp = IntegerDiv(42 * module_size, 6 * font_width, 2, 0);
		if(bit_pos_temp != -1){
			bit_pos = white_space + 3 * module_size + bit_pos_temp;
			for (j = 0; j < 6; j++)
				DrawHRIfont(bit_pos + font_width * j, symbol->text[j + 1]);
		}else{
			goto delete_syb; 
		}

		bit_pos_temp = IntegerDiv(white_space, font_width, 2, 1);
		if(bit_pos_temp != -1){
			bit_pos = white_space + 51 * module_size + bit_pos_temp;
			DrawHRIfont(bit_pos, symbol->text[7]);
		}else{
			goto delete_syb;
		}
	}

	else if (Cmd_List0 == 2 || Cmd_List0 == 67) {
		/*
		 * EAN-13 layout:
		 *
		 * white space|start|S-2|XXXXX|mid|XXXXX| C | end |white space
		 * -----------+-----+---+-----+---+-----+---+-----+-----------
		 * WHITE_SPACE  (3)  (7) (5*7) (5) (5*7) (7)  (3)  WHITE_SPACE
		 */

		bit_pos_temp = IntegerDiv(white_space, font_width, 2, 0);
		if(bit_pos_temp != -1){
			bit_pos = bit_pos_temp;
			DrawHRIfont(bit_pos, symbol->text[0]);
		}else{
			goto delete_syb;
		}

		bit_pos_temp = IntegerDiv(42 * module_size, 6 * font_width, 2, 0);
		if(bit_pos_temp != -1){
			bit_pos = white_space + 3 * module_size + bit_pos_temp;
			for (j = 0; j < 6; j++)
				DrawHRIfont(bit_pos + font_width * j, symbol->text[j + 1]);
		}else{
			goto delete_syb;
		}

		bit_pos_temp = IntegerDiv(42 * module_size, 6 * font_width, 2, 1);
		if(bit_pos_temp != -1){
			bit_pos = white_space + 50 * module_size + bit_pos_temp;
			for (j = 0; j < 6; j++)
				DrawHRIfont(bit_pos + font_width * j, symbol->text[j + 7]);
		}else{
			goto delete_syb;
		}
	}

	else if (Cmd_List0 == 3 || Cmd_List0 == 68) {
		/*
		 * EAN-8 layout:
		 *
		 * white space|start| XXXX|mid| XXXX| end |white space
		 * -----------+-----+-----+---+-----+-----+-----------
		 * WHITE_SPACE  (3)  (4*7) (5) (4*7)  (3)  WHITE_SPACE
		 */

		bit_pos_temp = IntegerDiv(28 * module_size, 4 * font_width, 2, 0);
		if(bit_pos_temp != -1){
			bit_pos = white_space + 3 * module_size + bit_pos_temp;
			for (j = 0; j < 4; j++)
				DrawHRIfont(bit_pos + font_width * j, symbol->text[j]);
		}else{
			goto delete_syb;
		}

		bit_pos_temp = IntegerDiv(28 * module_size, 4 * font_width, 2, 1);
		if(bit_pos_temp != -1){
			bit_pos = white_space + 36 * module_size + bit_pos_temp;
			for (j = 0; j < 4; j++)
				DrawHRIfont(bit_pos + font_width * j, symbol->text[j + 4]);
		}else{
			goto delete_syb;
		}
	}

	else {
		text_length = strlen((char *)symbol->text);

		bit_pos_temp = IntegerDiv(Barcode.SymbolWidth, text_length * font_width, 2, 0);
		if(bit_pos_temp != -1){
			bit_pos = bit_pos_temp;
			for (j = 0; j < text_length; j++)
				DrawHRIfont(bit_pos + font_width * j, symbol->text[j]);
		}else{
			goto delete_syb;
		}
	}

	delete_syb:
		ZBarcode_Delete(symbol);

	return 1;
}

static void GetBarcodeData(void)
{
	if (Cmd_List1 == 0) { /* 条码数据以NUL结束 */
		if (Cmd_Data != 0) {
			if (Cmd_List2 < 255)
				DataBuffer[Cmd_List2++] = Cmd_Data;
			return;
		}
	}
	else { /* 条码数据长度为Cmd_List1 */
		DataBuffer[Cmd_List2++] = Cmd_Data;
		if (Cmd_List2 < Cmd_List1)
			return;
	}

	if (GenerateBarcode())
		lineBuffer->printBarcode();
	ResetNextFun();
}

static void GS_k_m_n(void)
{
	if (Cmd_Data > 0) {
		Cmd_List1 = Cmd_Data;
		pNextFun = GetBarcodeData;
	}
	else
		ResetNextFun();
}

static void GS_k_m(void)
{
	Cmd_List0 = Cmd_Data;
	Cmd_List1 = 0; /* 需要接收的数据长度，为0表示用NUL结束 */
	Cmd_List2 = 0; /* 已接收数据计数 */

	if (Cmd_Data <= 6) {
		pNextFun = GetBarcodeData;
		return;
	}

	if ((Cmd_Data >= 65 && Cmd_Data <= 73) ||
		(Cmd_Data >= 81 && Cmd_Data <= 89)) {
		pNextFun = GS_k_m_n;
		return;
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | GS r                                         |
 *----------------------------------------------*/
static void TransmitStatus(void)
{
	const print_status_t *s = print_status();
	uint8_t status = 0;

	if (Cmd_Data >= 49 && Cmd_Data <= 52)
		Cmd_Data -= 48;

	switch (Cmd_Data) {
	case 1: /* Transmit paper sensor status */
		if (s->paper_running_out)
			status |= 3;
		if (s->no_paper)
			status |= 3 << 2;
		break;
	case 2: /* Transmit drawer kick-out connector status */
	case 4: /* Transmit ink status */
		break;
	default:
		goto _exit;
	}

	ReplyToHost(&status, sizeof(status), -1);

_exit:
	ResetNextFun();
}

/*----------------------------------------------*
 | GS v 0                                       |
 *----------------------------------------------*/
/*
 * Cmd_List0: 模式
 * Cmd_List1: 每行字节数
 * Cmd_List2: 行数
 * Cmd_List3: 每行接收字节数计数
 * Cmd_List4: 接收行计数
 * Cmd_List5: 起始行
 */
static void PrintRasterBitImage_Data(void)
{
	uint32_t row, pos, n, bitext, bits, rm;

	Cmd_List3++; /* 每行接收字节数计数 */

	rm = Settings.rightMargin();

	if (lineBuffer->EndPos < rm) { /* 一点行未满 */
		if (Cmd_Data != 0) {
			pos = lineBuffer->EndPos;
			row = MAX_LINE_HEIGHT - (Cmd_List5 - lineBuffer->Height);
			Cmd_Data <<= 24;
			if (Cmd_List0 & 0x01) { /* 倍宽 */
				DotsExtension(Cmd_Data, 8, 2, &bitext);
				bits = 16;
			}
			else {
				bitext = Cmd_Data;
				bits = 8;
			}
			if ((lineBuffer->EndPos + bits) > rm) { /* 超出打印范围 */
				/* 将超出范围部分去掉 */
				n = (rm - lineBuffer->EndPos);
				n = 32 - n;
				bitext >>= n;
				bitext <<= n;
				lineBuffer->EndPos = rm;
			}
			else
				lineBuffer->EndPos += bits;

			Dotline_SetDots(lineBuffer->dotlineAt(row), pos, bitext, 0xFFFF0000, false);
			if (Cmd_List0 & 0x02)
				Dotline_SetDots(lineBuffer->dotlineAt(row + 1), pos, bitext, 0xFFFF0000, false);
		}
		else {
			lineBuffer->EndPos += (Cmd_List0 & 0x01) ? 16 : 8; /* 倍宽 */
			if (lineBuffer->EndPos > rm) /* 超出打印范围 */
				lineBuffer->EndPos = rm;
		}
	}

	if (Cmd_List3 >= Cmd_List1) { /* 一行数据接收完全 */
		Cmd_List3 = 0; /* 每行接收字节数计数清零 */
		Cmd_List4++; /* 已接收行数计数 */

		lineBuffer->Height += (Cmd_List0 & 0x02) ? 2 : 1; /* 倍高 */
		lineBuffer->Top = MAX_LINE_HEIGHT - Cmd_List5;
		lineBuffer->Bottom = lineBuffer->Top + lineBuffer->Height - 1;

		if (Cmd_List4 < Cmd_List2) {
			if (lineBuffer->Height >= 24) {
				lineBuffer->print(0);
				Cmd_List5 = Cmd_List2 - Cmd_List4; /* 剩余未接收的行数 */
				if (Cmd_List0 & 0x02)
					Cmd_List5 <<= 1;
				if (Cmd_List5 > 24)
					Cmd_List5 = 24;
			}
			else
				lineBuffer->EndPos = Settings.leftMargin();
		}
		else {
			if (lineBuffer->Height > 0)
				lineBuffer->print(0);

#if 0
			buf[0] = 0x09;
			*((uint16_t *)(buf + 1)) = 0;
			printer_send_command(buf, 3);
#endif

//			SendPrinterCommand1(0x0B, 0);
			Cmd_DontClearBuffer = 0;

			ResetNextFun();
		}
	}
}

/*
 * Cmd_List0: 模式
 * Cmd_List1: 每行字节数
 * Cmd_List2: 行数
 */
static void PrintRasterBitImage_yH(void)
{
	Cmd_List2 |= Cmd_Data << 8; /* 行数 */
	if (Cmd_List2 > 0) {
		Cmd_DontClearBuffer = 1;
		if (!lineBuffer->isEmpty())
			lineBuffer->printLine();

		Cmd_List3 = 0;
		Cmd_List4 = 0;
		Cmd_List5 = Cmd_List2;
		if (Cmd_List0 & 0x02)
			Cmd_List5 <<= 1;
		if (Cmd_List5 > 24)
			Cmd_List5 = 24;

#if 0
		buf[0] = 0x09;
		*((uint16_t *)(buf + 1)) = 500;
		printer_send_command(buf, 3);
#endif

//		SendPrinterCommand1(0x0B, 1);

		pNextFun = PrintRasterBitImage_Data;
	}
	else
		ResetNextFun();
}

static void PrintRasterBitImage_yL(void)
{
	Cmd_List2 = Cmd_Data;
	pNextFun = PrintRasterBitImage_yH;
}

static void PrintRasterBitImage_xH(void)
{
	Cmd_List1 |= Cmd_Data << 8; /* 每行字节数 */
	if (Cmd_List1 > 0)
		pNextFun = PrintRasterBitImage_yL;
	else
		ResetNextFun();
}

static void PrintRasterBitImage_xL(void)
{
	Cmd_List1 = Cmd_Data;
	pNextFun = PrintRasterBitImage_xH;
}

static void PrintRasterBitImage_Mode(void)
{
	if (Cmd_Data >= 48 && Cmd_Data <= 51)
		Cmd_Data -= 48;
	if (Cmd_Data < 4) {
		Cmd_List0 = Cmd_Data;
		pNextFun = PrintRasterBitImage_xL;
	}
	else
		ResetNextFun();
}

static void PrintRasterBitImage(void)
{
	if (Cmd_Data == 48)
		pNextFun = PrintRasterBitImage_Mode;
	else
		ResetNextFun();
}

/*----------------------------------------------*
 | GS w                                         |
 *----------------------------------------------*/
static void SetBarcodeWidth(void)
{
	if (Cmd_Data >= 2 && Cmd_Data <= 6)
		Barcode.ModuleSize = Cmd_Data;

	ResetNextFun();
}

void PrintNVMonoImage(uint8_t kc1, uint8_t kc2, uint8_t xscale, uint8_t yscale)
{
	char pathname[64];
	uint8_t buf[16];
	uint32_t i, j, k, w, dots, width, height, pitch;
	uint32_t pos, endpos, row, bitext;
	int fd = -1, ret;

	if (kc1 < 32 || kc1 > 126)
		goto _exit;
	if (kc2 < 32 || kc2 > 126)
		goto _exit;
	if (xscale != 1 && xscale != 2)
		goto _exit;
	if (yscale != 1 && yscale != 2)
		goto _exit;

	sprintf(pathname, NV_GRAPHICS_DIR "%02x_%02x.bin", kc1, kc2);
	fd = open(pathname, O_RDONLY);
	if (fd < 0) {
		LogError("Failed to open \"%s\".", pathname);
		goto _exit;
	}

	/* 读取文件头 */
	ret = read(fd, buf, 16);
	if (ret != 16) {
		LogError("Failed to read \"%s\".", pathname);
		goto _exit;
	}

	/* 获取图像宽度和高度 */
	width  = ((uint16_t)buf[1] << 8) | ((uint16_t)buf[0]);
	height = ((uint16_t)buf[3] << 8) | ((uint16_t)buf[2]);
	pitch  = (width + 7) >> 3; /* 一行数据占用的字节数 */
	/* 计算打印一点行图像所需要的宽度 */
	endpos = Settings.leftMargin() + width * xscale;
	if (endpos > Settings.rightMargin()) {
		endpos = Settings.rightMargin();
		width = (endpos - Settings.leftMargin()) / xscale; /* 计算实际可以打印的图像宽度 */
	}
	if (width == 0 || height == 0)
		goto _exit;

	if (!lineBuffer->isEmpty())
		lineBuffer->printLine();

//	SendPrinterCommand1(0x0B, 1);

	row = MAX_LINE_HEIGHT - 24; /* 一次打印24点行 */
	for (i = 0; i < height; i++) {
		read(fd, linebuf, pitch);
		j = 0;
		w = width;
		pos = Settings.leftMargin();
		while (w > 0) {
			dots = (w < 8) ? w : 8;
			w -= dots;
			if (linebuf[j] != 0) {
				k = linebuf[j] << 24;
				DotsExtension(k, dots, xscale, &bitext);
				Dotline_SetDots(lineBuffer->dotlineAt(row), pos, bitext, 0xFFFF0000, false);
				if (yscale == 2) /* 倍高 */
					Dotline_SetDots(lineBuffer->dotlineAt(row + 1), pos, bitext, 0xFFFF0000, false);
			}
			pos += dots * xscale;
			j++;
		}
		row += yscale;
		if (row == MAX_LINE_HEIGHT) {
			lineBuffer->EndPos = endpos;
			lineBuffer->setHeight(24);
			lineBuffer->print(0);
			row = MAX_LINE_HEIGHT - 24;
		}
	}
	if (row > MAX_LINE_HEIGHT - 24) {
		lineBuffer->EndPos = endpos;
		lineBuffer->Height = row - (MAX_LINE_HEIGHT - 24);
		lineBuffer->Top = MAX_LINE_HEIGHT - 24;
		lineBuffer->Bottom = lineBuffer->Top + lineBuffer->Height - 1;
		lineBuffer->print(0);
	}

//	SendPrinterCommand1(0x0B, 0);

	LogInfo("Complete.");

_exit:
	if (fd >= 0)
		close(fd);
}

void PrintNVGrayscaleImage(uint8_t n, uint16_t s, uint8_t g, uint8_t m)
{
	char pathname[64];
	uint8_t *srcdata = NULL, *dstdata = NULL;
	uint8_t buf[16], dotline[BYTES_PER_LINE_MAX], mask;
	uint32_t i, j, k, width, height, depth, offset;
	uint32_t src_offset, dst_offset, copy_size;
	uint32_t srcsize, dstsize;
	int fd = -1, ret, shift_bits;

	if (n == 0)
		goto _exit;
	if (!(m >= 1 && m <= 8) && !(m >= 0x80 && m <= 0x8A))
		goto _exit;

	sprintf(pathname, "/data/PUB/image_%03u.bin", n);
	fd = open(pathname, O_RDONLY);
	if (fd < 0) {
		LogError("Failed to open \"%s\".", pathname);
		goto _exit;
	}

	ret = read(fd, buf, 16);
	if (ret != 16) {
		LogError("Failed to read \"%s\".", pathname);
		goto _exit;
	}

	width  = ((uint16_t)buf[1] << 8) | ((uint16_t)buf[0]);
	height = ((uint16_t)buf[3] << 8) | ((uint16_t)buf[2]);
	depth  = buf[4];

	src_offset = 0;
	dst_offset = 0;
	copy_size = width;
	shift_bits = s & 0x7FFF;
	if (s & 0x8000)
		shift_bits = -shift_bits;

#if 0
	i = (s & 0x7FFF);
	if (i > 0 && i < DOTS_PER_LINE) {
		if (s & 0x8000) { /* Shift left */
			src_offset = i;
			dst_offset = 0;
			copy_size = (width - i);
		}
		else { /* Shift right */
			src_offset = 0;
			dst_offset = i;
			copy_size = (width < (DOTS_PER_LINE - i)) ? width : (DOTS_PER_LINE - i);
		}
	}
#endif

	SendPrinterCommand1(0x08, g);
#define P (Settings.HeaderFooterImage.Header)
	if (P.dots_back > 0 || Settings.lineSpacing() == 0)
		lineBuffer->print(1); /*防止回退后走纸方向未改变，导致反向打印*/
#undef P

	if (m >= 1 && m <= 8) { /* Grayscale mode */
		if (depth == 0 || depth > 8)
			goto _exit_1;
		if (m > depth)
			m = depth;
		SendPrinterCommand1(0x04, m); /* Enter grayscale mode */

		if (copy_size > Settings.printAreaWidth())
			copy_size = Settings.printAreaWidth();
		if (Settings.Alignment == 1) /* Align center */
			dst_offset += (Settings.printAreaWidth() - copy_size) >> 1;
		else if (Settings.Alignment == 2) /* Align right */
			dst_offset += (Settings.printAreaWidth() - copy_size);
		dst_offset += Settings.leftMargin();

		offset = 0;
		memset(DataBuffer, 0xFF, sizeof(DataBuffer));

		for (i = 0; i < height; i++) {
			read(fd, linebuf, width);
			memcpy(DataBuffer + offset + dst_offset, linebuf + src_offset, copy_size);
			offset += DOTS_PER_LINE;
			if (offset == BYTES_PER_CHAR_ROW) {
				PrnDotLine(DataBuffer, offset, PRN_DATA_GRAYSCALE);
				offset = 0;
				memset(DataBuffer, 0xFF, sizeof(DataBuffer));
			}
		}
		if (offset > 0)
			PrnDotLine(DataBuffer, offset, PRN_DATA_GRAYSCALE);

		SendPrinterCommand1(0x04, 0); /* Exit grayscale mode */
	}
	else if (m >= 0x80 && m <= 0x87) { /* Mono mode */
		m = (1 << (m & 7));
		offset = 0;
		memset(DataBuffer, 0, sizeof(DataBuffer));

		for (i = 0; i < height; i++) {
			read(fd, linebuf, width);
			memset(dotline, 0, sizeof(dotline));
			for (j = 0, k = 0, mask = 0x80; j < width; j++) {
				dotline[k] |= (linebuf[j] & m) ? 0 : mask;
				if ((mask >>= 1) == 0) {
					mask = 0x80;
					k++;
				}
			}
			if (shift_bits != 0) {
				if (shift_bits < 0)
					Dotline_Lshift((uint32_t *)dotline, WORDS_PER_LINE,    -shift_bits);
				else
					Dotline_Rshift((uint32_t *)dotline, WORDS_PER_LINE, 0,  shift_bits);
			}
			memcpy(DataBuffer + offset, dotline, BYTES_PER_LINE);
			offset += BYTES_PER_LINE;
			if (offset == BYTES_PER_CHAR_ROW) {
				PrnDotLine(DataBuffer, offset, PRN_DATA_MONO);
				offset = 0;
				memset(DataBuffer, 0, sizeof(DataBuffer));
			}
		}
		if (offset > 0)
			PrnDotLine(DataBuffer, offset, PRN_DATA_MONO);
	}
	else if (m >= 0x88 && m <= 0x8A) { /* Dither to mono */
		srcsize = width * height;
		srcdata = (uint8_t *)malloc(srcsize);
		if (!srcdata)
			goto _exit_1;

		dstsize = BYTES_PER_LINE * height;
		dstdata = (uint8_t *)malloc(dstsize);
		if (!dstdata)
			goto _exit_1;

		read(fd, srcdata, srcsize);
		memset(dstdata, 0, dstsize);

#if 0
		buf[0] = 0x09;
		*((uint16_t *)(buf + 1)) = 500;
		printer_send_command(buf, 3);
#endif

		SendPrinterCommand1(0x0B, 1);

		if (m == 0x88) { /* Diffuse */
			DiffuseDither((uint32_t *)dstdata, srcdata, width, height, shift_bits);
		}
		else if (m == 0x89) { /* Ordered */
			OrderedDither((uint32_t *)dstdata, srcdata, width, height, shift_bits);
		}
		else { /* Threshold */
			ThresholdDither((uint32_t *)dstdata, srcdata, width, height, shift_bits);
		}

		if (width > Settings.printAreaWidth())
			width = Settings.printAreaWidth();
		if (Settings.Alignment == 1) /* Align center */
			shift_bits = (Settings.printAreaWidth() - width) >> 1;
		else if (Settings.Alignment == 2) /* Align right */
			shift_bits = (Settings.printAreaWidth() - width);
		shift_bits += Settings.leftMargin();

		if (shift_bits > 0) {
			uint32_t *p = (uint32_t *)dstdata;
			for (i = 0; i < height; i++, p += WORDS_PER_LINE)
				Dotline_Rshift(p, WORDS_PER_LINE, 0, shift_bits);
		}

		offset = 0;
		while (dstsize > 0) {
			i = (dstsize > BYTES_PER_CHAR_ROW) ? BYTES_PER_CHAR_ROW : dstsize;
			PrnDotLine(dstdata + offset, i, PRN_DATA_MONO);
			offset += i;
			dstsize -= i;
		}

#if 0
		buf[0] = 0x09;
		*((uint16_t *)(buf + 1)) = 0;
		printer_send_command(buf, 3);
#endif

		SendPrinterCommand1(0x0B, 0);
	}

_exit_1:
	SendPrinterCommand1(0x08, 0);

	LogInfo("Complete.");

_exit:
	if (fd >= 0)
		close(fd);
	if (dstdata)
		free(dstdata);
	if (srcdata)
		free(srcdata);
}

void GS_Proc(void)
{
	switch (Cmd_Data) {
	case 0x21: /* [GS !] Select character size */
		pNextFun = SetCharSize;
		break;
	case 0x24: /* [GS $] Set absolute vertical print position in page mode */
		Cmd_List0 = 0;
		pNextFun = SetPageModeAbsoluteVerticalPrintPosition;
		break;
	case 0x28: /* [GS (] Multifunction */
		pNextFun = GS_LP;
		break;
	case 0x2A: /* [GS *] Define downloaded bit image */
		pNextFun = DefineDownloadedBitImage_x;
		break;
	case 0x2F: /* [GS /] Print downloaded bit image */
		pNextFun = PrintDownloadedBitImage_m;
		break;
	case 0x42: /* [GS B] Turn white/black reverse print mode on/off */
		pNextFun = SetBlackWhiteReverseMode;
		break;
	case 0x48: /* [GS H] Select print position of HRI characters */
		pNextFun = SetBarcodeHRIPos;
		break;
	case 0x49: /* [GS I] Transmit printer ID */
		pNextFun = TransmitPrinterID;
		break;
	case 0x4C: /* [GS L] Set left margin */
		pNextFun = SetLeftMargin_nL;
		break;
	case 0x54: /* [GS T] Set print position to the beginning of print line */
		pNextFun = MoveToBegin;
		break;
	case 0x56: /* [GS V] Select cut mode and cut paper */
		pNextFun = CutPaper;
		break;
	case 0x57: /* [GS W] Set print area width */
		pNextFun = SetPrintAreaWidth_nL;
		break;
	case 0x5C: /* [GS \] Set relative vertical print position in page mode */
		Cmd_List0 = 0;
		pNextFun = SetPageModeRelativeVerticalPrintPosition;
		break;
	case 0x62: /* [GS b] Turn smoothing mode on/off */
		SkipNBytes(1);
		break;
	case 0x66: /* [GS f] Select font for HRI characters */
		pNextFun = SetBarcodeHRIFont;
		break;
	case 0x68: /* [GS h] Set bar code height */
		pNextFun = SetBarcodeHeight;
		break;
	case 0x6B: /* [GS k] Print bar code */
		pNextFun = GS_k_m;
		break;
	case 0x72: /* [GS r] Transmit status */
		pNextFun = TransmitStatus;
		break;
	case 0x76: /* [GS v 0] Print raster bit image */
		pNextFun = PrintRasterBitImage;
		break;
	case 0x77: /* [GS w] Set bar code width */
		pNextFun = SetBarcodeWidth;
		break;
	default:
		ResetNextFun();
		break;
	}
}
