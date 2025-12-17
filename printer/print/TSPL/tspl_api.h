#ifndef __TSPL_API_H__
#define __TSPL_API_H__

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TSPL_EVENT_SIZE,
    TSPL_EVENT_GAP,
    TSPL_EVENT_BLINE,
    TSPL_EVENT_OFFSET,
    TSPL_EVENT_SPEED,
    TSPL_EVENT_DENSITY,
    TSPL_EVENT_DIRECTION,
    // TSPL_EVENT_COUNTRY,
    // TSPL_EVENT_CODEPAGE,
    TSPL_EVENT_CLS,
    TSPL_EVENT_FEED,
    TSPL_EVENT_FORMFEED,
    TSPL_EVENT_HOME,
    TSPL_EVENT_PRINT,
    // TSPL_EVENT_SOUND,
    TSPL_EVENT_CUT,
    TSPL_EVENT_LIMITFEED,
    TSPL_EVENT_BAR,
    TSPL_EVENT_BARCODE,
    TSPL_EVENT_BITMAP,
    TSPL_EVENT_BOX,
    TSPL_EVENT_ERASE,
    TSPL_EVENT_DMATRIX,
    TSPL_EVENT_MAXICODE,
    TSPL_EVENT_PDF417,
    TSPL_EVENT_PUTPCX,
    TSPL_EVENT_REVERSE,
    TSPL_EVENT_TEXT,
    
    TSPL_EVENT_DEBUG,
    TSPL_EVENT_ERROR,
    TSPL_EVENT_SLEEP,
    
    TSPL_EVENT_MAX,
} tspl_event_enum;

typedef struct {
    uint32_t *bitmap;
    uint32_t validHeight;
} bitmap_t;

typedef struct {
    double n;
} tspl_one_param_t;

typedef struct {
    double m;
    double n;
} tspl_two_param_t;

typedef struct {
    bitmap_t BitMap;
}tspl_zero_param_t;

typedef struct {
    char *errtext;
    char *errline;
} tspl_err_t;

typedef union {
    tspl_zero_param_t zero_param;
    tspl_one_param_t one_param;
    tspl_two_param_t two_param;
    tspl_err_t err_info;
} tspl_event_data_t;

typedef struct {
    bool status;
    tspl_event_enum id;
    tspl_event_data_t *data;
} tspl_sdk_event_t;

typedef void (*tspl_sdk_event_callback)(tspl_sdk_event_t *event);

#define TSPL_SCRIPT_DIR "/tmp/print/"

int tspl_sdk_init(tspl_sdk_event_callback cb, uint32_t dots_per_line);
void tspl_sdk_destroy(void);
void tspl_swap_int32(uint32_t *word, uint32_t len);
void tspl_parser(char *buf, uint32_t buf_len, char *file);
#endif /* __TSPL_API_H__ */