#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "tspl_basic.h"
#include "tspl_label_func.h"
#include "tspl_PageBuffer.h"
#include "tspl_config.h"
#include "tspl_util.h"

// #define PARAMS_RANGE_LIMIT(x, y, x1, y1)  ParamsRangeLimit(x, y, x1, y1, __func__)
/* 模板 */
#if 0
static exec_size(void)
{
    uint32_t token_type = 0;
    char last_delim = 0;

    do {
        BasicParser->get_token();

        token_type = BasicParser->get_token();/* 取分隔符 */
        last_delim = *BasicParser->Token;
    } while (*BasicParser->Token == ',');

    if (BasicParser->Tok == EOL_ID || BasicParser->Tok == FINISHED_ID) {
        if (last_delim == ',')
            SERROR(0);
    }
}
#endif

static void print_params(double *p, size_t len, const char *funcname)
{
    size_t i;
    char buf[1024] = {};
    char *pos = buf;

    pos += snprintf(pos, buf+sizeof(buf)-pos, "[%s] params: ", funcname);
    for (i = 0; i < len; i++)
        pos += snprintf(pos, buf+sizeof(buf)-pos, "%lf ", p[i]);
    LogDbg("%s", buf);
}

static void print_bytes(uint8_t *p, uint32_t len, const char *funcname)
{
    uint32_t i;
    char buf[1024] = {};
    char *pos = buf;


    pos += snprintf(pos, buf+sizeof(buf)-pos, "[%s] bytes: ", funcname);
    for (i = 0; i < len; i++) {
        if ((i + 1) % 10 == 0) {
            LogDbg("%s", buf);
            pos = buf;
        }
        pos += snprintf(pos, buf+sizeof(buf)-pos, "0x%02x ", p[i] & 0xff);
    }
    LogDbg("%s", buf);
}

static void print_str_param(char *s, const char *funcname)
{
    LogDbg("[%s] str_param: ", funcname);
    LogDbg("%s", s);
}

static void exec_printf(void)
{
    uint32_t token_type = 0;
    double   value      = 0;
    char     last_delim = 0;
    char     buf[1024]  = {};
    char     *pos       = buf;
    
    do {
        token_type = BasicParser->get_token();
        if (BasicParser->Tok == EOL_ID || BasicParser->Tok == FINISHED_ID)
            break;

        switch (token_type) {
        case QUOTE:
            pos += snprintf(pos, buf+sizeof(buf)-pos, "%s", BasicParser->Token);
            break;
        case NUMBER:
            pos += snprintf(pos, buf+sizeof(buf)-pos, "%d", atoi(BasicParser->Token));
            break;
        case VARIABLE:
            BasicParser->put_back();
            BasicParser->get_exp(&value);
            pos += snprintf(pos, buf+sizeof(buf)-pos, "%f", value);
            break;
        case STRVARIABLE: {
            char strbuf[128] = {};
            BasicParser->put_back();
            BasicParser->get_str(strbuf, sizeof(strbuf));
            pos += snprintf(pos, buf+sizeof(buf)-pos, "%s", strbuf);
            break;
        }
        default:
            SERROR(0);
            break;
        }

        token_type = BasicParser->get_token();/* 取分隔符 */
        last_delim = *BasicParser->Token;
    } while (*BasicParser->Token == ',');

    if (BasicParser->Tok == EOL_ID || BasicParser->Tok == FINISHED_ID) {
        if (last_delim == ',')
            SERROR(0);
    }
    else if (token_type == COMMAND) {
        /* 如果print后紧接着命令的话，例如是个else、print，要回退继续进命令解析 */
        BasicParser->put_back();
    }
    
    LogInfo("%s", buf);
}

