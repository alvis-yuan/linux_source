#include <string.h>
#include "libcommon.h"
#include "CharBuffer.h"
#include "DLE_Instruct.h"
#include "ESC_Instruct.h"
#include "FS_Instruct.h"
#include "GS_Instruct.h"
#include "HarfBuzz.h"
#include "LineBuffer.h"
#include "PageBuffer.h"
#include "Runtime_Data.h"
#include "Util.h"
#include "WordSet.h"
#include "Instruct_Proc.h"
#include "tcpserver.h"
#include "usb.h"

pthread_mutex_t Cmd_Mutex = PTHREAD_MUTEX_INITIALIZER;
void (*pNextFun)(void) = Cmd_Start;
uint16_t Cmd_BufferId, Cmd_BufferOffset;
uint32_t Cmd_Data, Cmd_Len;
uint32_t Cmd_List0, Cmd_List1, Cmd_List2, Cmd_List3, Cmd_List4, Cmd_List5;
uint8_t Cmd_Bytes[16];
int Cmd_ChnId = -1, Cmd_DontClearBuffer = 0;

static int ble_send_data(const void *data, uint32_t len)
{
	static bool bt_api_initialized = false;
	uint32_t conn_id;
	int ret;

	if (!bt_api_initialized) {
		if (Bt_api_init() == 0) {
			bt_api_initialized = true;
		}
		else {
			LogDbg("Bt API initialization failed");
			return -1;
		}
	}

	if (BtGetState(PRO_BLE) != 1) { /* Not connected */
		LogDbg("Not connected");
		return -1;
	}
	conn_id = BtOpenSession(PRO_BLE);
	ret = BtSendData(PRO_BLE, conn_id, (unsigned char *)data, (int)len);
	LogDbg("conn_id=%u, ret=%d", conn_id, ret);
	return ret;
}

static int spp_send_data(int chn_id, const void *data, uint32_t len)
{
	static bool bt_api_initialized = false;
	int ret;
	char buf[128] = {0};
	uint32_t send_len = 0;

	if (!bt_api_initialized) {
		if (Bt_api_init() == 0) {
			bt_api_initialized = true;
		}
		else {
			LogDbg("Bt API initialization failed");
			return -1;
		}
	}
	memcpy(buf, data, len);
	//支持多路spp，buf数据的最后一位存放print channel id
	buf[len] = chn_id;
	send_len = len + 1;

	//利用现有函数，暂时用conn_id参数来传递prt_chan_id
	ret = BtSendData(PRO_SPP, chn_id, (unsigned char *)buf, (int)send_len);
	return ret;
}


static void SkipBytes(void)
{
	if (--Cmd_List0 == 0)
		ResetNextFun();
}

static void HT_Proc(void)
{
	uint32_t i = 0, lm = Settings.leftMargin();

	while (i < 32 && lm + Settings.TabPos[i] <= lineBuffer->EndPos)
		i++;
	if (i == 32)
		return;
	if (lm + Settings.TabPos[i] > Settings.rightMargin())
		lineBuffer->printLine();
	else
		lineBuffer->EndPos = lm + Settings.TabPos[i];
}

static void LF_Proc(void)
{
	lineBuffer->printLine();
	if (Settings.RightToLeftMode == 0)
		lineBuffer->clearFlag(LINE_BUFFER_FLAG_LINE_RTL);
	else
		lineBuffer->setFlag(LINE_BUFFER_FLAG_LINE_RTL);
}

void feed_one_line(void)
{
	harfBuzz->shape(2);
	LF_Proc();
}

static void FF_Proc(void)
{
	if (Settings.Mode == PAGE_MODE) {
		pageBuffer->print();
		pageBuffer->destroy();
		pageBuffer->resetPrintArea();
		Settings.Mode = STANDARD_MODE;
		lineBuffer->clear();
		lineBuffer->updateLineStride();
	}
}

static void CR_Proc(void)
{
	if (MemorySwitches[0] & 0x10)
		LF_Proc();
}

static void CAN_Proc(void)
{
	if (Settings.Mode == PAGE_MODE)
		pageBuffer->clear();
}

