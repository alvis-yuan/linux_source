#ifndef __WORDSET_H__
#define __WORDSET_H__

#include <stdint.h>
#include "CharBuffer.h"

#define USER_DEFINED_ASCII_CHAR_SIZE 38
#define USER_DEFINED_ASCII_BASE_ADDR 0x6B5A2

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern uint32_t ReadWordSet_ASCII_9x17(uint32_t *buf, uint8_t code);
extern uint32_t ReadWordSet_ASCII_12x24(uint32_t *buf, uint8_t code);

extern uint32_t ReadWordSet_ASC(CharBuffer *cbuf, uint8_t code);
extern uint32_t ReadWordSet_CodePage(CharBuffer *cbuf, uint8_t code);
extern uint32_t ReadWordSet_Unicode(CharBuffer *cbuf, uint32_t unicode);

extern uint32_t MBCS_to_Unicode(uint8_t *bytes, uint32_t subsequent_bytes);
extern uint32_t Utf8_to_Unicode(uint8_t *bytes, uint32_t subsequent_bytes);

extern int WordSetInit(void);

/*----------------------------------------------*
 | GLOBAL VARIABLES                             |
 *----------------------------------------------*/
extern uint8_t *fontData;
extern char fontVersion[16];
extern char fontLanguage[16];

#endif // __WORDSET_H__