void get_exp_params(double *params, uint32_t pnum, const char *funcname, bool unit_enable, bool int_enable) 
{
    uint32_t token_type = 0;
    uint8_t i = 0;
    char last_delim = 0;

    do {
        token_type = BasicParser->get_token();
        if (BasicParser->Tok == EOL_ID || BasicParser->Tok == FINISHED_ID)
            break;
        if (token_type != NUMBER && token_type != VARIABLE)
            SERROR(22);
        BasicParser->put_back();

        BasicParser->get_exp(&params[i]);
        i++;

        token_type = BasicParser->get_token();/* 取分隔符 */
        if (unit_enable) {
            if (token_type == UNIT) {
                token_type = BasicParser->get_token();
            }
            else {
                /* 没有单位，默认是英制单位 将英制转换为公制 1=2.54 */
                params[i - 1] = params[i - 1] * 2.54;
            }
        }
        else { /* 非预期取到单位 报错 */
            if (token_type == UNIT)
                SERROR(30);
        }
        last_delim = *BasicParser->Token;
    } while (*BasicParser->Token == ',' && i != pnum);

    if (*BasicParser->Token != ',') {
        BasicParser->put_back();
        if (last_delim == ',')
            SERROR(0);
    }
    if (i != pnum) {
        LogError("[%s][i=%d] param num is not correct! please check", __func__, i);
        SERROR(22);
    }
    /* 向下取整 */
    if (int_enable) {
        for (i = 0; i < pnum; i++) {
            params[i] = (double)(int)params[i];
        }
    }
    //FIXME: 正式时去掉
    // print_params(params, pnum, funcname);
}

IS_DELIMITER get_str_param(char *str_param, const uint32_t len, const char *funcname)
{
    uint32_t token_type = 0;
    IS_DELIMITER ret;
    token_type = BasicParser->get_token();
    if (token_type != QUOTE && token_type != STRVARIABLE && token_type != FUNCTION)
        SERROR(22);
    BasicParser->put_back();
    BasicParser->get_str(str_param, len);
    BasicParser->get_token();/* 取分隔符 */
    if (*BasicParser->Token != ',') {
        ret = NO_DELIMITER;
        BasicParser->put_back();
    }
    else {
        ret = HAS_DELIMITER;
    }
    //FIXME:用完记得删掉
    // print_str_param(str_param, funcname);
    return ret;
}

//FIXME: 具体向谁赋值在实际中考虑
void get_byte_param(uint8_t *byte_param, const uint32_t len, const char *funcname)
{
    uint32_t i = 0;
    uint8_t *p = byte_param;

    for (i = 0; i < len; i++) {
        if (*BasicParser->Prog == '\r' || *BasicParser->Prog == '\n')
            SERROR(23);
        *p++ = *BasicParser->Prog++;
    }
    //FIXME:用完记得删掉
    print_bytes(byte_param, len, funcname);
}

/* 获取可缺省参数 */
uint8_t get_option_params(double *p, uint32_t residue_len, const char *funcname)
{
    uint32_t token_type = 0;
    uint8_t get_option_num = 0;
    while(1) {
        token_type = BasicParser->get_token();
        if (token_type == QUOTE || token_type == STRVARIABLE)           break;
        if (token_type == FUNCTION && BasicParser->Tok == STR_ID)       break;
        if (token_type == DELIMITER && BasicParser->Tok == EOL_ID)      break;
        if (token_type == DELIMITER && BasicParser->Tok == FINISHED_ID) break;
        if (residue_len == 0)   SERROR(22);
        BasicParser->put_back();
        GET_EXP_PARAMS_I(p, 1);
        residue_len--; p++; get_option_num++;
    }
    BasicParser->put_back();
    return get_option_num;
}

/* ----------------------打印机指令 start------------------------- */
static void exec_size(void)
{
#define width params[0]
#define height params[1]
    double params[2] = {100, 100}; //TODO: 设置好默认值
    GET_EXP_PARAMS_UNIT(params, GET_ARRAY_LEN(params));

    sdk_event.id = TSPL_EVENT_SIZE;
    if (width < 0 || height < 0 || width > 10000 || height > 10000) {
        sdk_event.status = false;
        goto _exit;
    }
    sdk_event.status = true;
    sdk_event.data->two_param.m = width;
    sdk_event.data->two_param.n = height;
_exit:
    labelPrinter->event_callback(&sdk_event);
#undef width
#undef height
}

