#include "libcommon.h"
#include "printer.h"
#include "Realtime.h"
#include "Runtime_Data.h"

extern int ReplyToHost(const void *data, uint32_t len, int chn_id);

static void Cmd_Start(void);
static void (*pNextFun)(void);
static uint32_t Cmd_Data, Cmd_Len;
static uint32_t Cmd_List0, Cmd_List1, Cmd_List2;
static int Cmd_ChnId;

static void ResetNextFun(void)
{
	Cmd_Data = 0;
	Cmd_List0 = 0;
	Cmd_List1 = 0;
	Cmd_List2 = 0;
	pNextFun = Cmd_Start;
}

static void SkipBytes(void)
{
	if (--Cmd_List0 == 0)
		ResetNextFun();
}

static void SkipNBytes(uint32_t n)
{
	if (n > 0) {
		Cmd_List0 = n;
		pNextFun = SkipBytes;
	}
	else
		ResetNextFun();
}

/*----------------------------------------------*
 | DLE EOT                                      |
 *----------------------------------------------*/
static void TransmitRealtimeStatus(void)
{
	const print_status_t *s = print_status();
	uint8_t status = 0x12;

	switch (Cmd_Data) {
	case 1: /* Transmit printer status */
		if (printer_get_cashbox_drawer_state())
			status |= 1 << 2;
		break;
	case 2: /* Transmit offline status */
		if (s->platen_opened)
			status |= 1 << 2;
		if (s->no_paper)
			status |= 1 << 6;
		break;
	case 3: /* Transmit error status */
		if (s->no_paper || s->platen_opened)
			status |= 1 << 2;
		if (s->paper_jam)
			status |= 1 << 5;
		if (s->thermal_head_overheated || s->step_motor_overheated)
			status |= 1 << 6;
		break;
	case 4: /* Transmit roll paper sensor status */
		if (s->paper_running_out)
			status |= 3 << 2;
		if (s->no_paper)
			status |= 3 << 5;
		break;
	default:
		goto _exit;
	}

	ReplyToHost(&status, sizeof(status), Cmd_ChnId);

_exit:
	ResetNextFun();
}

static void DLE_Proc(void)
{
	switch (Cmd_Data) {
	case 0x04: /* [DLE EOT] Transmit real-time status */
		pNextFun = TransmitRealtimeStatus;
		break;
	default:
		ResetNextFun();
		break;
	}
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

static void ESC_Proc(void)
{
	switch (Cmd_Data) {
	case 0x70: /* [ESC p] Generate pulse */
		pNextFun = GeneratePulse;
		break;
	default:
		ResetNextFun();
		break;
	}
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
	ReplyToHost(&status, sizeof(status), Cmd_ChnId);

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
	ReplyToHost(&status, sizeof(status), Cmd_ChnId);

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
			ReplyToHost(&status, sizeof(status), Cmd_ChnId);

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
			ReplyToHost(&task_id, sizeof(task_id), Cmd_ChnId);

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
	ReplyToHost(buf, len, Cmd_ChnId);
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
		ReplyToHost(buf, 1, Cmd_ChnId);
		break;

#define P(...) \
do { \
	buf[0] = 0x5F; \
	len = snprintf((char *)(buf + 1), sizeof(buf) - 2, __VA_ARGS__); \
	buf[len + 1] = 0; \
	ReplyToHost(buf, len + 2, Cmd_ChnId); \
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

static void GS_LP(void)
{
	switch (Cmd_Data) {
	case 0x54: /* [GS ( T] Print task management commands */
		pNextFun = GS_LP_T_pL;
		break;
	default:
		ResetNextFun();
		break;
	}
}

static void GS_Proc(void)
{
	switch (Cmd_Data) {
	case 0x28: /* [GS (] Multifunction */
		pNextFun = GS_LP;
		break;
	case 0x49: /* [GS I] Transmit printer ID */
		pNextFun = TransmitPrinterID;
		break;
	default:
		ResetNextFun();
		break;
	}
}

static void Cmd_Start(void)
{
	switch (Cmd_Data) {
	case 0x10: /* DLE */
		pNextFun = DLE_Proc;
		break;
	case 0x1B: /* ESC */
		pNextFun = ESC_Proc;
		break;
	case 0x1D: /* GS */
		pNextFun = GS_Proc;
		break;
	default:
		ResetNextFun();
		break;
	}
}

void Realtime_Proc(int chn_id, const uint8_t *data, uint32_t len)
{
	uint32_t i;

	pNextFun = Cmd_Start;
	Cmd_ChnId = chn_id;
	for (i = 0; i < len; i++) {
		Cmd_Data = data[i];
		(*pNextFun)();
	}
}
