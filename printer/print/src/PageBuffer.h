#ifndef __PAGEBUFFER_H__
#define __PAGEBUFFER_H__

#include <stdint.h>
#include <cairo.h>
#include "Config.h"
#include "CharBuffer.h"

typedef struct {
	struct {
		/* 打印区域左上角相对于可打印区域的坐标 */
		uint16_t x;
		uint16_t y;
		/* 打印区域的宽度和高度 */
		uint16_t w;
		uint16_t h;
		uint16_t h_max;
	} PrintArea;
	uint8_t Direction;
	cairo_surface_t *Canvas;
	cairo_surface_t *Surface;
	cairo_t *CanvasCr;
	cairo_t *Cr;
	bool Initialized;

	int  (*init)(void);
	void (*create)(void);
	void (*destroy)(void);
	void (*print)(void);
	void (*clear)(void);

	void (*setPrintArea)(uint16_t, uint16_t, uint16_t, uint16_t);
	void (*setDirection)(uint8_t);
	void (*resetPrintArea)(void);
	void (*resetDirection)(void);

	void (*feed)(int);
	void (*paint)(uint8_t *, uint32_t, uint32_t, uint32_t, bool);

	uint16_t (*widthOfTextDir)(void);
	uint16_t (*heightOfTextDir)(void);
} PageBuffer;

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern void to_cairo_endian(uint8_t *data, uint32_t width, uint32_t height, uint32_t stride);

/*----------------------------------------------*
 | GLOBAL VARIABLES                             |
 *----------------------------------------------*/
extern PageBuffer *pageBuffer;

#endif /* __PAGEBUFFER_H__ */