static void exec_gap(void)
{
#define gap_betweenPaper params[0]
#define gap_offset params[1]
    double params[2] = {0, 0};
    GET_EXP_PARAMS_UNIT(params, GET_ARRAY_LEN(params));

    sdk_event.id = TSPL_EVENT_GAP;
    //FIXME: 这里 gap_offset > 10000 是随便设的，手册上说不大于标签纸长度
    if (gap_betweenPaper < 0 || gap_offset < 0 || gap_betweenPaper > 25.4|| gap_offset > 10000) {
        sdk_event.status = false;
        goto _exit;
    }
    sdk_event.status = true;
    sdk_event.data->two_param.m = gap_betweenPaper;
    sdk_event.data->two_param.n = gap_offset;
_exit:
    labelPrinter->event_callback(&sdk_event);
#undef gap_betweenPaper
#undef gap_offset
}

static void exec_bline(void)
{
#define bline_height params[0]
#define bline_offset params[1]
    double params[2] = {0, 0};
    GET_EXP_PARAMS_UNIT(params, GET_ARRAY_LEN(params));

    sdk_event.id = TSPL_EVENT_BLINE;
    //FIXME: 这里 bline_offset > 10000 是随便设的，手册上说不大于标签纸长度
    if (bline_height < 0 || bline_offset < 0 || bline_height > 25.4|| bline_offset > 10000) {
        sdk_event.status = false;
        goto _exit;
    }
    sdk_event.status = true;
    sdk_event.data->two_param.m = bline_height;
    sdk_event.data->two_param.n = bline_offset;
_exit:
    labelPrinter->event_callback(&sdk_event);
#undef bline_height
#undef bline_offset
}

static void exec_offset(void)
{
#define offset params[0]
    double params[1] = {};
    GET_EXP_PARAMS_UNIT(params, GET_ARRAY_LEN(params));

    sdk_event.id = TSPL_EVENT_OFFSET;
    if (offset < 0 || offset > 25.4) {
        sdk_event.status = false;
        goto _exit;
    }
    sdk_event.status = true;
    sdk_event.data->one_param.n = offset;
_exit:
    labelPrinter->event_callback(&sdk_event);
#undef offset
}

static void exec_speed(void)
{
#define speed params[0]
    double params[1] = {};
    GET_EXP_PARAMS_I(params, GET_ARRAY_LEN(params));

    sdk_event.id = TSPL_EVENT_SPEED;
    // if (speed != 1.5 && speed != 2 && speed != 3 && speed != 4) {
    //     sdk_event.status = false;
    //     goto _exit;
    // }
    if (speed < 0 || speed > 65536) {
        sdk_event.status = false;
        goto _exit;
    }
    sdk_event.status = true;
    sdk_event.data->one_param.n = speed;
_exit:
    labelPrinter->event_callback(&sdk_event);
#undef speed
}

static void exec_density(void)
{
#define density params[0]
    double params[1] = {};
    GET_EXP_PARAMS_I(params, GET_ARRAY_LEN(params));

    sdk_event.id = TSPL_EVENT_DENSITY;
    if (density < 0 || density > 15) {
        sdk_event.status = false;
        goto _exit;
    }
    sdk_event.status = true;
    sdk_event.data->one_param.n = density;
_exit:
    labelPrinter->event_callback(&sdk_event);
#undef density
}

static void exec_direction(void)
{
#define direction params[0]
    double params[1] = {};
    GET_EXP_PARAMS_I(params, GET_ARRAY_LEN(params));

    sdk_event.id = TSPL_EVENT_DIRECTION;
    if (direction != 0 && direction != 1) {
        sdk_event.status = false;
        goto _exit;
    }
    sdk_event.status = true;
    sdk_event.data->one_param.n = direction;
_exit:
    labelPrinter->event_callback(&sdk_event);
#undef direction
}

