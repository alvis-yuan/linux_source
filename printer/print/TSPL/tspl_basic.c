#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include "tspl_basic.h"
#include "tspl_calculator.h"
#include "tspl_label_func.h"
#include "tspl_config.h"

struct for_stack {
    uint8_t var_index;
    uint32_t target;
    char *local;
};

struct label {
    char *local;
    char name[32];
};

typedef struct {
    char *gosub_pos;
} GosubStack;

jmp_buf e_buf;   /* hold environment for longjmp() */
static double theVariables[26]                = {};
static char theStrVariables[26][STRVAR_LEN]   = {};
static variable_map_t theVariableMap          = {};
static variable_map_t *variableMap            = &theVariableMap;
static char theProg[PROG_SIZE]                = {};
static char theToken[TOKEN_SIZE]              = {};
static struct for_stack theForstack[FOR_NEST] = {};
static int8_t theForstack_pos                 = 0;
static GosubStack theGosubStack[GOSUB_NEST]   = {};
static int8_t theGosubStack_pos               = 0;
static struct label theLabels[LABLE_NUM]      = {};
file_handler_t fileH[2]                       = {};

static struct commands {
    char Command[20];
    uint32_t Tok;
} theCommandTable[] = {
    /* basic 指令 */
    {"printf",  PRINTF_ID},
    {"input",   INPUT_ID},
    {"if",      IF_ID},
    {"then",    THEN_ID},
    {"else",    ELSE_ID},
    {"goto",    GOTO},
    {"for",     FOR_ID},
    {"next",    NEXT_ID},
    {"to",      TO_ID},
    {"gosub",   GOSUB_ID},
    {"return",  RETURN_ID},
    {"end",     END_ID},
    {"eop",     EOP_ID},
    {"rem",     REM_ID},
    {"open",    OPEN_ID},
    {"read",    READ_ID},
    {"seek",    SEEK_ID},
    {"fread",   FREAD_ID},
    /* 打印机指令 */
    {"size",            SIZE_ID},
    {"gap",             GAP_ID},
    {"bline",           BLINE_ID},
    {"offset",          OFFSET_ID},
    {"speed",           SPEED_ID},
    {"density",         DENSITY_ID},
    {"direction",       DIRECTION_ID},
    {"reference",       REFERENCE_ID},
    {"country",         COUNTRY_ID},
    {"codepage",        CODEPAGE_ID},
    {"cls",             CLS_ID},
    {"feed",            FEED_ID},
    {"formfeed",        FORMFEED_ID},
    {"home",            HOME_ID},
    {"print",           PRINT_ID},
    {"sound",           SOUND_ID},
    {"cut",             CUT_ID},
    {"limitfeed",       LIMITFEED_ID},
    {"switchtoesc",     SWITCHTOESC_ID},
    /* 卷标内容设计指令 */
    {"bar",             BAR_ID},
    {"barcode",         BARCODE_ID},
    {"bitmap",          BITMAP_ID},
    {"box",             BOX_ID},
    {"erase",           ERASE_ID},
    {"dmatrix",         DMATRIX_ID},
    {"maxicode",        MAXICODE_ID},
    {"pdf417",          PDF417_ID},
    {"putpcx",          PUTPCX_ID},
    {"reverse",         REVERSE_ID},
    {"text",            TEXT_ID},
    /* 档案管理指令 */
    {"download",        DOWNLOAD_ID},
    /* 自定义指令 */
    {"debug",           DEBUG_ID},
    {"setlinespace",    SETLINESAPCE_ID},
    {"sleep",           SLEEP_ID},
};

static struct functions {
    char Function[20];
    uint32_t Tok;
} theFunctionTable[] = {
    {"str$",    STR_ID},
    {"var",     VAR_ID},
    {"asc",     ASC_ID},
    {"abs",     ABS_ID},
    {"int",     INT_ID},
    {"len",     LEN_ID},
    {"mid$",    MID_ID},
    {"left$",   LEFT_ID},
    {"right$",  RIGHT_ID},
    {"chr$",    CHR_ID},
    {"lof",     LOF_ID},
    {"fread$",  FREAD_ID},
};

static int issame_label(char *s);

static void init(void)
{
    memset(theVariables,    0, sizeof(theVariables));
    memset(&theVariableMap, 0, sizeof(theVariableMap));
    memset(theStrVariables, 0, sizeof(theStrVariables));
    memset(theProg,         0, sizeof(theProg));
    memset(theToken,        0, sizeof(theToken));
    memset(theForstack,     0, sizeof(theForstack));
    memset(theLabels,       0, sizeof(theLabels));
    if (fileH[0].f) fclose(fileH[0].f);
    if (fileH[1].f) fclose(fileH[1].f);
    memset(&fileH,          0, sizeof(fileH));
    theForstack_pos        = 0;
    BasicParser->Prog      = theProg;
    BasicParser->Token     = theToken;
    BasicParser->TokenType = 0;
    BasicParser->Tok       = 0;
    labelPrinter->maxicode_mode = MAXICODE_MODE_4; // 每个脚本运行时都默认4模式
}

