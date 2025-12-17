#ifndef __ESC_INSTRUCT_H__
#define __ESC_INSTRUCT_H__

#include <stdint.h>

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern void ReverseFeed(uint32_t dots);
extern void ESC_Proc(void);

#endif // __ESC_INSTRUCT_H__