static void exec_reference(void)
{
#define x params[0]
#define y params[1]
    double params[2] = {0, 0};
    double origin_x = 0, origin_y = 0;
    GET_EXP_PARAMS_I(params, GET_ARRAY_LEN(params));

    cairo_device_to_user(pageBuffer->cr, &origin_x, &origin_y);
    cairo_translate(pageBuffer->cr, origin_x, origin_y);// 回原点
    cairo_translate(pageBuffer->cr, x, y);
    pageBuffer->ReferenceHeight = y;
#undef x
#undef y
}

static void exec_country(void)
{
    double params[1] = {};

    GET_EXP_PARAMS(params, GET_ARRAY_LEN(params));

    //TODO:得到值后具体去设置,设置前做好限制
}

static void exec_codepage(void)
{
    double params[1] = {};
    GET_EXP_PARAMS_I(params, GET_ARRAY_LEN(params));

    //TODO:得到值后具体去设置,设置前做好限制
}

static void exec_cls(void)
{
    pageBuffer->clear();
}

static void exec_feed(void)
{
#define feed params[0]
    double params[1] = {};
    GET_EXP_PARAMS_I(params, GET_ARRAY_LEN(params));

    sdk_event.id = TSPL_EVENT_FEED;
    if (feed < 1 || feed > 65535) {
        sdk_event.status = false;
        goto _exit;
    }
    sdk_event.status = true;
    sdk_event.data->one_param.n = feed;
_exit:
    labelPrinter->event_callback(&sdk_event);
#undef feed
}

static void exec_formfeed(void)
{
    sdk_event.id = TSPL_EVENT_FORMFEED;
    sdk_event.status = true;
    labelPrinter->event_callback(&sdk_event);
}

static void exec_home(void)
{
    sdk_event.id = TSPL_EVENT_HOME;
    sdk_event.status = true;
    labelPrinter->event_callback(&sdk_event);
}

//TODO: 实现可缺省参数获取
static void exec_print(void)
{
    double params[2] = {1, 1};
    uint32_t copies = 1, i;
    GET_OPTION_PARAMS(params, 2);
    //TODO:得到值后具体去设置,设置前做好限制
    if (params[0] < 1 || params[0] > 65535)     SERROR(23);
    if (params[1] < 1 || params[1] > 65535)     SERROR(23);
    copies = (uint32_t)(params[0] * params[1]);
    for (i = 0; i < copies; i++)
        pageBuffer->print();
}

static void exec_sound(void)
{
    double params[2] = {};

    GET_EXP_PARAMS(params, GET_ARRAY_LEN(params));

    //TODO:得到值后具体去设置,设置前做好限制
}

static void exec_cut(void)
{
    sdk_event.id = TSPL_EVENT_CUT;
    sdk_event.status = true;
    labelPrinter->event_callback(&sdk_event);
}

static void exec_limitfeed(void)
{
#define limitfeed params[0]
    double params[1] = {};
    GET_EXP_PARAMS_UNIT(params, GET_ARRAY_LEN(params));

    sdk_event.id = TSPL_EVENT_LIMITFEED;
    if (limitfeed < 0 || limitfeed > 1000) { /* 不超过1000mm */
        sdk_event.status = false;
        goto _exit;
    }
    sdk_event.status = true;
    sdk_event.data->one_param.n = limitfeed;
_exit:
    labelPrinter->event_callback(&sdk_event);
#undef limitfeed
}
/* ------------------------打印机指令 end ------------------------------- */

/* ------------------------卷标内容涉及指令 start------------------------- */
static void exec_bar(void)
{
    double params[4] = {};

    GET_EXP_PARAMS_I(params, GET_ARRAY_LEN(params));
    pageBuffer->DrawRect(params[0], params[1], params[2], params[3]);
}