static void printf_errorline(void)
{
    char *p, *start, *end;
    char errorline[1024];
    p = BasicParser->Prog;

    if (*p == '\n' && p != BasicParser->ProgHead) p--;
    while ((*p != '\n' && *p != '\r') && p != BasicParser->ProgHead) p--;
    start = p;
    p = BasicParser->Prog;
    while ((*p != '\n' && *p != '\r') && p != BasicParser->ProgTail) p++;
    end = --p;
    snprintf(errorline, end - start + strlen("[errorline]: ") + 2, "[errorline]: %s", start);
    strcpy(labelPrinter->errline, errorline);
    LogError("%s", errorline);
}

extern jmp_buf e_buf;
void serror(int error, const char *funcname, int line)
{
    static char  *e[]  =  {
        "syntax error",    // 0
        "unbalanced parentheses",
        "no expression present",
        "equal sign expected",
        "not a variable",
        "label table full",
        "duplicate label",
        "undefined label",
        "THEN expected",
        "TO expected",
        "too many nested FOR loops", // 10
        "NEXT without FOR",
        "too many nested GOSUB",
        "RETURN without GOSUB",
        "fail to read program",
        "variable name only support 1 alpha",//15
        "label name conflict with command",
        "not a str variable",
        "str expression syntax",
        "var expression syntax",
        "operator can not continue",//20
        "function error",
        "printer command error",
        "printer command param error",
        "tspl_lineBuffer is not empty",//24
        "script file too large!",
        "unknown command",
        "coordinate_point error",
        "the program too large!",//28
        "malloc fail!",
        "the command dont need unit",
        "the declared variables come up to max",//31
        "hey bro, you cant just use variable without declare it",
        "variable name too long",
        "open handle only support 0 or 1",
        "open file fail (ram may have no this file)",
        "you cant just operate a file without open it",//36
        "basic param error",
        "cant get file size",
        "risk of spillover"
    };
    if (funcname != NULL)
        snprintf(labelPrinter->errtext, sizeof(labelPrinter->errtext),
                "[%s]|%d|%s", funcname, line, e[error]);
    else
        snprintf(labelPrinter->errtext, sizeof(labelPrinter->errtext),
                "%s", e[error]);

    LogError("%s", labelPrinter->errtext);
    printf_errorline();
    
    sdk_event.id = TSPL_EVENT_ERROR;
    sdk_event.status = true;
    sdk_event.data->err_info.errtext = labelPrinter->errtext;
    sdk_event.data->err_info.errline = labelPrinter->errline;
    labelPrinter->event_callback(&sdk_event);

    longjmp(e_buf,1);  /* return to save point */
}

static int iswhite(char c)
{
    if (c == ' ')
        return 1;
    return 0;
}

static int isdelim(char c)
{
    if (strchr(";,+-<>/*\%^=() \"", c) || c == '\t' || c == '\r' || c == '\n' || c == 0) {
        return 1;
    }
    return 0;
}

/* 回滚一个单元 */
static void put_back(void)
{
    char *t;
    t = BasicParser->Token;
    while(*t) {
        BasicParser->Prog--; 
        t++;
    }

    if (BasicParser->TokenType == QUOTE) {
        BasicParser->Prog--;
        BasicParser->Prog--;
    }
}

static int lookup_command(char *s)
{
    uint32_t len, i;
    char *p = NULL;

    p = s;
    while (*p) {*p = tolower(*p); p++;}

    len = sizeof(theCommandTable) / sizeof(theCommandTable[0]);
    for (i = 0; i < len; i++) {
        if (!strcmp(theCommandTable[i].Command, s)) {
            return theCommandTable[i].Tok;
        }
    }
    return 0;
}

static int lookup_function(char *s)
{
    uint32_t len, i;
    char *p = NULL;

    p = s;
    while (*p) {*p = tolower(*p); p++;}

    len = sizeof(theFunctionTable) / sizeof(theFunctionTable[0]);
    for (i = 0; i < len; i++) {
        if (!strcmp(theFunctionTable[i].Function, s)) {
            return theFunctionTable[i].Tok;
        }
    }
    return 0;
}

