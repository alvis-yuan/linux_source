#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libcommon.h"
#include "simple_timer.h"
#include "Config.h"
#include "HarfBuzz.h"
#include "Instruct_Proc.h"
#include "LineBuffer.h"
#include "PageBuffer.h"
#include "Runtime_Data.h"
#include "Util.h"
#include "WordSet.h"
#include "printer.h"

#define NODE_CASH_OPEN	"/sys/class/sunmi/base/gpio_cash_open"
#define NODE_CASH_SW	"/sys/class/sunmi/base/gpio_cash_sw"

#define PRTLINEBUFSIZE	(WORDS_PER_LINE * 8)

typedef struct {
	uint32_t state;
	uint32_t void_back;
	uint32_t line_index;
	uint32_t Array_Buf[0];
} PRINT_DIVISION_BUF;

static uint8_t cashbox_open_timer;

static PRINT_DIVISION_BUF *PrnDivBuf = NULL;
static size_t SizeOfPrnDivBuf;
static int PrnFd;

static int read_node(const char *node, void *data, uint32_t len)
{
	int fd, ret;

	fd = open(node, O_RDONLY|O_CLOEXEC);
	if (fd == -1)
		return -1;
	ret = read(fd, data, len);
	close(fd);
	return ret;
}

static int write_node(const char *node, const void *data, uint32_t len)
{
	int fd, ret;

	fd = open(node, O_WRONLY|O_APPEND|O_CLOEXEC);
	if (fd == -1)
		return -1;
	ret = write(fd, data, len);
	close(fd);
	return (ret == (int)len) ? 0 : -1;
}

static void cashbox_timer_handler(uint32_t context)
{
	write_node(NODE_CASH_OPEN, "0", 1);
}

static inline int write_to_kernel(void)
{
	SavePrintTaskBitmap_Append(PrnDivBuf->Array_Buf, BYTES_PER_LINE * (8 - PrnDivBuf->void_back));
	return write(PrnFd, PrnDivBuf, SizeOfPrnDivBuf);
}

int PrnDotLine(void *data, uint32_t dataLength, int dataType)
{
	uint32_t linecount, j, k, p, mask;
	int ret;

	if (dataLength == 0)
		return write(PrnFd, PrnDivBuf, 0);

	if (dataLength % BYTES_PER_LINE != 0) {
		LogError("Invalid dataLength.");
		return -1;
	}

	PrnDivBuf->state = 1;
	PrnDivBuf->line_index = 0;
	linecount = dataLength / BYTES_PER_LINE;

	if (Settings.BitsPerDot > 0 && dataType == PRN_DATA_MONO) {
		uint8_t *dst = (uint8_t *)PrnDivBuf->Array_Buf, v;
		uint32_t *src = (uint32_t *)data;

		v = (Settings.BlackWhiteReverseMode == 0 && Settings.Color != 0) ? 0x7F : 0xFF;
		while (linecount > 0) {
			p = 0;
			for (j = 0; j < WORDS_PER_LINE; j++) {
				mask = 0x80000000;
				for (k = 0; k < 32; k++) {
					dst[p++] = (src[j] & mask) ? v : 0x00;
					mask >>= 1;
				}
			}
			src += WORDS_PER_LINE;
			PrnDivBuf->void_back = 0;
			if ((ret = write_to_kernel()) != 0)
				return ret;
			linecount--;
		}
		return 0;
	}
	else {
		uint8_t *src = (uint8_t *)data;

		while (linecount > 0) {
			k = (linecount > 8) ? 8 : linecount;
			memcpy(PrnDivBuf->Array_Buf, src, k * BYTES_PER_LINE);
			PrnDivBuf->void_back = 8 - k;
			if ((ret = write_to_kernel()) != 0)
				return ret;
			src += k * BYTES_PER_LINE;
			linecount -= k;
		}
		return 0;
	}
}

int PrnBlankLines(uint32_t lines)
{
	uint32_t n;
	int ret;

	memset(PrnDivBuf, 0, SizeOfPrnDivBuf);
	PrnDivBuf->state = 1;
	PrnDivBuf->line_index = 0;

	while (lines > 0) {
		if (Settings.BitsPerDot == 0) {
			n = (lines > 8) ? 8 : lines;
			PrnDivBuf->void_back = 8 - n;
		}
		else {
			n = 1;
			PrnDivBuf->void_back = 0;
		}
		if ((ret = write_to_kernel()) != 0)
			return ret;
		lines -= n;
	}
	return 0;
}

