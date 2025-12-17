#ifndef __HARFBUZZ_H__
#define __HARFBUZZ_H__

#include <stdint.h>
#include "Config.h"
#include "host_cmd_def.h"

#define HARFBUZZ_CANVAS_HEIGHT      320
#define HARFBUZZ_CANVAS_WIDTH       20480
#define HARFBUZZ_MAX_UNICODE_LEN    32

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
	int   (*loadThirdPartyConf)(void);
	int   (*loadConf)(void);
	int   (*saveConf)(void);
	int   (*appendUnicode)(uint32_t);
	void  (*shape)(int);
	void  (*enableThirdPartyFace)(uint8_t, uint8_t);
	void  (*disableThirdPartyFace)(uint8_t);
	void  (*setAsciiCharEnabledState)(bool);
	void  (*setCjkCharEnabledState)(bool);
	void  (*setAsciiCharSize)(uint32_t);
	void  (*setCjkCharSize)(uint32_t);
	void  (*setOtherCharSize)(uint32_t);
	void *(*getFontConfItem)(void *, host_cmd_font_conf_item_t *);
	void  (*setFontConfItemStart)(void);
	void  (*appendFontConfItem)(const host_cmd_font_conf_item_t *);
	void  (*setFontConfItemEnd)(void);
	void  (*resetFontConf)(void);
} HarfBuzz;

/*----------------------------------------------*
 | GLOBAL VARIABLES                             |
 *----------------------------------------------*/
extern HarfBuzz *harfBuzz;

#endif /* __HARFBUZZ_H__ */
