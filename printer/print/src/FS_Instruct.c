#include "libcommon.h"
#include "printer.h"
#include "Config.h"
#include "Util.h"
#include "Instruct_Proc.h"
#include "LineBuffer.h"
#include "Runtime_Data.h"
#include "FS_Instruct.h"

static void SelectPrintModes(void)
{
	/* Bit 2 */
	if (Cmd_Data & 0x04) {
		Settings.CharHSize[1] = 2;
	}
	else {
		Settings.CharHSize[1] = 1;
	}

	/* Bit 3 */
	if (Cmd_Data & 0x08) {
		Settings.CharVSize[1] = 2;
	}
	else {
		Settings.CharVSize[1] = 1;
	}

	/* Bit 7 */
	if (Cmd_Data & 0x80) {
		Settings.UnderlineMode[1] |= 0x81;
	}
	else {
		Settings.UnderlineMode[1] &= 0x0F;
	}

	ResetNextFun();
}

static void SetCJKMode(void)
{
	Settings.CJKMode = 1;

	ResetNextFun();
}

/*----------------------------------------------*
 | FS ( L pL pH fn <function 66>                |
 *----------------------------------------------*/
static void FS_LP_L_42(void)
{
	print_paper_location_t location;
	uint32_t dataOutLength;
	int dots;
	uint8_t buf[16];

	if (Settings.PaperLayout.sa == 49){
		if(strcmp(CPU_TYPE, SUNMI_CPU_TYPE_X1600) == 0){
			dots = PaperLayoutUnitToDots(Settings.PaperLayout.se) + CUTTING_POSITION_TO_BM_1600E;
		}else{
			dots = PaperLayoutUnitToDots(Settings.PaperLayout.se) + CUTTING_POSITION_TO_BM;
		}	
	}else if (Settings.PaperLayout.sa == 113){
		if(strcmp(CPU_TYPE, SUNMI_CPU_TYPE_X1600) == 0){
			dots = CUTTING_POSITION_TO_BM_1600E - PaperLayoutUnitToDots(Settings.PaperLayout.se);
			if(dots <= 0){
				dots = 0;
			}
		}else{
			dots = CUTTING_POSITION_TO_BM - PaperLayoutUnitToDots(Settings.PaperLayout.se);
			if(dots <= 0){
				dots = 0;
			}
		}
	}
	else
		return;

	switch (DataBuffer[1]) {
	case 48:
		dataOutLength = sizeof(location);
		if (PrnIoctl(PRN_CMD_GET_PAPER_LOCATION, NULL, 0, &location, &dataOutLength) != 0)
			break;
		LogInfo("Black Mark location: %u", location.black_mark_location);
		LogInfo("   Black Mark count: %u", location.located_black_marks);
		LogInfo("   Current location: %u", location.current_location);
		if ((location.black_mark_location) != 0 && /* 已定位黑标 */
			(location.current_location - location.black_mark_location) == dots)
			break;
	case 49:
		buf[0] = 0x11;
		buf[1] = Settings.BlackMarkLocation;
		buf[2] = 1;
		buf[3] = 0;
		*((uint16_t *)(buf + 4 )) = 48;
		*((uint32_t *)(buf + 6 )) = dots;
		*((uint32_t *)(buf + 10)) = (Settings.PaperLayout.sb + (Settings.PaperLayout.sc << 1)) >> 3;
		printer_send_command(buf, 14);
		break;
	default:
		break;
	}
}

/*----------------------------------------------*
 | FS ( L pL pH fn <function 67>                |
 *----------------------------------------------*/
static void FS_LP_L_43(void)
{
	print_paper_location_t location;
	uint32_t dataOutLength, diff;
	int dots;
	uint8_t mode, buf[16];

	
	if (Settings.PaperLayout.sa == 49){
		if(strcmp(CPU_TYPE, SUNMI_CPU_TYPE_X1600) == 0){
			dots = PaperLayoutUnitToDots(Settings.PaperLayout.sd) + PRINT_POSITION_TO_BM_1600E;
		}else{
			dots = PaperLayoutUnitToDots(Settings.PaperLayout.sd) + PRINT_POSITION_TO_BM;
		}		
	}
	else if (Settings.PaperLayout.sa == 113) {
		if(strcmp(CPU_TYPE, SUNMI_CPU_TYPE_X1600) == 0){
			dots  = CUTTING_POSITION_TO_BM_1600E - PaperLayoutUnitToDots(Settings.PaperLayout.se);
			dots += PaperLayoutUnitToDots(Settings.PaperLayout.sd);
			dots -= 76;
			LogInfo("dots = %d\n", dots);
		}else{
			dots  = CUTTING_POSITION_TO_BM - PaperLayoutUnitToDots(Settings.PaperLayout.se);
			dots += PaperLayoutUnitToDots(Settings.PaperLayout.sd);
			dots -= CUTTING_POSITION_TO_BM - PRINT_POSITION_TO_BM;
		}
	}
	else
		return;

	mode = 1;

	switch (DataBuffer[1]) {
	case 48:
		dataOutLength = sizeof(location);
		if (PrnIoctl(PRN_CMD_GET_PAPER_LOCATION, NULL, 0, &location, &dataOutLength) != 0)
			break;
		LogInfo("Black Mark location: %u", location.black_mark_location);
		LogInfo("   Black Mark count: %u", location.located_black_marks);
		LogInfo("   Current location: %u", location.current_location);
		if ((location.black_mark_location) != 0) { /* 已定位黑标 */
			diff = location.current_location - location.black_mark_location;
			if (diff == dots)
				break;
			if (diff < dots) {
				dots -= diff;
				mode = 8;
			}
		}
	case 49:
		buf[0] = 0x11;
		buf[1] = Settings.BlackMarkLocation;
		buf[2] = mode;
		buf[3] = 0;
		*((uint16_t *)(buf + 4 )) = 48;
		*((uint32_t *)(buf + 6 )) = dots;
		*((uint32_t *)(buf + 10)) = (Settings.PaperLayout.sb + (Settings.PaperLayout.sc << 1)) >> 3;
		printer_send_command(buf, 14);
		break;
	default:
		break;
	}
}