int PrnDuplicateLines(void *dotline, uint32_t lines)
{
	uint32_t i, n, *p;
	int ret;

	PrnDivBuf->state = 1;
	PrnDivBuf->line_index = 0;

	if (Settings.BitsPerDot == 0) {
		n = (lines > 8) ? 8 : lines;
		p = PrnDivBuf->Array_Buf;
		for (i = 0; i < n; i++) {
			memcpy(p, dotline, BYTES_PER_LINE);
			p += WORDS_PER_LINE;
		}
		while (lines > 0) {
			n = (lines > 8) ? 8 : lines;
			PrnDivBuf->void_back = 8 - n;
			if ((ret = write_to_kernel()) != 0)
				return ret;
			lines -= n;
		}
	}
	else {
		p = PrnDivBuf->Array_Buf;
		memcpy(p, dotline, DOTS_PER_LINE);
		PrnDivBuf->void_back = 0;
		while (lines > 0) {
			if ((ret = write_to_kernel()) != 0)
				return ret;
			lines--;
		}
	}
	return 0;
}

int PrnGetStatus(PRN_STATUS *status)
{
	uint32_t dataOutLength = sizeof(PRN_STATUS);

	return PrnIoctl(PRN_CMD_GET_STATUS, NULL, 0, status, &dataOutLength);
}

int PrnIoctl(uint32_t cmd, void *dataIn, uint32_t dataInLength, void *dataOut, uint32_t *dataOutLength)
{
	uint32_t ms;
	int state = 0;

	switch (cmd) {
	case PRN_CMD_GET_STATUS:
		if (!dataOut || *dataOutLength < sizeof(PRN_STATUS))
			return -1;
		*dataOutLength = sizeof(PRN_STATUS);
		return ioctl(PrnFd, cmd, dataOut);

	case PRN_CMD_SEND_EMBEDDED_COMMAND:
		if (!dataIn || dataInLength == 0 || dataInLength >= 48)
			return -1;
		PrnDivBuf->state = 3;
		PrnDivBuf->void_back = dataInLength;
		PrnDivBuf->line_index = 0;
		memcpy(PrnDivBuf->Array_Buf, dataIn, dataInLength);
		return write(PrnFd, PrnDivBuf, SizeOfPrnDivBuf);

	case PRN_CMD_GET_BUFFER_ID:
		if (!dataOut || *dataOutLength < sizeof(uint32_t))
			return -1;
		*dataOutLength = sizeof(uint32_t);
		return ioctl(PrnFd, cmd, dataOut);

	case PRN_CMD_CUT:
	case PRN_CMD_SET_VP_EN_STATE:
	case PRN_CMD_SET_FEED_STATE:
	case PRN_CMD_SET_PAPER_NOT_TAKEN_ACTION:
	case PRN_CMD_SET_MOTOR_OVERHEATED_STATE:
		if (!dataIn || dataInLength < 1)
			return -1;
		return ioctl(PrnFd, cmd, *((uint8_t *)dataIn));

	case PRN_CMD_GET_PAPER_LOCATION:
		if (!dataOut || *dataOutLength < sizeof(print_paper_location_t))
			return -1;
		*dataOutLength = sizeof(print_paper_location_t);
		return ioctl(PrnFd, cmd, dataOut);

	case PRN_CMD_GET_TPH_VERSION:
		state = ioctl(PrnFd, cmd, 0);
		LogInfo("thermal_printer.ko version: %d.%d.%d",
			(state >> 16) & 0xFF,
			(state >> 8 ) & 0xFF,
			(state      ) & 0xFF);
		return 0;

	case PRN_CMD_GET_DOTS:
		if (!dataIn || dataInLength < 8 ||
			!dataOut || *dataOutLength < BYTES_PER_LINE)
			return -1;
		memcpy(dataOut, dataIn, 8);
		*dataOutLength = BYTES_PER_LINE;
		return ioctl(PrnFd, cmd, dataOut);

	case PRN_CMD_OPEN_CASHBOX:
		if (!dataIn || dataInLength < 5)
			return -1;
		if (simple_timer_is_running(cashbox_open_timer))
			return -1;
		ms = *((uint16_t *)(dataIn + 1));
		if (ms == 0)
			return -1;
		write_node(NODE_CASH_OPEN, "1", 1);
		simple_timer_start(cashbox_open_timer, ms, 0);
		return 0;

	case PRN_CMD_GET_VOLTAGE_ADC:
	case PRN_CMD_GET_PAPER_STATE:
		if (!dataOut || *dataOutLength < sizeof(int))
			return -1;
		state = ioctl(PrnFd, cmd, 0);
		if (state < 0)
			return -1;
		*((int *)dataOut) = state;
		*dataOutLength = sizeof(int);
		return 0;

	case PRN_CMD_SET_GRAY_LEVEL:
	case PRN_CMD_SET_PRINT_SPEED:
		if (!dataIn || dataInLength != sizeof(uint32_t))
			return -1;
		return ioctl(PrnFd, cmd, *((uint32_t *)dataIn));

	case PRN_CMD_CLEAR_BUFFER:
	case PRN_CMD_GET_CASHBOX_OPEN_COUNT:
	case PRN_CMD_FLUSH_RING_BUFFER:
	case PRN_CMD_CLEAR_PAPER_NOT_TAKEN:
		return ioctl(PrnFd, cmd, 0);

	case PRN_CMD_CONSOLE_RX_ENABLE:
	case PRN_CMD_CONSOLE_RX_DISABLE:
		break;
	case PRN_CMD_CLEAR_REPRINT_FLAG:
		return ioctl(PrnFd, cmd, 0);

	default:
		return ioctl(PrnFd, cmd, 0);
	}

	return -1;
}