static void exec_barcode(void)
{
    double params[7] = {};
    char str_params[2][1024] = {};
    int ret;

    GET_EXP_PARAMS_I(params, 2);
    GET_STR_PARAM(str_params[0], sizeof(str_params[0]));
    GET_EXP_PARAMS_I(params + 2, 5);
    GET_STR_PARAM(str_params[1], sizeof(str_params[1]));

    if (params[0] > pageBuffer->DOTS_PER_LINE || params[1] > MAX_PAGE_HEIGHT)
        SERROR(23);
    ret = pageBuffer->DrawBarcode(params[0], params[1], str_params[0], params[2], params[3], params[4], params[5], params[6], str_params[1]);
    if (ret != 0) {
        LogError("DrawBarcode err! ret = %d", ret);
        SERROR(23);
    }
}

static void exec_bitmap(void)
{
    double params[5] = {};
    uint8_t *byte_param = NULL;
    uint32_t bitmap_len = 0;
    int x, y, x1, y1;

    GET_EXP_PARAMS_I(params, GET_ARRAY_LEN(params));
    bitmap_len = params[2] * params[3];
    x = params[0];
    y = params[1];
    x1 = params[0] + params[2] * 8;
    y1 = params[1] + params[3];
    if (x > pageBuffer->DOTS_PER_LINE || x1 > pageBuffer->DOTS_PER_LINE)        SERROR(23);
    if (y > MAX_PAGE_HEIGHT || y1 > MAX_PAGE_HEIGHT)    SERROR(23);

    byte_param = (uint8_t *)malloc(bitmap_len);/* width(byte) * height(dot) */
    GET_BYTE_PARAM(byte_param, bitmap_len);
    pageBuffer->DrawBitmap(x, y, x1, y1, byte_param);
    free(byte_param);
}

static void exec_box(void)
{
    double params[5] = {};

#define X params[0]
#define Y params[1]
#define W (params[2] - params[0])
#define H (params[3] - params[1])
#define thickness params[4]

    GET_EXP_PARAMS_I(params, GET_ARRAY_LEN(params));
    if (params[0] > pageBuffer->DOTS_PER_LINE)      SERROR(23);
    if (params[1] > MAX_PAGE_HEIGHT)    SERROR(23);
    if (params[2] > pageBuffer->DOTS_PER_LINE)      params[2] = pageBuffer->DOTS_PER_LINE;
    if (params[3] > MAX_PAGE_HEIGHT)    params[3] = MAX_PAGE_HEIGHT;

    pageBuffer->DrawEmptyRect(X, Y, W, H, thickness);
#undef X
#undef Y
#undef W
#undef H
#undef thickness
}

static void exec_erase(void)
{
    double params[4] = {};

    GET_EXP_PARAMS_I(params, GET_ARRAY_LEN(params));
    pageBuffer->EraseRegion(params[0], params[1], params[0] + params[2], params[1] + params[3]);
}

static void exec_dmatrix(void)
{
#define x params[0]
#define y params[1]
#define width params[2]
#define height params[3]
#define xm params[4]
#define row params[5]
#define col params[6]
    double params[7] = {};
    char str_param[128] = {};
    uint8_t option_num = 0;

    GET_EXP_PARAMS_I(params, 4);
    option_num = GET_OPTION_PARAMS(params + 4, 3);
    GET_STR_PARAM(str_param, sizeof(str_param));

    //TODO:得到值后具体去设置,设置前做好限制
    pageBuffer->DrawDmatrix(x, y, width, height, xm, row, col, option_num, str_param);

    option_num = option_num;
#undef x
#undef y
#undef width
#undef height
#undef xm
#undef row
#undef col
}

static void exec_maxicode(void)
{
#define x params[0]
#define y params[1]
    double params[2] = {};
    char str_param[1024] = {};
    GET_EXP_PARAMS_I(params, GET_ARRAY_LEN(params));
    GET_STR_PARAM(str_param, sizeof(str_param));
    // TODO:得到了字符串再具体去拆出字符串里的4个信息

    //TODO:得到值后具体去设置,设置前做好限制
    pageBuffer->DrawMaxicode(x, y, str_param);
#undef x
#undef y
}

/*
 * return: 这里的返回值由于顺序不同，不适用，对于exec_pdf417具体赋值时，直接全部赋一遍就好
 */