static void FS_LP_L_ReceiveData(void)
{
	DataBuffer[Cmd_List0++] = Cmd_Data;

	if (Cmd_List0 < Cmd_Len)
		return;

	switch (DataBuffer[0]) {
	case 66: /* Feed paper to the cutting position */
		if (Cmd_Len != 2)
			goto _exit;
		FS_LP_L_42();
		break;

	case 67: /* Feed paper to the print starting position */
		if (Cmd_Len != 2)
			goto _exit;
		FS_LP_L_43();
		break;

	default:
		break;
	}

_exit:
	ResetNextFun();
}

static void FS_LP_L_pH(void)
{
	Cmd_Len |= Cmd_Data << 8;

	if (Cmd_Len == 0)
		ResetNextFun();
	else if (Cmd_Len > DATA_BUFFER_SIZE)
		SkipNBytes(Cmd_Len);
	else {
		Cmd_List0 = 0;
		pNextFun = FS_LP_L_ReceiveData;
	}
}

static void FS_LP_L_pL(void)
{
	Cmd_Len = Cmd_Data;

	pNextFun = FS_LP_L_pH;
}

static void FS_LP(void)
{
	switch (Cmd_Data) {
	case 0x4C: /* [FS ( L] Select label and black mark control function(s) */
		pNextFun = FS_LP_L_pL;
		break;
	default:
		ResetNextFun();
		break;
	}
}

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

	Settings.UnderlineMode[1] = mode;

_exit:
	ResetNextFun();
}

static void CancelCJKMode(void)
{
	Settings.CJKMode = 0;

	ResetNextFun();
}

static void SetCJKWordSet(void)
{
	switch (Cmd_Data) {
	case 0:
	case 1:
	case 11:
	case 12:
	case 21:
	case 128:
		Settings.CJK_WordSet = Cmd_Data;
		break;
	default:
		break;
	}

	ResetNextFun();
}

static void SetCJKCharSpacing_n2(void)
{
	if (Settings.Mode == PAGE_MODE)
		Settings.RightCharSpacing[1][1] = Cmd_Data;
	else
		Settings.RightCharSpacing[0][1] = Cmd_Data;

	ResetNextFun();
}

static void SetCJKCharSpacing_n1(void)
{
	Settings.LeftCharSpacing[1] = Cmd_Data;

	pNextFun = SetCJKCharSpacing_n2;
}

static void SetQuadSizeMode(void)
{
	if (Cmd_Data & 0x01) {
		Settings.CharHSize[1] = 2;
		Settings.CharVSize[1] = 2;
	}
	else {
		Settings.CharHSize[1] = 1;
		Settings.CharVSize[1] = 1;
	}

	ResetNextFun();
}

void FS_Proc(void)
{
	switch (Cmd_Data) {
	case 0x21: /* [FS !] Select print mode(s) for Kanji characters */
		pNextFun = SelectPrintModes;
		break;
	case 0x26: /* [FS &] Select Kanji character mode */
		SetCJKMode();
		break;
	case 0x28: /* [FS (] Multifunction */
		pNextFun = FS_LP;
		break;
	case 0x2D: /* [FS -] Turn underline mode on/off for Kanji characters */
		pNextFun = SetUnderlineMode;
		break;
	case 0x2E: /* [FS .] Cancel Kanji character mode */
		CancelCJKMode();
		break;
	case 0x43: /* [FS C] Select Kanji character code system */
		pNextFun = SetCJKWordSet;
		break;
	case 0x53: /* [FS S] Set Kanji character spacing */
		pNextFun = SetCJKCharSpacing_n1;
		break;
	case 0x57: /* [FS W] Turn quadruple-size mode on/off for Kanji characters */
		pNextFun = SetQuadSizeMode;
		break;
	default:
		ResetNextFun();
		break;
	}
}