int printer_clear_buffer(void)
{
	lineBuffer->clear();
	ResetNextFun();
	return PrnIoctl(PRN_CMD_CLEAR_BUFFER, NULL, 0, NULL, NULL);
}

int printer_send_command(uint8_t *buf, uint32_t len)
{
	int CuttingAutoLogo = 0;
	if (len == 0 || len >= 48)
		return -1;

	CuttingAutoLogo = app_setting_get_int("printer", "CuttingAutoLogo", 0);
	if (buf[0] == 0x21 || buf[0] == 0x22) { /* 0x21:立即切纸; 0x22:延迟切纸 */
		/* Patch the cut command */
		if (Settings.CutOption != 0) {
			if (Settings.CutOption == 1) /* Partial cut always */
				buf[1] = 1;
			else if (Settings.CutOption == 2) /* Full cut always */
				buf[1] = 2;
			else if (Settings.CutOption == 3) /* Never cut */
				buf[1] = 255;
		}
		/* 对立即切纸指令，判断是否需要打印票头票尾logo */
		if (buf[0] == 0x21) {
			if(cutter_flag == 1 || cutter_flag == 2){ //1-上电后切纸或者翻盖切纸后   			2-在发生1的状态下，下次打印之前需要追加头部logo
				PrnIoctl(PRN_CMD_SEND_EMBEDDED_COMMAND, buf, len, NULL, NULL);
			}else{
				if(print_chn_id_filter == 0){
					if(CuttingAutoLogo > 0){
						PrintFooterLogo();
						if (*((uint32_t *)(buf + 2)) < 106) /* CAP06-347-B1打印头到切刀距离为76点，在票尾logo后空30点切纸 */
							*((uint32_t *)(buf + 2)) = 106;
					}
					if(buf[1] == 2){ // 顶部的logo在下一次订单的时候打出
						cutter_flag = 3; //全切
					}else{
						cutter_flag = 4; //半切
					}
				}
				PrnIoctl(PRN_CMD_SEND_EMBEDDED_COMMAND, buf, len, NULL, NULL);
			}
			goto _done;
		}
	}

	PrnIoctl(PRN_CMD_SEND_EMBEDDED_COMMAND, buf, len, NULL, NULL);

_done:
	switch (buf[0]) {
	case 0x01: // Set buffer ID
	case 0x0D: // Set bits per dot
	case 0x11: // Locate black mark
	case 0x21: // Cut paper
	case 0x22: // Cut paper
		LogDbg("FLUSH_RING_BUFFER: 0x%02X", buf[0]);
		PrnIoctl(PRN_CMD_FLUSH_RING_BUFFER, NULL, 0, NULL, NULL);
		break;
	default:
		break;
	}

	return 0;
}

