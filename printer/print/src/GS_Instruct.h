#ifndef __GS_INSTRUCT_H__
#define __GS_INSTRUCT_H__

#include <stdint.h>

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern void PrintNVMonoImage(uint8_t kc1, uint8_t kc2, uint8_t xscale, uint8_t yscale);
extern void PrintNVGrayscaleImage(uint8_t n, uint16_t s, uint8_t g, uint8_t m);
extern void GS_Proc(void);

#endif /* __GS_INSTRUCT_H__ */
