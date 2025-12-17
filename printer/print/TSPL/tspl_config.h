#ifndef __TSPL_CONFIG_H__
#define __TSPL_CONFIG_H__

#include "tspl_api.h"
/*----------------------------------------------*
 | Definitions and Constants                    |
 *----------------------------------------------*/
// #define DOTS_PER_LINE			(sys_global_var()->prt_dots_per_line)
// #define DOTS_PER_LINE			(384)
// #define WORDS_PER_LINE			(DOTS_PER_LINE >> 5)
// #define BYTES_PER_LINE			(DOTS_PER_LINE >> 3)
#define DOTS_PER_LINE_MAX		(576)
#define WORDS_PER_LINE_MAX		(18)
#define BYTES_PER_LINE_MAX		(72)
#define MAX_CHAR_HEIGHT			(24)
#define MAX_LINE_HEIGHT			(192)	/* (MAX_CHAR_HEIGHT  )*8 */
#define BASE_LINE				(191)	/* (MAX_LINE_HEIGHT-1)    */
#define TOTAL_LINE_HEIGHT		(240)	/* (MAX_CHAR_HEIGHT+6)*8 */
// #define TOTAL_LINE_HEIGHT		(240)	/* (MAX_CHAR_HEIGHT+6)*8 */
// #define BYTES_PER_CHAR_ROW		(BYTES_PER_LINE * MAX_CHAR_HEIGHT)
#define MAX_PAGE_HEIGHT         (MAX_LINE_HEIGHT * 10) /* pageBuffer height is 10 times as much to tspl_lineBuffer height */


typedef struct {
	uint8_t ModuleSize;
	uint8_t Columns;
	uint8_t Rows;
	uint8_t RowHeight;
	uint8_t ECLevel;
	uint8_t Options;
	uint16_t SymbolRows;
	uint16_t SymbolWidth;
} PDF417_t;

extern tspl_sdk_event_t sdk_event;
extern tspl_event_data_t sdk_data;

/*----------------------------------------------*
 | log_api 临时存放                              |
 *----------------------------------------------*/
#define localDbg(fmt,args...) fprintf(stderr,"%s|%d|%s() "fmt"\n",__FILE__,__LINE__,__func__,##args)
#define LogFatal(fmt,args...) localDbg(fmt,##args)
#define LogError(fmt,args...) localDbg(fmt,##args)
#define LogWarn(fmt,args...)  localDbg(fmt,##args)
#define LogInfo(fmt,args...)  localDbg(fmt,##args)
#define LogDbg(fmt,args...)   localDbg(fmt,##args)

#endif // __CONFIG_H__