static uint8_t get_pdf417_option_params(PDF417_options *options)
{
    enum { P,E,M,U,W,H,R,C,T };
    uint32_t token_type = 0;
    double temp[2];
    uint8_t get_option_num = 0;
    // uint32_t temp = residue_len;
    while(1) {
        token_type = BasicParser->get_token();
        if (token_type == STRING) {
            switch (*BasicParser->Token) {
            case 'p': options->p = atoi(BasicParser->Token + 1); break;
            case 'e': options->e = atoi(BasicParser->Token + 1); break;
            case 'm': options->m = atoi(BasicParser->Token + 1); break;
            case 'u':
                options->u[0] = atoi(BasicParser->Token + 1);
                token_type = BasicParser->get_token();/* 取分隔符 */
                if (*BasicParser->Token != ',') SERROR(22);
                GET_EXP_PARAMS_I(temp, 2);
                options->u[1] = temp[0];
                options->u[2] = temp[1];
                BasicParser->put_back();
                break;
            case 'w': options->w = atoi(BasicParser->Token + 1); break;
            case 'h': options->h = atoi(BasicParser->Token + 1); break;
            case 'r': options->r = atoi(BasicParser->Token + 1); break;
            case 'c': options->c = atoi(BasicParser->Token + 1); break;
            case 't': options->t = atoi(BasicParser->Token + 1); break;
            default:
                break;
            }
        }
        token_type = BasicParser->get_token();/* 取分隔符 */
        if (*BasicParser->Token != ',') SERROR(22);
        token_type = BasicParser->get_token();
        if (token_type == QUOTE || token_type == STRVARIABLE)  break;
        if (token_type == FUNCTION && BasicParser->Tok == STR_ID) break;
        BasicParser->put_back();
        get_option_num++;
    }
    BasicParser->put_back();
    LogDbg("get_pdf417_option_params options:");
    LogDbg("options->p=%d", options->p);
    LogDbg("options->e=%d", options->e);
    LogDbg("options->m=%d", options->m);
    LogDbg("options->u=%d %d %d", options->u[0], options->u[1], options->u[2]);
    LogDbg("options->w=%d", options->w);
    LogDbg("options->h=%d", options->h);
    LogDbg("options->r=%d", options->r);
    LogDbg("options->c=%d", options->c);
    LogDbg("options->t=%d", options->t);
    return get_option_num;
}

static void exec_pdf417(void)
{
    double params[5] = {};
    PDF417_options options = {};
    char str_param[128] = {};
    int ret;

    GET_EXP_PARAMS_I(params, 5);
    get_pdf417_option_params(&options);
    GET_STR_PARAM(str_param, sizeof(str_param));
    //TODO:得到值后具体去设置,设置前做好限制

    ret = pageBuffer->DrawPDF417(params[0], params[1], params[2], params[3], params[4], &options, str_param);
    if (ret != 0) {
        LogError("pageBuffer->DrawPDF417 failed, ret = %d", ret);
        SERROR(23);
    }
}