static void AppendToLineBuffer(void)
{
	if (Settings.isClockwiseRotation())
		charBuffer->clockwiseRotation();

//	if (Settings.BlackWhiteReverseMode)
//		charBuffer->blackWhiteReverse();

	if (lineBuffer->append(charBuffer) > 0) {
		lineBuffer->printLine();
		lineBuffer->append(charBuffer);
	}
}

static void CJK_SubsequentBytes(void)
{
	Cmd_Bytes[Cmd_List1++] = Cmd_Data;

	if (Cmd_List1 < Cmd_List0)
		return;

	Cmd_List0 = MBCS_to_Unicode(Cmd_Bytes, Cmd_List1 - 1);
	if (Cmd_List0 == 0) /* Undefined */
		goto _exit;
	if (Cmd_List0 < 0x20) { /* Expect more bytes */
		if (Cmd_List0 > Cmd_List1)
			return;
		goto _exit;
	}
#if 0
	if (Cmd_List0 < 0x80)
		ASC_Proc(Cmd_List0);
	else
		Unicode_Proc(Cmd_List0);
#else
	if (harfBuzz->appendUnicode(Cmd_List0)) /* 如果HarfBuzz没有接收这个字符，则仍然按点阵处理 */
		Unicode_Proc(Cmd_List0);
#endif

_exit:
	ResetNextFun();
}

static void Utf8_SubsequentBytes(void)
{
	Cmd_Bytes[Cmd_List1++] = Cmd_Data;

	if (Cmd_List1 < Cmd_List0)
		return;

	Cmd_List0 = Utf8_to_Unicode(Cmd_Bytes, Cmd_List1 - 1);
	if (Cmd_List0 == 0) /* Invalid */
		goto _exit;
	if (Cmd_List0 < 0x20) { /* Expect more bytes */
		if (Cmd_List0 > Cmd_List1)
			return;
		goto _exit;
	}
	if (harfBuzz->appendUnicode(Cmd_List0)) /* 如果HarfBuzz没有接收这个字符，则仍然按点阵处理 */
		Unicode_Proc(Cmd_List0);

_exit:
	ResetNextFun();
}

static int HandleSingleByteChar(uint32_t unicode)
{
	if (lineBuffer->Flags & (LINE_BUFFER_FLAG_CHAR_RTL | LINE_BUFFER_FLAG_LINE_RTL)) {
		/* 如果这一行是右到左的阅读顺序，需要对空格、括号字符进行特殊处理 */
		if ((unicode >= '0' && unicode <= '9') ||
			(unicode >= 'A' && unicode <= 'Z') ||
			(unicode >= 'a' && unicode <= 'z')) {
				return 0; // Not handled
		}
		else {
			harfBuzz->shape(10);
			if (lineBuffer->Flags & LINE_BUFFER_FLAG_LINE_RTL) {
				switch (unicode) {
				case '(': unicode = ')'; break;
				case ')': unicode = '('; break;
				case '<': unicode = '>'; break;
				case '>': unicode = '<'; break;
				case '[': unicode = ']'; break;
				case ']': unicode = '['; break;
				case '{': unicode = '}'; break;
				case '}': unicode = '{'; break;
				default:                 break;
				}
			}
			if (harfBuzz->appendUnicode(unicode) == 0) {
				lineBuffer->setFlag(LINE_BUFFER_FLAG_CHAR_RTL);
				harfBuzz->shape(14);
			}
			else { /* 如果HarfBuzz没有接收这个字符，则仍然按点阵处理 */
				Unicode_Proc(unicode);
			}
			return 1; // Handled
		}
	}
	return 0; // Not handled
}