static bool load_program_from_file(const char *filename)
{
    FILE *fp;
    size_t flen, rlen;

    if (!(fp = fopen(filename, "r"))) {
        LogFatal("open %s failed!, maybe this file is not exist, please check..", filename);
        return false;
    }

    fseek(fp, 0, SEEK_END);
    flen = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (flen > PROG_SIZE) {
        fclose(fp);
        SERROR(25);
    }
    
    rlen = fread(theProg, 1, PROG_SIZE, fp);
    fclose(fp);
    if (!rlen) {
        LogError("read file failed. err: %s", strerror(errno));
        SERROR(14);
    }

    theProg[rlen] = 0; /* null terminate the program  */
    BasicParser->ProgLen = rlen;
    BasicParser->ProgTail = &theProg[rlen];
    return true;
}

static bool load_program_from_memory(const char *sbuf, uint32_t len)
{
    if (len > PROG_SIZE)
        return false;
    memcpy(theProg, sbuf, len);
    BasicParser->ProgLen = len + 1;/* 1 for '\0' */
    BasicParser->ProgTail = &theProg[len + 1];
    return true;
}

static int get_token(void)
{
    char *temp;

    BasicParser->TokenType = 0;
    BasicParser->Tok = 0;
    temp = BasicParser->Token;

    while (iswhite(*BasicParser->Prog)) BasicParser->Prog++;

    /* 文件结束 */
    if (*BasicParser->Prog == '\0') {
        *BasicParser->Token = 0;
        BasicParser->Tok = FINISHED_ID;
        return (BasicParser->TokenType = DELIMITER);
    }

    /* 换行符处理 */
    if (*BasicParser->Prog == '\r' || *BasicParser->Prog == '\n') {
        *temp = *BasicParser->Prog;
        temp++; BasicParser->Prog++;
        if (*BasicParser->Prog == '\r' || *BasicParser->Prog == '\n') {
            *temp = *BasicParser->Prog;
            temp++; BasicParser->Prog++;
        }
        BasicParser->Tok = EOL_ID;
        *temp = 0;
        return (BasicParser->TokenType = DELIMITER);
    }

    /* 在"+-*^/%=;(),><:"查找prog */ 
    if (strchr("+-*/^\%=;(),><:", *BasicParser->Prog)) {
        *temp = *BasicParser->Prog;
        temp++;BasicParser->Prog++;
        *temp = 0;
        return (BasicParser->TokenType = DELIMITER);
    }

    /* 找常量字符串 */
    if (*BasicParser->Prog == '"') {
        BasicParser->Prog++;
        while (*BasicParser->Prog != '"' && *BasicParser->Prog != '\r' && *BasicParser->Prog != '\n') *temp++ = *BasicParser->Prog++;
        if (*BasicParser->Prog == '\r' || *BasicParser->Prog == '\n')
            SERROR(1);
        *temp = 0; BasicParser->Prog++;
        return (BasicParser->TokenType = QUOTE);
    }

    /* 数字处理 */
    if (isdigit(*BasicParser->Prog)) {
        while (!isdelim(*BasicParser->Prog) && !isalpha(*BasicParser->Prog)) *temp++ = *BasicParser->Prog++;
        *temp = 0;
        return (BasicParser->TokenType = NUMBER);
    }

    /* 处理字符串（判断是变量、命令、字符变量、单位、函数还是字符串） */
    if (isalpha(*BasicParser->Prog)) {
        while (!isdelim(*BasicParser->Prog)) *temp++ = *BasicParser->Prog++;
        BasicParser->TokenType = STRING;
    }
    *temp = 0; 
    if (BasicParser->TokenType == STRING) {
        BasicParser->Tok = lookup_command(BasicParser->Token);
        if (BasicParser->Tok) {
            BasicParser->TokenType = COMMAND;
        }
        else if (strlen(BasicParser->Token) == 2 && !strcmp("mm", BasicParser->Token)) {
            BasicParser->TokenType = UNIT;
        }
        else if (*BasicParser->Prog == '(') {
            BasicParser->Tok = lookup_function(BasicParser->Token);
            if (BasicParser->Tok) {
                BasicParser->TokenType = FUNCTION;
            }
        }
        else { /* 剩下的就是变量、字符串变量和标签名称了，支持多字母名称 */
            int len = strlen(BasicParser->Token);
            char *t, *p;
            t = BasicParser->Token;
            p = BasicParser->Prog;
            while(*t) {
                p--; 
                t++;
            }
            p--;/* 再回退一次应到达':' */
            if (*p == ':' || issame_label(BasicParser->Token))
                BasicParser->TokenType = STRING;
            else if (BasicParser->Token[len-1] == '$')
                BasicParser->TokenType = STRVARIABLE;
            else
                BasicParser->TokenType = VARIABLE;
        }
    }

    if (BasicParser->TokenType == 0)
        SERROR(0);
    return BasicParser->TokenType;
}

