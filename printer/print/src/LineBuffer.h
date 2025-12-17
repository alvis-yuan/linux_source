#ifndef __LINEBUFFER_H__
#define __LINEBUFFER_H__

#include <stdint.h>
#include "Config.h"
#include "CharBuffer.h"

#define LINE_BUFFER_FLAG_LINE_RTL (1UL << 0)
#define LINE_BUFFER_FLAG_CHAR_RTL (1UL << 1)

typedef struct {
	uint32_t *Data;
	uint32_t *Underlines;
	uint32_t EndPos;
	uint32_t Height;
	uint32_t Top;
	uint32_t Bottom;
	uint32_t UnderlineCount;
	uint32_t Flags;
	uint32_t CursorPos;
	uint32_t BWReverseRange[2];

	int       (*init)(void);
	int       (*isEmpty)(void);
	void      (*clear)(void);
	int       (*append)(CharBuffer *);
	int       (*appendHarfBuzzCanvas)(uint32_t, uint32_t);
	void      (*setHeight)(uint32_t);
	void      (*updateHeight)(uint32_t, uint32_t);
	void      (*setFlag)(uint32_t);
	void      (*clearFlag)(uint32_t);
	void      (*print)(uint32_t);
	void      (*printLine)(void);
	void      (*printRaster)(uint32_t);
	void      (*printBarcode)(void);
	void      (*printPDF417)(void);
	void      (*printQRCode)(void);
	uint32_t *(*firstDotline)(void);
	uint32_t *(*dotlineAt)(uint32_t);
	uint32_t  (*firstLine)(void);
	uint32_t  (*lastLine)(void);
	void      (*updateLineStride)(void);
	uint32_t  (*bytesPerLine)(void);
	uint32_t  (*wordsPerLine)(void);
	uint32_t  (*dotsPerLine)(void);
} LineBuffer;

/*----------------------------------------------*
 | GLOBAL VARIABLES                             |
 *----------------------------------------------*/
extern LineBuffer *lineBuffer;

#endif /* __LINEBUFFER_H__ */