static void exec_putpcx(void)
{
    double params[2] = {};
    char filename[128] = {};
    char filepath[128] = "/tmp/tspl/";
    uint32_t file_size;
    uint8_t *pcxmap;
    FILE *fp;

    GET_EXP_PARAMS_I(params, GET_ARRAY_LEN(params));
    GET_STR_PARAM(filename, sizeof(filename));
    //TODO:得到值后具体去设置,设置前做好限制
    strcat(filepath, filename);
    if (!(fp = fopen(filepath, "rb"))) {
        LogError("there is no pcx name %s, please check.", filename);
        return;
    }
    fseek(fp, 0L, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    pcxmap = (uint8_t *)malloc(file_size);
    fread(pcxmap, 1, file_size, fp);
    fclose(fp);
    //TODO:找资料，如何解析pcx图片
    // pageBuffer->DrawBitmap();

    free(pcxmap);
}

static void exec_reverse(void)
{
    double params[4] = {};

    GET_EXP_PARAMS_I(params, GET_ARRAY_LEN(params));

    //TODO:得到值后具体去设置,设置前做好限制
    pageBuffer->ReverseRegion(params[0], params[1], params[0] + params[2], params[1] + params[3]);
}

static void exec_text(void)
{
    double params[5] = {};
    char str_params[2][1024] = {};
#define X params[0]
#define Y params[1]
#define rotation params[2]
#define x_multipy params[3]
#define y_multipy params[4]
#define font str_params[0]
#define content str_params[1]

    GET_EXP_PARAMS_I(params, 2);
    GET_STR_PARAM(font, sizeof(font));
    GET_EXP_PARAMS_I(params + 2, 3);
    GET_STR_PARAM(content, sizeof(content));
    //TODO:得到值后具体去设置,设置前做好限制
    if (X < 0 || X >= pageBuffer->DOTS_PER_LINE)    SERROR(23);
    if (Y < 0 || Y >= MAX_PAGE_HEIGHT)  SERROR(23);
    // if (str_params)
    if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270)   SERROR(23);
    if (x_multipy < 1 || x_multipy > 8)    SERROR(23);
    if (y_multipy < 1 || y_multipy > 8)    SERROR(23);

    uint8_t font_size = 0;
    if      (!strcmp(font, "1")) font_size = 12;
    else if (!strcmp(font, "2")) font_size = 20;
    else if (!strcmp(font, "3")) font_size = 24;
    else if (!strcmp(font, "4")) font_size = 32;
    else if (!strcmp(font, "5")) font_size = 48;
    else {
        SERROR(23);
    }
    pageBuffer->DrawText(X, Y, rotation, x_multipy, y_multipy, font_size, content);
#undef X
#undef Y
#undef rotation
#undef x_multipy
#undef y_multipy
#undef font
#undef content
}
/* ------------------------卷标内容涉及指令 end--------------------------- */

/* ------------------------档案管理指令 start--------------------------- */
static void exec_download(void)
{
    char filepath[128] = "/tmp/tspl/";
    char filename[128] = {};
    double data_size = 0;
    uint8_t *byte_param, mode;
    FILE *fp;
    //TODO: 看看是否要区分文件类型是 BAS、FILE、PCX
    if (GET_STR_PARAM(filename, sizeof(filename)) == NO_DELIMITER)
        mode = 0;
    else
        mode = 1;

    switch (mode)
    {
    /* save script to tmp */
    case 0:
        /* Consume the '\n' */
        BasicParser->get_token();
        if (*BasicParser->Token != '\n')
            SERROR(22);
        /* save the script */
        mkdir(filepath, 0666);
        strcat(filepath, filename);
        fp = fopen(filepath, "w+");
        fwrite(BasicParser->Prog, 1, BasicParser->ProgLen - (BasicParser->Prog - BasicParser->ProgHead), fp);
        fclose(fp);
        /* avoid to execute the script */
        longjmp(e_buf,2);
        break;
    /* save datafile / PCXfile */
    case 1:
        GET_EXP_PARAMS_I(&data_size, 1);
        byte_param = (uint8_t *)malloc(data_size);
        GET_BYTE_PARAM(byte_param, data_size); //FIXME：考虑是否要加上档头
        mkdir(filepath, 0666);
        strcat(filepath, filename);
        fp = fopen(filepath, "w+");
        fwrite(byte_param, 1, data_size, fp);
        fclose(fp);
        free(byte_param);
        break;
    default:
        break;
    }
}
/* ------------------------档案管理指令 end--------------------------- */

static void exec_debug(void)
{
    double debug_mode;

    sdk_event.id = TSPL_EVENT_DEBUG;
    sdk_event.status = false;

    GET_EXP_PARAMS_I(&debug_mode, 1);
    if (debug_mode != 0 && debug_mode != 1)
        goto _exit;
    pageBuffer->DebugMode = debug_mode;
    if (debug_mode)
        localDbg("enter debug mode");
    else
        localDbg("exit debug mode");

    sdk_event.status = true;
    sdk_event.data->one_param.n = debug_mode;

_exit:
    labelPrinter->event_callback(&sdk_event);
}