static void get_str(char *out, uint32_t len);
static void get_exp(double *out);

static double basic_var(void)
{
    char buf[64] = {0};
    double variable = 0;
    get_token();
    if (*BasicParser->Token != '(') SERROR(21);
    get_str(buf, sizeof(buf));
    variable = atof(buf); //VAR(x)
    get_token();
    if (*BasicParser->Token != ')') SERROR(21);
    return variable;
}

static double basic_asc(void)
{
    char buf[64] = {0};
    double variable = 0;
    get_token();
    if (*BasicParser->Token != '(') SERROR(21);
    get_str(buf, sizeof(buf));
    variable = buf[0];
    get_token();
    if (*BasicParser->Token != ')') SERROR(21);
    return variable;
}

#define BASIC_ABS(x) ((x>=0) ? (x) : -(x))
static double basic_abs(void)
{
    double variable = 0;
    get_token();
    if (*BasicParser->Token != '(') SERROR(21);

    get_exp(&variable);
    variable = BASIC_ABS(variable);
    get_token();
    if (*BasicParser->Token != ')') SERROR(21);
    return variable;
}

static double basic_int(void)
{
    double variable = 0;
    get_token();
    if (*BasicParser->Token != '(') SERROR(21);

    get_exp(&variable);
    variable = (double)(uint32_t)variable;
    get_token();
    if (*BasicParser->Token != ')') SERROR(21);
    return variable;
}

static double basic_len(void)
{
    char buf[1024] = {0};
    double variable = 0;
    get_token();
    if (*BasicParser->Token != '(') SERROR(21);
    get_str(buf, sizeof(buf));
    variable = strlen(buf);
    get_token();
    if (*BasicParser->Token != ')') SERROR(21);
    return variable;
}

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static double basic_lof(void)
{
    char filename[FILENAME_LEN] = {};
    double filesize = 0;
    get_token();
    if (*BasicParser->Token != '(') SERROR(21);
    get_str(filename, sizeof(filename));

    int i;
    for (i = 0; i < 2; i++) {
        //FIXME: 当前操作是如果没有打开则不能获取文件大小，不清楚TSPL在获取文件大小前需不需要打开文件
        if (!strcmp(fileH[i].filename, filename) && fileH[i].f) {
            struct stat file_info;
            char full_pathName[FILENAME_LEN + 128];

            snprintf(full_pathName, sizeof(full_pathName), "%s%s", FILE_FULL_PATH, filename);

            if (stat(full_pathName, &file_info) != 0) {
                localDbg("%s", strerror(errno));
                SERROR(38);
            }
            filesize = file_info.st_size;
            break;
        }
        else {
            SERROR(36);
        }
    }
    get_token();
    if (*BasicParser->Token != ')') SERROR(21);
    return filesize;
}

static void basic_str(char *buf, int size)
{
    double value = 0;
    get_token();
    if (*BasicParser->Token != '(') SERROR(21);
    get_exp(&value);
    gcvt(value, 10, buf);// FIXME: 这里怎么防止buf溢出？
    get_token();
    if (*BasicParser->Token != ')') SERROR(21);
}

static void basic_mid(char *s, int size)
{
    char buf[1024] = {0};
    double m, n;
    get_token();
    if (*BasicParser->Token != '(') SERROR(21);
    GET_STR_PARAM(buf, sizeof(buf));
    GET_EXP_PARAMS_I(&m, 1);
    GET_EXP_PARAMS_I(&n, 1);
    get_token();
    if (*BasicParser->Token != ')') SERROR(21);

    if (n + 1 > size) SERROR(39);
    snprintf(s, n + 1, "%s", buf + (uint32_t)m);
}

static void basic_left(char *s, int size)
{
    char buf[1024] = {0};
    double n;
    get_token();
    if (*BasicParser->Token != '(') SERROR(21);
    GET_STR_PARAM(buf, sizeof(buf));
    GET_EXP_PARAMS_I(&n, 1);
    get_token();
    if (*BasicParser->Token != ')') SERROR(21);

    if (n + 1 > size) SERROR(39);
    snprintf(s, n + 1, "%s", buf); 
}

static void basic_right(char *s, int size)
{
    char buf[1024] = {0};
    double n;
    get_token();
    if (*BasicParser->Token != '(') SERROR(21);
    GET_STR_PARAM(buf, sizeof(buf));
    GET_EXP_PARAMS_I(&n, 1);
    get_token();
    if (*BasicParser->Token != ')') SERROR(21);

    if (n + 1 > size) SERROR(39);
    snprintf(s, n + 1, "%s", buf + strlen(buf) - (uint32_t)n); 
}

