#include <string.h>
#include <zint.h>
#include "printer.h"
#include "Instruct_Proc.h"
#include "LineBuffer.h"
#include "PageBuffer.h"
#include "Runtime_Data.h"
#include "WordSet.h"
#include "Util.h"
#include "ESC_Instruct.h"

/*----------------------------------------------*
 | ESC FF                                       |
 *----------------------------------------------*/
static void PrintDataInPageMode(void)
{
	if (Settings.Mode == PAGE_MODE)
		pageBuffer->print();

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC SP                                       |
 *----------------------------------------------*/
static void SetRightSideCharSpacing(void)
{
	if (Settings.Mode == PAGE_MODE) {
		Settings.RightCharSpacing[1][0] = Cmd_Data;
		Settings.RightCharSpacing[1][1] = Cmd_Data;
	}
	else {
		Settings.RightCharSpacing[0][0] = Cmd_Data;
		Settings.RightCharSpacing[0][1] = Cmd_Data;
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC !                                        |
 *----------------------------------------------*/
static void SelectPrintModes(void)
{
	/* Bit 0 */
	Settings.ASC_WordSet = (Cmd_Data & 0x01) ? 1 : 0;

	/* Bit 3 */
	if (Cmd_Data & 0x08) {
		Settings.EmphasizedMode[0] = 1;
		Settings.EmphasizedMode[1] = 1;
	}
	else {
		Settings.EmphasizedMode[0] = 0;
		Settings.EmphasizedMode[1] = 0;
	}

	/* Bit 4 */
	if (Cmd_Data & 0x10) {
		Settings.CharVSize[0] = 2;
		Settings.CharVSize[1] = 2;
	}
	else {
		Settings.CharVSize[0] = 1;
		Settings.CharVSize[1] = 1;
	}

	/* Bit 5 */
	if (Cmd_Data & 0x20) {
		Settings.CharHSize[0] = 2;
		Settings.CharHSize[1] = 2;
	}
	else {
		Settings.CharHSize[0] = 1;
		Settings.CharHSize[1] = 1;
	}

	/* Bit 7 */
	if (Cmd_Data & 0x80) {
		Settings.UnderlineMode[0] |= 0x81;
		Settings.UnderlineMode[1] |= 0x81;
	}
	else {
		Settings.UnderlineMode[0] &= 0x0F;
		Settings.UnderlineMode[1] &= 0x0F;
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC $                                        |
 *----------------------------------------------*/
static void SetAbsolutePrintPosition_nH(void)
{
	Cmd_List0 |= (Cmd_Data << 8);
	if (Cmd_List0 >= Settings.leftMargin() && Cmd_List0 <= Settings.rightMargin())
		lineBuffer->EndPos = Cmd_List0;

	ResetNextFun();
}

static void SetAbsolutePrintPosition_nL(void)
{
	Cmd_List0 = Cmd_Data;

	pNextFun = SetAbsolutePrintPosition_nH;
}

/*----------------------------------------------*
 | ESC %                                        |
 *----------------------------------------------*/
static void SelectCancelUserDefinedCharSet(void)
{
	Settings.UserDefEnabled[0] = (Cmd_Data & 0x01);

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC &                                        |
 *----------------------------------------------*/
static void DefineASCIIChar_CharData(void)
{
	uint32_t i, index, addr;
	uint8_t mask1, mask2;

	if (Cmd_List0 == 0) {
		if (Cmd_Data != 12) {
			ResetNextFun();
			return;
		}
		Cmd_List4 = Cmd_Data; /* x */
		Cmd_List5 = (Cmd_List2 - 0x20) * USER_DEFINED_ASCII_CHAR_SIZE + USER_DEFINED_ASCII_BASE_ADDR; /* Font data address */
		memset(fontData + Cmd_List5, 0, USER_DEFINED_ASCII_CHAR_SIZE);
	}
	else {
		index = Cmd_List0 - 1;
		if (index < 24) {
			addr = Cmd_List5 + 2 + (index % 3) * 8;
			mask1 = 1 << (7 - (index / 3));
			for (i = 0; i < 8; i++, addr++) {
				if (Cmd_Data & (1 << (7 - i)))
					fontData[addr] |= mask1;
			}
		}
		else {
			index -= 24;
			addr = Cmd_List5 + 26 + (index % 3) * 4;
			mask1 = 1 << (7 - (index / 3));
			mask2 = 1 << (3 - (index / 3));
			for (i = 0; i < 8; i += 2, addr++) {
				if (Cmd_Data & (1 << (7 - i)))
					fontData[addr] |= mask1;
				if (Cmd_Data & (1 << (6 - i)))
					fontData[addr] |= mask2;
			}
		}
	}

	if (++Cmd_List0 == Cmd_List1 * Cmd_List4 + 1) {
		fontData[Cmd_List5] = 1;
		if (++Cmd_List2 > Cmd_List3)
			ResetNextFun();
		else
			Cmd_List0 = 0;
	}
}

static void DefineASCIIChar_y(void)
{
	switch (Cmd_List0++) {
	case 0: /* y */
		if (Cmd_Data != 3)
			break;
		Cmd_List1 = Cmd_Data;
		return;
	case 1: /* c1 */
		if (Cmd_Data < 0x20 || Cmd_Data > 0x7E)
			break;
		Cmd_List2 = Cmd_Data;
		return;
	case 2: /* c2 */
		if (Cmd_Data < Cmd_List2 || Cmd_Data > 0x7E)
			break;
		Cmd_List3 = Cmd_Data;
		Cmd_List0 = 0;
		pNextFun = DefineASCIIChar_CharData;
		return;
	default:
		break;
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC *                                        |
 *----------------------------------------------*/
static void SelectBitImageMode_ReceiveData(void)
{
	uint32_t i, j, k, data, line, pos, posinc;
	uint8_t mask;

	if (Cmd_List2 < DATA_BUFFER_SIZE)
		DataBuffer[Cmd_List2] = Cmd_Data;

	if (++Cmd_List2 < Cmd_List1)
		return;

	if (Cmd_List0 & 1) { /* 双密度 */
		data = 0x80000000;
		posinc = 1;
	}
	else { /* 单密度 */
		data = 0xC0000000;
		posinc = 2;
	}

	if (Cmd_List0 < 32) { /* 8点模式 */
		k = Settings.printAreaWidth();
		if (k > Cmd_List1)
			k = Cmd_List1;
		line = MAX_LINE_HEIGHT - 8;
		for (mask = 0x80; mask != 0; mask >>= 1) {
			pos = lineBuffer->EndPos;
			for (i = 0; i < k; i++) {
				if (DataBuffer[i] & mask)
					Dotline_SetDots(lineBuffer->dotlineAt(line), pos, data, data, false);
				pos += posinc;
			}
			line++;
		}
		lineBuffer->EndPos = pos;
		lineBuffer->setHeight(8);
	}
	else { /* 24点模式 */
		k = Settings.printAreaWidth() * 3;
		if (k > Cmd_List1)
			k = Cmd_List1;
		line = MAX_LINE_HEIGHT - 24;
		for (j = 0; j < 3; j++) {
			for (mask = 0x80; mask != 0; mask >>= 1) {
				pos = lineBuffer->EndPos;
				for (i = j; i < k; i += 3) {
					if (DataBuffer[i] & mask)
						Dotline_SetDots(lineBuffer->dotlineAt(line), pos, data, data, false);
					pos += posinc;
				}
				line++;
			}
		}
		lineBuffer->EndPos = pos;
		lineBuffer->setHeight(24);
	}

	ResetNextFun();
}

static void SelectBitImageMode_nH(void)
{
	Cmd_List1 |= Cmd_Data << 8;

	if (Cmd_List0 > 1)
		Cmd_List1 *= 3;

	if (Cmd_List1 == 0)
		ResetNextFun();
	else {
		Cmd_List2 = 0;
		pNextFun = SelectBitImageMode_ReceiveData;
	}
}

static void SelectBitImageMode_nL(void)
{
	Cmd_List1 = Cmd_Data;

	pNextFun = SelectBitImageMode_nH;
}

static void SelectBitImageMode_m(void)
{
	if (Cmd_Data == 0 || Cmd_Data == 1 ||
		Cmd_Data == 32 || Cmd_Data == 33) {
		Cmd_List0 = Cmd_Data;
		pNextFun = SelectBitImageMode_nL;
		return;
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC -                                        |
 *----------------------------------------------*/
static void SetUnderlineMode(void)
{
	uint8_t mode;

	if (Cmd_Data >= 48 && Cmd_Data <= 50)
		Cmd_Data -= 48;

	switch (Cmd_Data) {
	case 0:
		mode = 0x00;
		break;
	case 1:
		mode = 0x81;
		break;
	case 2:
		mode = 0x83;
		break;
	default:
		goto _exit;
	}

	Settings.UnderlineMode[0] = mode;
	Settings.UnderlineMode[1] = mode;

_exit:
	ResetNextFun();
}

/*----------------------------------------------*
 | ESC 2                                        |
 *----------------------------------------------*/
static void SelectDefaultLineSpacing(void)
{
	Settings.LineSpacing[0] = 30;
	Settings.LineSpacing[1] = 30;

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC 3                                        |
 *----------------------------------------------*/
static void SetLineSpacing(void)
{
	uint32_t dotsToFeed = (203 * Cmd_Data) / Settings.PrintVerticalAccuracy;
	if (Settings.Mode == PAGE_MODE)
		Settings.LineSpacing[1] = dotsToFeed;
	else
		Settings.LineSpacing[0] = dotsToFeed;

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC ?                                        |
 *----------------------------------------------*/
static void CancelASCIIChar(void)
{
	if (Cmd_Data >= 0x20 && Cmd_Data <= 0x7E) {
		Cmd_Data = (Cmd_Data - 0x20) * USER_DEFINED_ASCII_CHAR_SIZE + USER_DEFINED_ASCII_BASE_ADDR;
		memset(fontData + Cmd_Data, 0, USER_DEFINED_ASCII_CHAR_SIZE);
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC D                                        |
 *----------------------------------------------*/
static void SetTabPos(void)
{
	uint32_t width, pos;

	if (Cmd_Data == 0)
		goto _exit;

	switch (Settings.ASC_WordSet) {
	case 1:  width = 9;  break;
	default: width = 12; break;
	}
	pos = (width + Settings.rightCharSpacing(0)) * Cmd_Data * Settings.CharHSize[0];

	if (Cmd_List0 > 0 && pos <= Settings.TabPos[Cmd_List0 - 1])
		goto _exit;

	Settings.TabPos[Cmd_List0++] = pos;

	if (Cmd_List0 < 32)
		return;

_exit:
	ResetNextFun();
}

/*----------------------------------------------*
 | ESC E / ESC G                                |
 *----------------------------------------------*/
static void SetEmphasizedMode(void)
{
	Cmd_Data &= 0x01;
	Settings.EmphasizedMode[0] = Cmd_Data;
	Settings.EmphasizedMode[1] = Cmd_Data;

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC J                                        |
 *----------------------------------------------*/
static void PrintAndFeedDotlines(void)
{
	uint32_t dotsToFeed = (203 * Cmd_Data) / Settings.PrintVerticalAccuracy;
	lineBuffer->print(dotsToFeed);

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC K                                        |
 *----------------------------------------------*/
static void PrintAndReverseFeed(void)
{
	uint32_t h = lineBuffer->Height;
	uint32_t dotsToFeed = (203 * Cmd_Data) / Settings.PrintVerticalAccuracy;

	if (h > 0)
		lineBuffer->print(0);

	ReverseFeed(dotsToFeed + h);

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC L                                        |
 *----------------------------------------------*/
static void SelectPageMode(void)
{
	if (Settings.Mode == STANDARD_MODE && lineBuffer->isEmpty()) {
		Settings.Mode = PAGE_MODE;
		lineBuffer->updateLineStride();
		pageBuffer->create();
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC M                                        |
 *----------------------------------------------*/
static void SelectCharFont(void)
{
	if (Cmd_Data >= 48 && Cmd_Data <= 49)
		Cmd_Data -= 48;

	if (Cmd_Data <= 1)
		Settings.ASC_WordSet = Cmd_Data;

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC R                                        |
 *----------------------------------------------*/
static void SelectInternationalCharSet(void)
{
	if (Cmd_Data <= 16)
		Settings.InternationalCharSet = Cmd_Data;

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC S                                        |
 *----------------------------------------------*/
static void SelectStandardMode(void)
{
	if (Settings.Mode == PAGE_MODE) {
		pageBuffer->destroy();
		pageBuffer->resetPrintArea();
		Settings.Mode = STANDARD_MODE;
		lineBuffer->clear();
		lineBuffer->updateLineStride();
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC T                                        |
 *----------------------------------------------*/
static void SelectPrintDirection(void)
{
	if (Cmd_Data >= 48 && Cmd_Data <= 51)
		Cmd_Data -= 48;

	if (Cmd_Data <= 3)
		pageBuffer->setDirection(Cmd_Data);

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC V                                        |
 *----------------------------------------------*/
static void SetClockwiseRotationMode(void)
{
	if (Cmd_Data >= 48 && Cmd_Data <= 49)
		Cmd_Data -= 48;

	if (Cmd_Data <= 1)
		Settings.ClockwiseRotationMode = Cmd_Data;

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC W                                        |
 *----------------------------------------------*/
static void SetPageModePrintArea(void)
{
	switch (Cmd_List0++) {
	/* x */
	case 0:
		Cmd_List1  = Cmd_Data;
		return;
	case 1:
		Cmd_List1 |= Cmd_Data << 8;
		return;
	/* y */
	case 2:
		Cmd_List2  = Cmd_Data;
		return;
	case 3:
		Cmd_List2 |= Cmd_Data << 8;
		return;
	/* dx */
	case 4:
		Cmd_List3  = Cmd_Data;
		return;
	case 5:
		Cmd_List3 |= Cmd_Data << 8;
		return;
	/* dy */
	case 6:
		Cmd_List4  = Cmd_Data;
		return;
	case 7:
		Cmd_List4 |= Cmd_Data << 8;
		pageBuffer->setPrintArea(Cmd_List1, Cmd_List2, Cmd_List3, Cmd_List4);
		break;
	default:
		break;
	}

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC Z                                        |
 *----------------------------------------------*/
static void SPRT_PrintQRCode_Data(void)
{
	if (Cmd_List4 < DATA_BUFFER_SIZE)
		DataBuffer[Cmd_List4++] = Cmd_Data;

	if (Cmd_List4 < Cmd_List3)
		return;

	struct zint_symbol *symbol;
	int i, j, k, p, x_size, y_size;
	uint8_t mask;

	QRCode.SymbolSize = 0;

	symbol = ZBarcode_Create();
	if (symbol == NULL)
		goto _exit;

	symbol->symbology = BARCODE_QRCODE;

	symbol->option_1 = (Cmd_List1 >= 1 && Cmd_List1 <= 4) ? Cmd_List1 : 3; /* Error correction level, 1-4 */
	symbol->option_2 = Cmd_List0; /* Model */

	if (ZBarcode_Encode_and_Buffer(symbol, DataBuffer, Cmd_List3, 0) > ZINT_WARN_INVALID_OPTION) {
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

	if (Cmd_List2 > 2)
		QRCode.ModuleSize = Cmd_List2;
	lineBuffer->printQRCode();
	lineBuffer->printLine();

_exit:
	ResetNextFun();
}

static void SPRT_PrintQRCode_nH(void)
{
	Cmd_List3 |= Cmd_Data << 8;

	if (Cmd_List3 == 0)
		ResetNextFun();
	else {
		Cmd_List4 = 0;
		pNextFun = SPRT_PrintQRCode_Data;
	}
}

static void SPRT_PrintQRCode_nL(void)
{
	Cmd_List3 = Cmd_Data;

	pNextFun = SPRT_PrintQRCode_nH;
}

static void SPRT_PrintQRCode_n3(void)
{
	Cmd_List2 = Cmd_Data;

	pNextFun = SPRT_PrintQRCode_nL;
}

static void SPRT_PrintQRCode_n2(void)
{
	Cmd_List1 = Cmd_Data;

	pNextFun = SPRT_PrintQRCode_n3;
}

static void SPRT_PrintQRCode_n1(void)
{
	Cmd_List0 = Cmd_Data;

	pNextFun = SPRT_PrintQRCode_n2;
}

/*----------------------------------------------*
 | ESC \                                        |
 *----------------------------------------------*/
static void SetRelativePrintPosition_nH(void)
{
	int pos;

	Cmd_List0 |= (Cmd_Data << 8);
	pos = (int)lineBuffer->EndPos + (short)Cmd_List0;
	if (pos >= (int)Settings.leftMargin() && pos <= (int)Settings.rightMargin())
		lineBuffer->EndPos = (uint32_t)pos;

	ResetNextFun();
}

static void SetRelativePrintPosition_nL(void)
{
	Cmd_List0 = Cmd_Data;

	pNextFun = SetRelativePrintPosition_nH;
}

/*----------------------------------------------*
 | ESC a                                        |
 *----------------------------------------------*/
static void SetAlignment(void)
{
	if (Cmd_Data >= 48 && Cmd_Data <= 50)
		Cmd_Data -= 48;

	if (Cmd_Data <= 2)
		Settings.Alignment = Cmd_Data;

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC d                                        |
 *----------------------------------------------*/
static void PrintAndFeedLines(void)
{
	lineBuffer->print(Settings.lineSpacing() * Cmd_Data);

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC e                                        |
 *----------------------------------------------*/
static void PrintAndReverseFeedLines(void)
{
	uint32_t h = lineBuffer->Height;

	if (h > 0)
		lineBuffer->print(0);

	ReverseFeed(Settings.lineSpacing() * Cmd_Data + h);

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC i / ESC m                                |
 *----------------------------------------------*/
static void PartialCut(void)
{
	uint8_t buf[16];

	memset(buf, 0, sizeof(buf));

	buf[0] = 0x21;
	buf[1] = 1;
	MakeCutCmdId((uint64_t *)(buf + 6));
	printer_send_command(buf, 14);

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC p                                        |
 *----------------------------------------------*/
static void GeneratePulse_t2(void)
{
	uint8_t dataIn[8];

	dataIn[0] = Cmd_List0;
	*((uint16_t *)(dataIn + 1)) = Cmd_List1 << 1;
	*((uint16_t *)(dataIn + 3)) = Cmd_Data  << 1;
	PrnIoctl(PRN_CMD_OPEN_CASHBOX, dataIn, 5, NULL, NULL);

	ResetNextFun();
}

static void GeneratePulse_t1(void)
{
	Cmd_List1 = Cmd_Data;

	pNextFun = GeneratePulse_t2;
}

static void GeneratePulse(void)
{
	Cmd_List0 = Cmd_Data;

	pNextFun = GeneratePulse_t1;
}

/*----------------------------------------------*
 | ESC r                                        |
 *----------------------------------------------*/
static void SelectColor(void)
{
	if (Cmd_Data >= 48 && Cmd_Data <= 49)
		Cmd_Data -= 48;

	if (Cmd_Data <= 1)
		Settings.Color = Cmd_Data;

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC t                                        |
 *----------------------------------------------*/
static void SelectCodePage(void)
{
	if (Cmd_Data < 64 && CODE_PAGE_MAPPINGS[Cmd_Data] != 255)
		Settings.CodePage = Cmd_Data;

	ResetNextFun();
}

/*----------------------------------------------*
 | ESC {                                        |
 *----------------------------------------------*/
static void SetUpsideDownMode(void)
{
	Settings.UpsideDownMode = (Cmd_Data & 0x01);

	ResetNextFun();
}

void ReverseFeed(uint32_t dots)
{
	uint8_t buf[8];

	memset(buf, 0, sizeof(buf));
	buf[0] = 0x31;
	buf[1] = 2;
	*((uint32_t *)(buf + 2)) = dots;
	printer_send_command(buf, 6);
}

void ESC_Proc(void)
{
	switch (Cmd_Data) {
	case 0x0C: /* [ESC FF] Print data in page mode */
		PrintDataInPageMode();
		break;
	case 0x20: /* [ESC SP] Set right-side character spacing */
		pNextFun = SetRightSideCharSpacing;
		break;
	case 0x21: /* [ESC !] Select print mode(s) */
		pNextFun = SelectPrintModes;
		break;
	case 0x24: /* [ESC $] Set absolute print position */
		pNextFun = SetAbsolutePrintPosition_nL;
		break;
	case 0x25: /* [ESC %] Select/cancel user-defined character set */
		pNextFun = SelectCancelUserDefinedCharSet;
		break;
	case 0x26: /* [ESC &] Define user-defined characters */
		Cmd_List0 = 0;
		pNextFun = DefineASCIIChar_y;
		break;
	case 0x2A: /* [ESC *] Select bit-image mode */
		pNextFun = SelectBitImageMode_m;
		break;
	case 0x2D: /* [ESC -] Turn underline mode on/off */
		pNextFun = SetUnderlineMode;
		break;
	case 0x32: /* [ESC 2] Select default line spacing */
		SelectDefaultLineSpacing();
		break;
	case 0x33: /* [ESC 3] Set line spacing */
		pNextFun = SetLineSpacing;
		break;
	case 0x3F: /* [ESC ?] Cancel user-defined characters */
		pNextFun = CancelASCIIChar;
		break;
	case 0x40: /* [ESC @] Initialize printer */
		RuntimeDataInit();
		break;
	case 0x44: /* [ESC D] Set horizontal tab positions */
		memset(Settings.TabPos, 0, sizeof(Settings.TabPos));
		Cmd_List0 = 0;
		pNextFun = SetTabPos;
		break;
	case 0x45: /* [ESC E] Turn emphasized mode on/off */
	case 0x47: /* [ESC G] Turn double-strike mode on/off */
		pNextFun = SetEmphasizedMode;
		break;
	case 0x4A: /* [ESC J] Print and feed paper */
		pNextFun = PrintAndFeedDotlines;
		break;
	case 0x4B: /* [ESC K] Print and reverse feed */
		pNextFun = PrintAndReverseFeed;
		break;
	case 0x4C: /* [ESC L] Select page mode */
		SelectPageMode();
		break;
	case 0x4D: /* [ESC M] Select character font */
		pNextFun = SelectCharFont;
		break;
	case 0x52: /* [ESC R] Select an international character set */
		pNextFun = SelectInternationalCharSet;
		break;
	case 0x53: /* [ESC S] Select standard mode */
		SelectStandardMode();
		break;
	case 0x54: /* [ESC T] Select print direction in page mode */
		pNextFun = SelectPrintDirection;
		break;
	case 0x56: /* [ESC V] Turn 90-degree clockwise rotation mode on/off */
		pNextFun = SetClockwiseRotationMode;
		break;
	case 0x57: /* [ESC W] Set print area in page mode */
		Cmd_List0 = 0;
		pNextFun = SetPageModePrintArea;
		break;
	case 0x5A: /* [ESC Z] SPRT: Print QR code */
		pNextFun = SPRT_PrintQRCode_n1;
		break;
	case 0x5C: /* [ESC \] Set relative print position */
		pNextFun = SetRelativePrintPosition_nL;
		break;
	case 0x61: /* [ESC a] Select justification */
		pNextFun = SetAlignment;
		break;
	case 0x64: /* [ESC d] Print and feed n lines */
		pNextFun = PrintAndFeedLines;
		break;
	case 0x65: /* [ESC e] Print and reverse feed n lines */
		pNextFun = PrintAndReverseFeedLines;
		break;
	case 0x69: /* [ESC i] Partial cut */
	case 0x6D: /* [ESC m] Partial cut */
		PartialCut();
		break;
	case 0x70: /* [ESC p] Generate pulse */
		pNextFun = GeneratePulse;
		break;
	case 0x72: /* [ESC r] Select print color */
		pNextFun = SelectColor;
		break;
	case 0x74: /* [ESC t] Select character code table */
		pNextFun = SelectCodePage;
		break;
	case 0x7B: /* [ESC {] Turn upside-down print mode on/off */
		pNextFun = SetUpsideDownMode;
		break;
	default:
		ResetNextFun();
		break;
	}
}
