#ifndef __REALTIME_H__
#define __REALTIME_H__

#include <stdint.h>

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern void Realtime_Proc(int chn_id, const uint8_t *data, uint32_t len);

#endif /* __REALTIME_H__ */
