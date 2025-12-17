#ifndef __TSPL_LABEL_FUNC_H__
#define __TSPL_LABEL_FUNC_H__

#include "tspl_api.h"
typedef struct {
    uint16_t p;
    uint16_t e;
    uint16_t m;
    uint16_t u[3];
    uint16_t w;
    uint16_t h;
    uint16_t r;
    uint16_t c;
    uint16_t t;
} PDF417_options;

typedef enum{
    MAXICODE_MODE_2,
    MAXICODE_MODE_3,
    MAXICODE_MODE_4,
    MAXICODE_MODE_5,
    MAXICODE_MODE_6,
} maxicode_mode_enum; 


struct LabelPrinter {
    maxicode_mode_enum maxicode_mode;
    char errtext[128];
    char errline[1024];
    void (*event_callback)(tspl_sdk_event_t *event);
    void (*exec_printf)(void);
    void (*exec_size)(void);
    void (*exec_gap)(void);
    void (*exec_bline)(void);
    void (*exec_offset)(void);
    void (*exec_speed)(void);
    void (*exec_density)(void);
    void (*exec_direction)(void);
    void (*exec_reference)(void);
    void (*exec_country)(void);
    void (*exec_codepage)(void);
    void (*exec_cls)(void);
    void (*exec_feed)(void);
    void (*exec_formfeed)(void);
    void (*exec_home)(void);
    void (*exec_print)(void);
    void (*exec_sound)(void);
    void (*exec_cut)(void);
    void (*exec_limitfeed)(void);
    void (*exec_switch2esc)(void);
    void (*exec_bar)(void);
    void (*exec_barcode)(void);
    void (*exec_bitmap)(void);
    void (*exec_box)(void);
    void (*exec_erase)(void);
    void (*exec_dmatrix)(void);
    void (*exec_maxicode)(void);
    void (*exec_pdf417)(void);
    void (*exec_putpcx)(void);
    void (*exec_reverse)(void);
    void (*exec_text)(void);
    void (*exec_download)(void);
    void (*exec_debug)(void);
    void (*exec_setlinespace)(void);
    void (*exec_sleep)(void);
    void (*skip_bitmap)(void);
    void (*skip_download)(void);
    void (*change_page_mode)(void);
};

typedef enum {NO_DELIMITER, HAS_DELIMITER} IS_DELIMITER;

#define GET_ARRAY_LEN(array)   ( sizeof(array) / sizeof(array[0]) )
#define GET_EXP_PARAMS_UNIT(params, pnum) get_exp_params(params, pnum, __func__, true, false)
#define GET_EXP_PARAMS(params, pnum)      get_exp_params(params, pnum, __func__, false, false)
#define GET_EXP_PARAMS_I(params, pnum)    get_exp_params(params, pnum, __func__, false, true)
#define GET_STR_PARAM(str_param, len)     get_str_param(str_param, len, __func__)
#define GET_BYTE_PARAM(byte_param, len)   get_byte_param(byte_param, len, __func__)
#define GET_OPTION_PARAMS(p, residue_len) get_option_params(p, residue_len, __func__)
#if 0
#define RECT_RANGE_LIMIT(x, y, w, h)  do {                  \
    if (x > DOTS_PER_LINE)        SERROR(23);     \
    if (y > MAX_PAGE_HEIGHT)      SERROR(23);     \
    if (x + w > DOTS_PER_LINE)    w = DOTS_PER_LINE - x;    \
    if (y + h > MAX_PAGE_HEIGHT)  h = MAX_PAGE_HEIGHT - y;  \
} while(0)
#endif

extern void get_exp_params(double *, uint32_t, const char *, bool, bool);
extern IS_DELIMITER get_str_param(char *, const uint32_t , const char *);
extern void get_byte_param(uint8_t *, const uint32_t, const char *);
extern uint8_t get_option_params(double *, uint32_t, const char *);

extern struct LabelPrinter *labelPrinter;
#endif