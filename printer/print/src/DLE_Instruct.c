#include "printer.h"
#include "Instruct_Proc.h"
#include "DLE_Instruct.h"

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

	ReplyToHost(&status, sizeof(status), -1);

_exit:
	ResetNextFun();
}

void DLE_Proc(void)
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
