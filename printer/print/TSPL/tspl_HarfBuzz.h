#ifndef __TSPL_HARFBUZZ_H__
#define __TSPL_HARFBUZZ_H__

#include <stdint.h>
#include <stdbool.h>

#define HARFBUZZ_CANVAS_HEIGHT   320
#define HARFBUZZ_CANVAS_WIDTH    10240
#define HARFBUZZ_MAX_UNICODE_LEN 128

typedef struct {
	uint8_t Canvas[HARFBUZZ_CANVAS_HEIGHT][HARFBUZZ_CANVAS_WIDTH / 8];
	struct {
		int x_min, x_max;
		int y_min, y_max;
	} CanvasBBox;
	uint32_t Unicodes[HARFBUZZ_MAX_UNICODE_LEN];
	int UnicodeLen;
	void *ActiveInst;
	bool Initialized;

	int   (*init)(void);
	int   (*loadConf)(void);
	int   (*appendUnicode)(uint32_t);
	void  (*shape)(int);
} HarfBuzz;

/*----------------------------------------------*
 | GLOBAL VARIABLES                             |
 *----------------------------------------------*/
extern HarfBuzz *tspl_harfBuzz;

#endif /* __HARFBUZZ_H__ */