static void Char_Proc(void)
{
	if (Cmd_Data < 0x20)
		goto _exit;

	if (Settings.Utf8_WordSet > 0) {
		Cmd_Bytes[0] = Cmd_Data;
		Cmd_List0 = Utf8_to_Unicode(Cmd_Bytes, 0);
		if (Cmd_List0 == 0) /* Invalid */
			goto _exit;
		if (Cmd_List0 < 0x20) { /* Expect more bytes */
			Cmd_List1 = 1;
			pNextFun = Utf8_SubsequentBytes;
			return;
		}
		/*
		 * 这是单字节字符 (0x20~0x7F)。
		 */
		if (HandleSingleByteChar(Cmd_List0) == 0) {
			if (harfBuzz->appendUnicode(Cmd_List0)) /* 如果HarfBuzz没有接收这个字符，则仍然按点阵处理 */
				Unicode_Proc(Cmd_List0);
		}
		goto _exit;
	}

	if (Settings.CJKMode && Settings.CJK_WordSet != CJK_WORDSET_NONE) {
		Cmd_Bytes[0] = Cmd_Data;
		Cmd_List0 = MBCS_to_Unicode(Cmd_Bytes, 0);
		if (Cmd_List0 == 0) /* Undefined */
			goto _exit;
		if (Cmd_List0 < 0x20) { /* Expect more bytes */
			Cmd_List1 = 1;
			pNextFun = CJK_SubsequentBytes;
			return;
		}
		/*
		 * 这是单字节字符 (0x20~0x7F)。
		 */
		if (HandleSingleByteChar(Cmd_List0) == 0) {
			if (harfBuzz->appendUnicode(Cmd_List0)) /* 如果HarfBuzz没有接收这个字符，则仍然按点阵处理 */
				Unicode_Proc(Cmd_List0);
		}
		goto _exit;
	}

	if (Cmd_Data < 0x80)
		ASC_Proc(Cmd_Data);
	else
		CodePage_Proc(Cmd_Data);

_exit:
	ResetNextFun();
}

void ASC_Proc(uint8_t code)
{
	charBuffer->readWordSetASC(code);
	AppendToLineBuffer();
}

void CodePage_Proc(uint8_t code)
{
	charBuffer->readWordSetCodePage(code);
	AppendToLineBuffer();
}

void Unicode_Proc(uint32_t unicode)
{
	charBuffer->readWordSetUnicode(unicode);
	AppendToLineBuffer();
}

void Cmd_Start(void)
{
	switch (Cmd_Data) {
	case 0x09: /* HT */
		harfBuzz->shape(1);
		HT_Proc();
		break;
	case 0x0A: /* LF */
		harfBuzz->shape(2);
		LF_Proc();
		break;
	case 0x0C: /* FF */
		harfBuzz->shape(3);
		FF_Proc();
		break;
	case 0x0D: /* CR */
		harfBuzz->shape(4);
		CR_Proc();
		break;
	case 0x10: /* DLE */
		harfBuzz->shape(5);
		pNextFun = DLE_Proc;
		break;
	case 0x18: /* CAN */
		harfBuzz->shape(6);
		CAN_Proc();
		break;
	case 0x1B: /* ESC */
		harfBuzz->shape(7);
		pNextFun = ESC_Proc;
		break;
	case 0x1C: /* FS */
		harfBuzz->shape(8);
		pNextFun = FS_Proc;
		break;
	case 0x1D: /* GS */
		harfBuzz->shape(9);
		pNextFun = GS_Proc;
		break;
	default:
		Char_Proc();
		break;
	}
}

/*
 * This is the entry point of ESC/POS processing.
 */
void Cmd_Proc(uint32_t data)
{
	Cmd_Data = data;
	(*pNextFun)();
}

void SkipNBytes(uint32_t n)
{
	if (n > 0) {
		Cmd_List0 = n;
		pNextFun = SkipBytes;
	}
	else
		ResetNextFun();
}

void ResetNextFun(void)
{
	Cmd_Data = 0;
	Cmd_List0 = 0;
	Cmd_List1 = 0;
	Cmd_List2 = 0;
	Cmd_List3 = 0;
	Cmd_List4 = 0;
	Cmd_List5 = 0;
	pNextFun = Cmd_Start;
}

int ReplyToHost(const void *data, uint32_t len, int chn_id)
{
	if (chn_id == -1)
		chn_id = Cmd_ChnId;
	switch (chn_id) {
	case PRINT_CHN_ID_USB:
		return usb_send_data(data, len);
	case PRINT_CHN_ID_TCP_BEGIN ... PRINT_CHN_ID_TCP_END:
		return tcp_server_send_data(chn_id, data, len);
	case PRINT_CHN_ID_BLE:
		return ble_send_data(data, len);
	case PRINT_CHN_ID_SPP:
	case PRINT_CHN_ID_SPP2:
	case PRINT_CHN_ID_SPP3:
	case PRINT_CHN_ID_SPP4:
		return spp_send_data(chn_id, (unsigned char *)data, len);
	default:
		break;
	}
	return -1;
}