int printer_send_data(const print_buffer_t *buf, uint16_t offset)
{
	int rc = 0;

	if (!buf)
		return PrnDotLine(NULL, 0, PRN_DATA_MONO);

	pthread_mutex_lock(&Cmd_Mutex);

	RUNTIME_FLAG_CLEAR(RTF_KERN_BUF_FULL);
	Cmd_BufferId = buf->id;
	for (Cmd_BufferOffset = offset; Cmd_BufferOffset < buf->len; Cmd_BufferOffset++) {
		Cmd_Proc(buf->data[Cmd_BufferOffset]);
		if (RUNTIME_FLAG_IS_SET(RTF_KERN_BUF_FULL)) {
			rc = PRN_BUF_FULL;
			break;
		}
	}

	pthread_mutex_unlock(&Cmd_Mutex);

	return rc;
}

int printer_send_debug_string(const char *fmt, ...)
{
	va_list ap;
	uint8_t backup[8], buf[128];
	int i, len;

	pthread_mutex_lock(&Cmd_Mutex);

	i = 0;
	backup[i++] = (uint8_t)Settings.LineSpacing[0];
	backup[i++] = Settings.CharHSize[0];
	backup[i++] = Settings.CharVSize[0];
	backup[i++] = Settings.Alignment;
	backup[i++] = Settings.ClockwiseRotationMode;
	backup[i++] = Settings.BlackWhiteReverseMode;

	Settings.LineSpacing[0] = 16;
	Settings.CharHSize[0] = 1;
	Settings.CharVSize[0] = 1;
	Settings.Alignment = 0;
	Settings.ClockwiseRotationMode = 0;
	Settings.BlackWhiteReverseMode = 0;

	va_start(ap, fmt);
	len = vsnprintf((char *)buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);

	if (len > 0 && buf[len - 1] != 0x0A)
		buf[len++] = 0x0A;

	for (i = 0; i < len; i++)
		Cmd_Proc(buf[i]);

	i = 0;
	Settings.LineSpacing[0] = backup[i++];
	Settings.CharHSize[0] = backup[i++];
	Settings.CharVSize[0] = backup[i++];
	Settings.Alignment = backup[i++];
	Settings.ClockwiseRotationMode = backup[i++];
	Settings.BlackWhiteReverseMode = backup[i++];

	pthread_mutex_unlock(&Cmd_Mutex);

	return 0;
}

void printer_set_density(uint8_t density, int save)
{
	if (density < 10)
		density = 10;
	else if (density > 200)
		density = 200;

	SendPrinterCommand1(0x02, density);

	if (save) {
		app_setting_set_int(PRINTER_SECTION, "Density", density);
	}
}

void printer_set_maxspeed(uint8_t maxspeed, int save)
{
	SendPrinterCommand1(0x03, maxspeed);

	if (save) {
		app_setting_set_int(PRINTER_SECTION, "MaxSpeed", maxspeed);
	}
}

uint8_t printer_get_cashbox_drawer_state(void)
{
	char buf[4];

	if (read_node(NODE_CASH_SW, buf, sizeof(buf)) <= 0)
		return 0;
	return (buf[0] == '1') ? 1 : 0;
}

int printer_init(void)
{
	simple_timer_create(&cashbox_open_timer, cashbox_timer_handler);

	PrnFd = open("/dev/tph", O_RDWR|O_CLOEXEC);
	if (PrnFd == -1) {
		LogFatal("open() failed. errno=%d (%s)", errno, strerror(errno));
		return -1;
	}

	PrnIoctl(PRN_CMD_GET_TPH_VERSION, NULL, 0, NULL, NULL);

	SizeOfPrnDivBuf = sizeof(PRINT_DIVISION_BUF) + (PRTLINEBUFSIZE << 2);
	LogInfo("DOTS_PER_LINE=%u", DOTS_PER_LINE);
	PrnDivBuf = (PRINT_DIVISION_BUF *)malloc(SizeOfPrnDivBuf);
	if (!PrnDivBuf)
		return -1;
	memset(PrnDivBuf, 0, SizeOfPrnDivBuf);

	WordSetInit();

	PersistentDataInit();

	harfBuzz->init();
	lineBuffer->init();
	pageBuffer->init();

	RuntimeDataInit();

	return 0;
}