static void basic_chr(char *buf, int size)
{
    double value = 0;
    
    if (size < 2)
        SERROR(39);

    get_token();
    if (*BasicParser->Token != '(') SERROR(21);
    GET_EXP_PARAMS_I(&value, 1);
    buf[0] = (char)value;
    buf[1] = 0;
    get_token();
    if (*BasicParser->Token != ')') SERROR(21);
}

static void basic_fread(char *buf, int size)
{
    double handle, byte;
    get_token();
    if (*BasicParser->Token != '(') SERROR(21);
    
    GET_EXP_PARAMS_I(&handle, 1);
    if (handle != 0 && handle != 1)
        SERROR(34);
    if (fileH[(int)handle].f == NULL)
        SERROR(35);

    GET_EXP_PARAMS_I(&byte, 1);
    if (size < byte)
        SERROR(39);

    fread(buf, 1, byte, fileH[(int)handle].f);

    get_token();
    if (*BasicParser->Token != ')') SERROR(21);
}

static void malloc_variable(char *name, double value)
{
    if (variableMap->valid_cnt >= MAX_VARIABLE_NUM)
        SERROR(31);
    if (strlen(name) >= MAX_VARIABLE_NAME)
        SERROR(33);

    char *p = name;
    while (*p) {*p = tolower(*p); p++;}

    uint32_t i, same_index = 0xffffffff;
    for (i = 0; i < variableMap->valid_cnt; i++) {
        if (!strcmp(variableMap->variable[i].name, name)) {
            same_index = i;
            break;
        }
    }
    if (same_index != 0xffffffff) {//有相同名字的变量，直接改变该变量的值
        variableMap->variable[same_index].value = value;
    }
    else {
        strncpy(variableMap->variable[variableMap->valid_cnt].name, name, MAX_VARIABLE_NAME);
        variableMap->variable[variableMap->valid_cnt].value = value;
        variableMap->valid_cnt++;
    }
}

static double fetch_variable(char *name)
{
    uint32_t i;
    char *p = NULL;

    p = name;
    while (*p) {*p = tolower(*p); p++;}

    for (i = 0; i < MAX_VARIABLE_NUM; i++) {
        if (!strcmp(variableMap->variable[i].name, name)) {
            return variableMap->variable[i].value;
        }
    }

    SERROR(32);/* 没有找到该变量 */
    return 0.0;
}

static void malloc_strVariable(char *name, char *content)
{
    if (variableMap->str_valid_cnt >= MAX_VARIABLE_NUM)
        SERROR(31);
    if (strlen(name) >= MAX_VARIABLE_NAME)
        SERROR(33);

    char *p = name;
    while (*p) {*p = tolower(*p); p++;}
    
    uint32_t i, same_index = 0xffffffff;
    for (i = 0; i < variableMap->str_valid_cnt; i++) {
        if (!strcmp(variableMap->str_variable[i].name, name)) {
            same_index = i;
            break;
        }
    }
    if (same_index != 0xffffffff) {//有相同名字的变量，直接改变该变量的值
        strncpy(variableMap->str_variable[same_index].content, content, STRVAR_LEN);
    }
    else {
        strncpy(variableMap->str_variable[variableMap->str_valid_cnt].name, name, MAX_VARIABLE_NAME);
        strncpy(variableMap->str_variable[variableMap->str_valid_cnt].content, content, STRVAR_LEN);
        variableMap->str_valid_cnt++;
    }
}

static char *fetch_strVariable(char *name)
{
    uint32_t i;
    char *p = NULL;

    p = name;
    while (*p) {*p = tolower(*p); p++;}

    for (i = 0; i < MAX_VARIABLE_NUM; i++) {
        if (!strcmp(variableMap->str_variable[i].name, name)) {
            return variableMap->str_variable[i].content;
        }
    }

    SERROR(32);/* 没有找到该变量 */
    return NULL;
}

