#ifndef __LED_CTRL_H__
#define __LED_CTRL_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

enum {
	LED_ID_NET,
	LED_ID_ERROR,
	LED_ID_TAPE,
	LED_ID_MAX,
};

typedef enum {
	LED_OP_OFF,
	LED_OP_ON,
	LED_OP_BLINK,
	LED_OP_MAX,
} LED_OPERATE_E;

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern int led_alarm_tape_get( ) ;
int led_ctrl(int led,LED_OPERATE_E op);

#endif /*__LED_CTRL_H__ */