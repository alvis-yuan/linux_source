#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "libcommon.h"

/*----------------------------------------------*
 | Definitions and Constants                    |
 *----------------------------------------------*/
#define DOTS_PER_LINE			(sys_global_var()->prt_dots_per_line)
#define WORDS_PER_LINE			(DOTS_PER_LINE >> 5)
#define BYTES_PER_LINE			(DOTS_PER_LINE >> 3)
#define DOTS_PER_LINE_MAX		(576)
#define WORDS_PER_LINE_MAX		(18)
#define BYTES_PER_LINE_MAX		(72)
#define MAX_CHAR_HEIGHT			(24)
#define BYTES_PER_CHAR_ROW		(BYTES_PER_LINE * MAX_CHAR_HEIGHT)
/* Line Buffer related */
#define BASE_LINE				(191)	/* (MAX_LINE_HEIGHT-1 )   */
#define MAX_LINE_HEIGHT			(192)	/* (MAX_CHAR_HEIGHT   )*8 */
#define TOTAL_LINE_HEIGHT		(320)	/* (MAX_CHAR_HEIGHT+16)*8 */
/* Page Buffer related */
#define PAGE_BUFFER_MAX_HEIGHT	(2400)	/* 300mm */

#define CUTTING_POSITION_TO_BM	(208)
#define PRINT_POSITION_TO_BM	(136)

#define CUTTING_POSITION_TO_BM_1600E	(208-32)
#define PRINT_POSITION_TO_BM_1600E		(136)


#endif // __CONFIG_H__