static void get_exp(double *out)
{
    char     expression[256] = {0};
    char     var_str[64]     = {0};
    char     *pos            = expression;
    double   variable        = 0;
    uint32_t token_type      = 0;

    do {
        token_type = get_token();
        switch (token_type) {
        case NUMBER:
            pos += snprintf(pos, expression+sizeof(expression)-pos, "%s", BasicParser->Token);
            break;
        case DELIMITER:
            if (*BasicParser->Token == '+' || *BasicParser->Token == '-' || *BasicParser->Token == '*' || *BasicParser->Token == '/')
                pos += snprintf(pos, expression+sizeof(expression)-pos, "%s", BasicParser->Token);
            else
                goto _out;
            break;
        case VARIABLE:
            variable = fetch_variable(BasicParser->Token);
            gcvt(variable, 10, var_str);
            pos += snprintf(pos, expression+sizeof(expression)-pos, "%s", var_str);
            break;
        case FUNCTION:
            switch (BasicParser->Tok) {
            case VAR_ID:   variable = basic_var();     break;
            case ASC_ID:   variable = basic_asc();     break;
            case ABS_ID:   variable = basic_abs();     break;
            case INT_ID:   variable = basic_int();     break;
            case LEN_ID:   variable = basic_len();     break;
            case LOF_ID:   variable = basic_lof();     break;
            default:    SERROR(21);           break;
            }
            gcvt(variable, 10, var_str);
            pos += snprintf(pos, expression+sizeof(expression)-pos, "%s", var_str);
            break;
        default:
            // SERROR(19);
            break;
        }
    } while (BasicParser->Tok != EOL_ID && (token_type == NUMBER || token_type == DELIMITER || token_type == VARIABLE || token_type == FUNCTION)); //FIXME:这里没有考虑同一行上面还有另一个命令的情况

_out:
    put_back();
    *out = calculate(expression, sizeof(expression));
}

static void get_str(char *out, const uint32_t len)
{
    char     buf[1024]    = {0};
    uint32_t token_type = 0;
    char     *temp      = NULL;
    char     *pos       = out;
    do {
        token_type = get_token();
        switch (token_type) {
        case QUOTE:
            pos += snprintf(pos, out+len-pos, "%s", BasicParser->Token);
            break;
        case DELIMITER:
            if (*BasicParser->Token != '+')
                goto _exit;
            break;
        case STRVARIABLE:
            pos += snprintf(pos, out+len-pos, "%s", fetch_strVariable(BasicParser->Token));
            break;
        case FUNCTION:
            switch (BasicParser->Tok) {
            case STR_ID:    basic_str(buf, sizeof(buf));         break;
            case MID_ID:    basic_mid(buf, sizeof(buf));         break;
            case LEFT_ID:   basic_left(buf, sizeof(buf));        break;
            case RIGHT_ID:  basic_right(buf, sizeof(buf));       break;
            case CHR_ID:    basic_chr(buf, sizeof(buf));         break;
            case FREAD_ID:  basic_fread(buf, sizeof(buf));       break;
            default:        SERROR(21);    break;
            }
            pos += snprintf(pos, out+len-pos, "%s", buf);
            break;
        default:
            break;
        }
    } while (BasicParser->Tok != EOL_ID && (token_type == QUOTE || token_type == DELIMITER || token_type == STRVARIABLE || token_type == FUNCTION));

_exit:
    /* put_back是为了防止一种情况：
     *  1.如果多采的一个 DELIMITER 是')'，为了不破坏平衡，需要回退给函数去采集
     *  但是如果put_back返回后位置落在'"'上时，会使下一次采集QUOTE抛出 unbalanced parentheses 的错误
     *  因此回退后如若遇到'"'，则没必要回退了
     */
    temp = BasicParser->Prog;
    put_back();
    if (*BasicParser->Prog == '"')
        BasicParser->Prog = temp;
    return;
}

static void var_assignment(void)
{
    double value = 0;
    char name[MAX_VARIABLE_NUM];
    get_token();
    if (!isalpha(*BasicParser->Token))
        SERROR(4);
    strcpy(name, BasicParser->Token);
    get_token();
    if (*BasicParser->Token != '=')
        SERROR(3);
    get_exp(&value);
    malloc_variable(name, value);
}

static void str_assignment(void)
{
    char name[MAX_VARIABLE_NAME];
    char buf[STRVAR_LEN] = {0};
    get_token();
    if (!isalpha(*BasicParser->Token))
        SERROR(17);
    strncpy(name, BasicParser->Token, MAX_VARIABLE_NAME);
    get_token();
    if (*BasicParser->Token != '=')
        SERROR(3);
    get_str(buf, sizeof(buf));
    malloc_strVariable(name, buf);
}

static void find_eol(void)
{
    do {
        get_token();
    } while (BasicParser->Tok != EOL_ID && BasicParser->Tok != FINISHED_ID);
}