static void exec_setlinespace(void)
{
    double linespace;

    GET_EXP_PARAMS_I(&linespace, 1);
    if (linespace < 0)
        return;
    pageBuffer->LineSpace = linespace;
}

static void exec_sleep(void)
{
    double sleeptime;

    GET_EXP_PARAMS_I(&sleeptime, 1);

    sdk_event.id = TSPL_EVENT_SLEEP;
    sdk_event.status = true;
    sdk_event.data->one_param.n = sleeptime;
    labelPrinter->event_callback(&sdk_event);
}

static void skip_bitmap(void)
{
    double params[5] = {};
    uint32_t bitmap_len = 0, i = 0;
    GET_EXP_PARAMS_I(params, GET_ARRAY_LEN(params));
    bitmap_len = params[2] * params[3];
    for (i = 0; i < bitmap_len; i++) {
        BasicParser->Prog++;
    }
    if (*BasicParser->Prog != '\r' && *BasicParser->Prog != '\n')
        SERROR(23);
    BasicParser->get_token();//跳过回车，到达下一行
}

static void skip_download(void)
{
    double data_size = 0;
    uint32_t i = 0;
    char str_param[128] = {};
    if (GET_STR_PARAM(str_param, sizeof(128)) == HAS_DELIMITER) {
        GET_EXP_PARAMS_I(&data_size, 1);
        for (i = 0; i < data_size; i++) {
            BasicParser->Prog++;
        }
        if (*BasicParser->Prog != '\r' && *BasicParser->Prog != '\n')
            SERROR(23);
    }
    BasicParser->get_token();//跳过回车，到达下一行
}

static void change_page_mode(void)
{
    double param = 0;
    GET_EXP_PARAMS_I(&param, 1);
    if (param != 0 || param != 1) SERROR(22);
    if (param == 1) {
        // Settings.TSPLMode = 1;
        LogInfo("pageBufferMode now");
    }
    else {
        // Settings.TSPLMode = 0;
        LogInfo("lineBufferMode now");
    }
}

static void exec_switch2esc(void)
{
    // Settings.TSPLMode = 0;
    LogInfo("TSPLMode = 0");
}

struct LabelPrinter theLabelPrinter = {
    .exec_printf      = exec_printf,
    .exec_size        = exec_size,
    .exec_gap         = exec_gap,
    .exec_bline       = exec_bline,
    .exec_offset      = exec_offset,
    .exec_speed       = exec_speed,
    .exec_density     = exec_density,
    .exec_direction   = exec_direction,
    .exec_reference   = exec_reference,
    .exec_country     = exec_country,
    .exec_codepage    = exec_codepage,
    .exec_cls         = exec_cls,
    .exec_feed        = exec_feed,
    .exec_formfeed    = exec_formfeed,
    .exec_home        = exec_home,
    .exec_print       = exec_print,
    .exec_sound       = exec_sound,
    .exec_cut         = exec_cut,
    .exec_limitfeed   = exec_limitfeed,
    .exec_switch2esc  = exec_switch2esc,
    .exec_bar         = exec_bar,
    .exec_barcode     = exec_barcode,
    .exec_bitmap      = exec_bitmap,
    .exec_box         = exec_box,
    .exec_erase       = exec_erase,
    .exec_dmatrix     = exec_dmatrix,
    .exec_maxicode    = exec_maxicode,
    .exec_pdf417      = exec_pdf417,
    .exec_putpcx      = exec_putpcx,
    .exec_reverse     = exec_reverse,
    .exec_text        = exec_text,
    .exec_download    = exec_download,
    .exec_debug       = exec_debug,
    .exec_setlinespace= exec_setlinespace,
    .exec_sleep       = exec_sleep,
    .skip_bitmap      = skip_bitmap,
    .skip_download    = skip_download,
    .change_page_mode = change_page_mode,
};

struct LabelPrinter *labelPrinter = &theLabelPrinter;