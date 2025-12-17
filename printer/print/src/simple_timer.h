#ifndef __SIMPLE_TIMER_H__
#define __SIMPLE_TIMER_H__

#include <stdbool.h>
#include <stdint.h>

/*----------------------------------------------*
 | TYPES                                        |
 *----------------------------------------------*/
typedef void (*simple_timer_handler_t)(uint32_t context);

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern int simple_timer_create(uint8_t *p_timer_id, simple_timer_handler_t handler);
extern int simple_timer_start(uint8_t timer_id, uint32_t ms_to_expire, uint32_t context);
extern void simple_timer_stop(uint8_t timer_id);
extern bool simple_timer_is_running(uint8_t timer_id);
extern int simple_timer_init(void);

#endif /* __SIMPLE_TIMER_H__ */