static void exec_if(void)
{
    double x, y;
    uint8_t cond;
    char op;

    get_exp(&x);

    get_token();
    //FIXME: 暂不支持>= <=
    if (*BasicParser->Token != '>' && *BasicParser->Token != '<' && *BasicParser->Token == '=')
        SERROR(0);
    op = *BasicParser->Token;

    get_exp(&y);

    cond = 0;
    switch (op)
    {
    case '<':
        if (x < y) cond = 1;
        break;
    case '>':
        if (x > y) cond = 1;
        break;
    case '=':
        if (x == y) cond = 1;
        break;
    default:
        break;
    }

    if (cond) {
        get_token();
        if (BasicParser->Tok != THEN_ID) {
            SERROR(8);
        }
    }
    else {
        do {
            get_token();
            if (BasicParser->Tok == EOL_ID)
                return;////>>>>//??
        } while (BasicParser->Tok != ELSE_ID);
    }
    // find_eol();
}

static void for_push(struct for_stack i)
{
    if (theForstack_pos >= FOR_NEST)
        SERROR(10);
    theForstack[theForstack_pos] = i;
    theForstack_pos++;
}

static struct for_stack for_pop(void)
{
    theForstack_pos--;
    if (theForstack_pos < 0)
        SERROR(11);
    return theForstack[theForstack_pos];
}

static void exec_for(void)
{
    struct for_stack i;
    double value1 = 0, value2 = 0;

    get_token();
    if (!isalpha(*BasicParser->Token))
        SERROR(4);
    i.var_index = toupper(*BasicParser->Token) - 'A';

    get_token();
    if (*BasicParser->Token != '=')
        SERROR(3);

    get_exp(&value1);
    theVariables[i.var_index] = value1;

    get_token();
    if (BasicParser->Tok != TO_ID)
        SERROR(9);
    
    get_exp(&value2);
    i.target = value2;

    if (value1 <= i.target) {
        i.local = BasicParser->Prog;
        for_push(i);
    }
    else {
        while (BasicParser->Tok != NEXT_ID)
            get_token();
    }
}

static void exec_next(void)
{
    struct for_stack i;
    i = for_pop();
    theVariables[i.var_index]++;
    if (theVariables[i.var_index] > i.target)
        return;
    for_push(i);
    BasicParser->Prog = i.local;
}

static int issame_label(char *s)
{
    uint8_t addr;
    for (addr = 0; addr < LABLE_NUM; addr++) {
        if (strlen(theLabels[addr].name) == 0)
            return 0;
        if (!strcmp(theLabels[addr].name, s))
            return 1;
    }
    return 0;
}

static int isconflict_with_command(char *s)
{
    int i, len;
    char *p = s;
    while (*p) {*p = tolower(*p); p++;}
    len = sizeof(theCommandTable) / sizeof(theCommandTable[0]);
    for (i = 0; i < len; i++) {
        if(!strcmp(theCommandTable[i].Command, s))
            return 1;
    }
    return 0;
}

//FIXME: bug 如果输入小写标签也支持
static void scan_labels(void)
{
    uint8_t addr = 0;
    char *temp = NULL;
    uint32_t token_type = 0;

    temp = BasicParser->Prog;
    do {
        token_type = get_token();
        /* 由于BITMAP中有可能出现不是字符串的byte 例如 0 ff ，所以遇到bitmap要避免使用get_token()
         * 直接跳过，去找下一个command
         */
        if (BasicParser->Tok == BITMAP_ID) {
            labelPrinter->skip_bitmap();
        }
        else if (BasicParser->Tok == DOWNLOAD_ID) {
            labelPrinter->skip_download();
        }
        else if (BasicParser->Tok == REM_ID) {
            BasicParser->exec_rem();
        }
        else if (token_type != DELIMITER) {
            find_eol();
        }
        else if (token_type == DELIMITER && *BasicParser->Token == ':') {
            if (addr >= LABLE_NUM)
                SERROR(5);
            token_type = get_token();
            if (token_type == STRING) { /* 检测是否是字符串 */
                if (issame_label(BasicParser->Token))
                    SERROR(6);
                if (isconflict_with_command(BasicParser->Token))
                    SERROR(16);
                strcpy(theLabels[addr].name, BasicParser->Token);
                find_eol();
                theLabels[addr].local = BasicParser->Prog;
                addr++;
            }
        }
    } while (BasicParser->Tok != FINISHED_ID);

    BasicParser->Prog = temp;
}

static void exec_goto(void)
{
    uint8_t addr;
    char *p = NULL;
    get_token();
    if (BasicParser->TokenType != STRING)
        SERROR(7);
    p = BasicParser->Token;
    while (*p) {*p = tolower(*p); p++;}
    for (addr = 0; addr < LABLE_NUM; addr++) {
        if (!strcmp(theLabels[addr].name, BasicParser->Token)) {
            BasicParser->Prog = theLabels[addr].local;
            return;
        }
    }
    SERROR(7);
}

