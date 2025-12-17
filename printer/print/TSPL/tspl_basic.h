#ifndef __TSPL_BASIC_H__
#define __TSPL_BASIC_H__

#include <stdio.h>
#include <stdbool.h>
#include <setjmp.h>
#include <stdint.h>

#define NUM_LAB             100
#define LAB_LEN             10
#define FOR_NEST            25 //嵌套层级上限
#define SUB_NEST            25
#define LABLE_NUM           25
#define GOSUB_NEST          25
#define MAX_VARIABLE_NAME   64
#define MAX_VARIABLE_NUM    64
#define STRVAR_LEN          128
#define TOKEN_SIZE          128
#define PROG_SIZE           (1024 * 10)
#define FILENAME_LEN        128
#define FILE_FULL_PATH      "/tmp/print/file/"
/* 标识符类型 TokenType */
#define DELIMITER   1
#define VARIABLE    2
#define STRVARIABLE 3
#define NUMBER      4
#define COMMAND     5
#define STRING      6
#define QUOTE       7
#define FUNCTION    8
#define UNIT        9
/* 关键字 Tok */
enum {
    /* basic 关键字 */
    PRINTF_ID = 1  ,
    INPUT_ID       ,
    IF_ID          ,
    THEN_ID        ,
    ELSE_ID        ,
    FOR_ID         ,
    NEXT_ID        ,
    TO_ID          ,
    GOTO           ,
    EOL_ID         ,
    FINISHED_ID    ,
    GOSUB_ID       ,
    RETURN_ID      ,
    END_ID         ,
    EOP_ID         ,
    REM_ID         ,
    OPEN_ID        ,
    READ_ID        ,
    SEEK_ID        ,
    FREAD_ID       ,
    STR_ID         ,
    VAR_ID         ,
    ASC_ID         ,
    ABS_ID         ,
    INT_ID         ,
    LEN_ID         ,
    MID_ID         ,
    LEFT_ID        ,
    RIGHT_ID       ,
    CHR_ID         ,
    LOF_ID         ,
    /* 打印机指令 */
    SIZE_ID        ,
    GAP_ID         ,
    BLINE_ID       ,
    OFFSET_ID      ,
    SPEED_ID       ,
    DENSITY_ID     ,
    DIRECTION_ID   ,
    REFERENCE_ID   ,
    COUNTRY_ID     ,
    CODEPAGE_ID    ,
    CLS_ID         ,
    FEED_ID        ,
    FORMFEED_ID    ,
    HOME_ID        ,
    PRINT_ID       ,
    SOUND_ID       ,
    CUT_ID         ,
    LIMITFEED_ID   ,
    SWITCHTOESC_ID ,
    /* 卷标内容设计指令 */
    BAR_ID         ,
    BARCODE_ID     ,
    BITMAP_ID      ,
    BOX_ID         ,
    ERASE_ID       ,
    DMATRIX_ID     ,
    MAXICODE_ID    ,
    PDF417_ID      ,
    PUTPCX_ID      ,
    REVERSE_ID     ,
    TEXT_ID        ,
    DOWNLOAD_ID    ,
    /* 自定义指令 */
    DEBUG_ID       ,
    SETLINESAPCE_ID,
    SLEEP_ID       ,
};

extern jmp_buf e_buf;

extern void serror(int error, const char *funcname, int line);
#define SERROR(n)            serror(n, __func__, __LINE__)

typedef struct {
    char name[MAX_VARIABLE_NAME];
    double value;
} variable_t;

typedef struct {
    char name[MAX_VARIABLE_NAME];
    char content[STRVAR_LEN];
} str_variable_t;


typedef struct {
    variable_t variable[MAX_VARIABLE_NUM];
    str_variable_t str_variable[STRVAR_LEN];
    uint32_t valid_cnt;
    uint32_t str_valid_cnt;
} variable_map_t;

typedef struct {
    FILE *f;
    char filename[FILENAME_LEN];
} file_handler_t;

struct basic {
    char *Prog;
    char *ProgHead;
    char *ProgTail;
    uint32_t ProgLen;

    char    *Token;     /* 具体内容 */
    uint32_t TokenType; /* 记录标识符类型 */
    uint32_t Tok;       /* 记录关键字 */

    struct _commands{
        char Command[20];
        uint32_t Tok;
    } *Table;

    void (*init)(void);
    bool (*load_program_from_file)(const char *);
    bool (*load_program_from_memory)(const char *, uint32_t);
    int (*get_token)(void);
    void (*put_back)(void);
    void (*scan_labels)(void);
    void (*exec_if)(void);
    void (*exec_for)(void);
    void (*exec_next)(void);
    void (*exec_goto)(void);
    void (*exec_gosub)(void);
    void (*exec_return)(void);
    void (*exec_rem)(void);
    void (*exec_open)(void);
    void (*exec_read)(void);
    void (*exec_seek)(void);
    void (*find_eol)(void);
    void (*var_assignment)(void);
    void (*str_assignment)(void);
    void (*get_exp)(double *);
    void (*get_str)(char *, uint32_t);
};

extern struct basic *BasicParser;

#endif