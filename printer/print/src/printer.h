#ifndef __PRINTER_H__
#define __PRINTER_H__

#include "print_api.h"

enum {
	PRN_CMD_CUT = 0,
	PRN_CMD_GET_DOTS,
	PRN_CMD_OPEN_CASHBOX,
	PRN_CMD_GET_CASHBOX,
	PRN_CMD_GET_VOLTAGE_ADC,
	PRN_CMD_GET_PAPER_STATE,
	PRN_CMD_GET_TEMPERATURE,
	PRN_CMD_SET_GRAY_LEVEL,
	PRN_CMD_SET_VP_EN_STATE,
	PRN_CMD_SET_PRINT_SPEED,
	PRN_CMD_CLEAR_BUFFER,
	PRN_CMD_CONSOLE_RX_ENABLE,
	PRN_CMD_CONSOLE_RX_DISABLE,
	PRN_CMD_GET_CASHBOX_OPEN_COUNT,
	PRN_CMD_SEND_EMBEDDED_COMMAND,
	PRN_CMD_GET_BUFFER_ID,
	PRN_CMD_SET_FEED_STATE,
	PRN_CMD_GET_PAPER_LOCATION,
	PRN_CMD_SET_PAPER_NOT_TAKEN_ACTION,
	PRN_CMD_SET_MOTOR_OVERHEATED_STATE,
	PRN_CMD_GET_TPH_VERSION,
	PRN_CMD_FLUSH_RING_BUFFER,
	PRN_CMD_CLEAR_PAPER_NOT_TAKEN,

	PRN_CMD_CLEAR_REPRINT_FLAG = 199,
	PRN_CMD_I2C_TEST = 200,

	PRN_CMD_GET_STATUS = 500,

	PRN_CMD_MAX
};

enum {
	PRN_DATA_MONO = 0,
	PRN_DATA_GRAYSCALE,
	PRN_DATA_MAX
};

#define PRN_BUF_FULL (4)

/* 该结构体必须与TPH.c保持一致 */
typedef struct {
	uint32_t stSize;
	uint32_t cut;
	uint32_t paper;
	uint32_t temp;
	uint32_t cap;
	uint32_t state;
	uint32_t voltage;
	uint32_t other;
	uint32_t printedMM;
	uint32_t cashbox_open_count;
	int motor_temperature;
	uint64_t cut_cmd_id;
} PRN_STATUS;

typedef struct {
	/*
	 * 定位到的黑标位置，以定位到黑标时的累计走纸行数表示。
	 * 为0表示未定位黑标。定位黑标后只要有走纸或开盖，
	 * 这个值就变0，需要重新定位。
	 */
	uint32_t black_mark_location;

	/*
	 * 已定位的黑标个数。一旦发生开盖，这个值变0。
	 */
	uint32_t located_black_marks;

	/*
	 * 当前的累计走纸行数。
	 */
	uint32_t current_location;
} print_paper_location_t;

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern int PrnDotLine(void *data, uint32_t dataLength, int dataType);
extern int PrnBlankLines(uint32_t lines);
extern int PrnDuplicateLines(void *dotline, uint32_t lines);
extern int PrnGetStatus(PRN_STATUS *status);
extern int PrnIoctl(uint32_t cmd, void *dataIn, uint32_t dataInLength, void *dataOut, uint32_t *dataOutLength);

extern int printer_clear_buffer(void);
extern int printer_send_command(uint8_t *buf, uint32_t len);
extern int printer_send_data(const print_buffer_t *buf, uint16_t offset);
extern int printer_send_debug_string(const char *fmt, ...);

extern void printer_set_density(uint8_t density, int save);
extern void printer_set_maxspeed(uint8_t maxspeed, int save);

extern uint8_t printer_get_cashbox_drawer_state(void);

extern int printer_init(void);

#endif /* __PRINTER_H__ */