static void gosub_push(GosubStack g)
{
    if (theGosubStack_pos >= GOSUB_NEST)
        SERROR(12);
    theGosubStack[theGosubStack_pos] = g;
    theGosubStack_pos++;
}

static GosubStack *gosub_pop(void)
{
     theGosubStack_pos--;
    if (theGosubStack_pos < 0)
        SERROR(13);
    return &theGosubStack[theGosubStack_pos];
}

static void exec_gosub(void)
{
    uint8_t addr;
    char *p = NULL;
    GosubStack gostk;
    get_token();
    if (BasicParser->TokenType != STRING)
        SERROR(7);
    p = BasicParser->Token;
    while (*p) {*p = tolower(*p); p++;}
    for (addr = 0; addr < LABLE_NUM; addr++) {
        if (!strcmp(theLabels[addr].name, BasicParser->Token)) {
            gostk.gosub_pos = BasicParser->Prog;
            BasicParser->Prog = theLabels[addr].local;
            gosub_push(gostk);
            return;
        }
    }
    SERROR(7);
}

static void exec_return(void)
{
    GosubStack *gostkP;
    gostkP = gosub_pop();
    if (gostkP)
        BasicParser->Prog = gostkP->gosub_pos;
}

static void exec_rem(void)
{
    while (*BasicParser->Prog != '\n' && *BasicParser->Prog!= '\0')
        BasicParser->Prog++;
    while (*BasicParser->Prog == '\n' || *BasicParser->Prog == '\r')
        BasicParser->Prog++;
}

static void exec_open(void)
{
    char filename[FILENAME_LEN];
    char full_pathName[FILENAME_LEN+128];
    double handle;
    FILE *f;
    get_str(filename, FILENAME_LEN);
    get_exp(&handle);

    if (handle != 0 && handle != 1)
        SERROR(34);
    
    snprintf(full_pathName, sizeof(full_pathName), "%s%s", FILE_FULL_PATH, filename);

    f = fopen(full_pathName, "r");
    if (f == NULL)
        SERROR(35);

    if (fileH[(int)handle].f)
        fclose(fileH[(int)handle].f);
    
    fileH[(int)handle].f = f;
    strcpy(fileH[(int)handle].filename, filename);
}


static void exec_read(void)
{
#define SKIP_DELIMITER() get_token();if (BasicParser->TokenType != DELIMITER) SERROR(37)
    double handle;
    get_exp(&handle);

    if (handle != 0 && handle != 1)
        SERROR(34);
    if (fileH[(int)handle].f == NULL)
        SERROR(35);

    SKIP_DELIMITER();
    get_token();
    if (BasicParser->TokenType == STRVARIABLE)
        malloc_strVariable(BasicParser->Token, "hello i am read");
    else
        SERROR(37);

    SKIP_DELIMITER();
    get_token();
    if (BasicParser->TokenType == VARIABLE) //price
        malloc_variable(BasicParser->Token, 110);
    else
        SERROR(37);

    SKIP_DELIMITER();
    get_token();
    if (BasicParser->TokenType == VARIABLE) //quantity
        malloc_variable(BasicParser->Token, 119);
    else
        SERROR(37);

#undef SKIP_DELIMITER
}

static void exec_seek(void)
{
    double handle, offset;

    GET_EXP_PARAMS_I(&handle, 1);
    if (handle != 0 && handle != 1)
        SERROR(34);
    if (fileH[(int)handle].f == NULL)
        SERROR(35);

    GET_EXP_PARAMS_I(&offset, 1);
    fseek(fileH[(int)handle].f, offset, SEEK_SET);
}

static struct basic theBasicParser = {
    .Prog                     = theProg,
    .ProgHead                 = theProg,
    .ProgLen                  = 0,
    .Token                    = theToken,
    .TokenType                = 0,
    .Tok                      = 0,
    .Table                    = (struct _commands *)theCommandTable,
    .get_token                = get_token,
    .init                     = init,
    .load_program_from_memory = load_program_from_memory,
    .load_program_from_file   = load_program_from_file,
    .put_back                 = put_back,
    .scan_labels              = scan_labels,
    .exec_if                  = exec_if,
    .exec_for                 = exec_for,
    .exec_next                = exec_next,
    .exec_goto                = exec_goto,
    .exec_gosub               = exec_gosub,
    .exec_return              = exec_return,
    .exec_rem                 = exec_rem,
    .exec_open                = exec_open,
    .exec_read                = exec_read,
    .exec_seek                = exec_seek,
    .find_eol                 = find_eol,
    .var_assignment           = var_assignment,
    .str_assignment           = str_assignment,
    .get_exp                  = get_exp,
    .get_str                  = get_str,
};

struct basic *BasicParser = &theBasicParser;
