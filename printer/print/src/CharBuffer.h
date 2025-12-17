#ifndef __CHARBUFFER_H__
#define __CHARBUFFER_H__

#include <stdint.h>
#include "Config.h"

typedef struct {
	uint32_t Data[32];
	uint64_t Data64[64];
	uint32_t Width;
	uint32_t Height;
	uint32_t Top;
	int AdvanceX;
	int Left;
	int CharType;
	int UseData64;

	void (*dump)(void);
	void (*clear)(void);
	void (*readWordSetASC)(uint8_t);
	void (*readWordSetCodePage)(uint8_t);
	void (*readWordSetUnicode)(uint32_t);
	void (*clockwiseRotation)(void);
	void (*blackWhiteReverse)(void);
} CharBuffer;

/*----------------------------------------------*
 | GLOBAL VARIABLES                             |
 *----------------------------------------------*/
extern CharBuffer *charBuffer;

#endif /* __CHARBUFFER_H__ */
