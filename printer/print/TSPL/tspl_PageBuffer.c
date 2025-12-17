#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zint.h>
#include <math.h>
#include "tspl_PageBuffer.h"
#include "tspl_LineBuffer.h"
#include "tspl_basic.h"
#include "tspl_config.h"
#include "tspl_util.h"
#include "tspl_HarfBuzz.h"

#define COORDINATE_POINT_ASSERT(x,y,x1,y1) do {                                     \
                                            if (x < 0 || y < 0 || x1 < 0 || y1 < 0) \
                                                SERROR(27);               \
                                            if (x1 < x || y1 < y)                   \
                                                SERROR(27);               \
} while (0)

PDF417_t PDF417;

static int init(uint32_t dots_per_line)
{
    pageBuffer->Data               = (uint32_t *)calloc(MAX_PAGE_HEIGHT * WORDS_PER_LINE_MAX, sizeof(uint32_t));
    if (!pageBuffer->Data)
        SERROR(29);
    pageBuffer->DebugMode          = 0;
    pageBuffer->EndPos             = 0;
    pageBuffer->Height             = 0;
    pageBuffer->ValidHeight        = 0;
    pageBuffer->ReferenceHeight    = 0;
    pageBuffer->TextLineIndexHeight= 0;
    pageBuffer->TextLeftMargin     = 0;
    pageBuffer->LineSpace          = 30;
    pageBuffer->HCharSize          = 1;
    pageBuffer->VCharSize          = 1;
    pageBuffer->DOTS_PER_LINE      = dots_per_line;
    pageBuffer->BYTES_PER_LINE     = dots_per_line >> 3;
    pageBuffer->WORDS_PER_LINE     = dots_per_line >> 5;
    pageBuffer->BYTES_PER_CHAR_ROW = pageBuffer->BYTES_PER_LINE * MAX_CHAR_HEIGHT;
    int _stride                    = cairo_format_stride_for_width(CAIRO_FORMAT_A1, dots_per_line);
    pageBuffer->surface            = cairo_image_surface_create_for_data((unsigned char *)pageBuffer->Data, CAIRO_FORMAT_A1, 
                                                                dots_per_line, MAX_PAGE_HEIGHT, _stride);
    pageBuffer->cr        = cairo_create(pageBuffer->surface);

    tspl_harfBuzz->init();
    tspl_lineBuffer->init();
    tspl_lineBuffer->clear();
                                                                
    return (pageBuffer->Data) ? 0 : -1;
}

static void clear(void)
{
    memset(pageBuffer->Data, 0, MAX_PAGE_HEIGHT * pageBuffer->WORDS_PER_LINE * sizeof(uint32_t));
    pageBuffer->Height      = 0;
    pageBuffer->ValidHeight = 0;
}

static void kill(void)
{
    cairo_destroy(pageBuffer->cr);
    cairo_surface_destroy(pageBuffer->surface);
    if (pageBuffer->Data)
        free(pageBuffer->Data);
}

static void dump(void)
{
    uint32_t v, h;
    uint32_t *data;
    char buf[1024] = {0};
    char *pos = buf;

    LogDbg("pageBuffer dump:");

    data = pageBuffer->Data;
    for (v = 0; v < MAX_PAGE_HEIGHT; v++) {
        pos += snprintf(pos, buf+sizeof(buf)-pos, "[%d]\t", v);
        for (h = 0; h < pageBuffer->WORDS_PER_LINE; h++) {
            pos += snprintf(pos, buf+sizeof(buf)-pos, "0x%08x ", *data++);
        }
        LogDbg("%s", buf);
        pos = buf;
    }
}

static uint32_t getHeight(void)
{
    return pageBuffer->Height;
}

static void setHeight(uint32_t h)
{
    if (pageBuffer->ReferenceHeight + h > MAX_PAGE_HEIGHT)
        pageBuffer->Height = MAX_PAGE_HEIGHT;
    else
        pageBuffer->Height = h + pageBuffer->ReferenceHeight;
    if (pageBuffer->ValidHeight < pageBuffer->Height)
        pageBuffer->ValidHeight = pageBuffer->Height;
}

/*
 *  set the pageBuffer height without consider how height now.
 */
static void setValidHeight(uint32_t h)
{
    if (h > MAX_PAGE_HEIGHT)
        pageBuffer->ValidHeight = MAX_PAGE_HEIGHT;
    else
        pageBuffer->ValidHeight = h;
}

uint32_t *dotlineAt(uint32_t row)
{
    return pageBuffer->Data + row * pageBuffer->WORDS_PER_LINE;
}

static void print()
{
    sdk_event.id = TSPL_EVENT_PRINT;
    if (pageBuffer->ValidHeight == 0) {
        localDbg("no content to print, exit.");
        sdk_event.status = false;
        sdk_event.data->zero_param.BitMap.validHeight = 0;
        sdk_event.data->zero_param.BitMap.bitmap = NULL;
        goto _exit;
    }

    sdk_event.status = true;
    sdk_event.data->zero_param.BitMap.bitmap = pageBuffer->Data;
    sdk_event.data->zero_param.BitMap.validHeight = pageBuffer->ValidHeight;
_exit:
    labelPrinter->event_callback(&sdk_event);
    // cairo_surface_write_to_png(pageBuffer->surface, "/home/dodo/mylearn_test/src/printer_instructionSet/TSPL/test.png");
    // dump();
}

static void DrawRect(int x, int y, int w, int h)
{
    cairo_rectangle(pageBuffer->cr, x, y, w, h);
    cairo_fill(pageBuffer->cr);
    setHeight(y + h);
}

static void DrawEmptyRect(int x, int y, int w, int h, int thickness)
{
    if (thickness < 0)
        SERROR(23);
    cairo_set_line_width(pageBuffer->cr, thickness);
    cairo_rectangle(pageBuffer->cr, x+thickness/2, y+thickness/2, w-thickness, h-thickness);
    cairo_stroke(pageBuffer->cr);
    setHeight(y+h);
}

//FIXME: test 用完删掉
static void invert_region(unsigned int *image_data, int stride,
                    int x, int y, int x1, int y1)
{
    unsigned int row, word, pos, dots, bitext, temp;
    unsigned int reverse_dotline[pageBuffer->DOTS_PER_LINE / 32];
    unsigned int *data;
    unsigned char start, end;

    if (x > pageBuffer->DOTS_PER_LINE)  x = pageBuffer->DOTS_PER_LINE;
    if (y > MAX_PAGE_HEIGHT)            y = MAX_PAGE_HEIGHT;
    if (x1 > pageBuffer->DOTS_PER_LINE) x1 = pageBuffer->DOTS_PER_LINE;
    if (y1 > MAX_PAGE_HEIGHT)           y1 = MAX_PAGE_HEIGHT;

    memset(reverse_dotline, 0, sizeof(reverse_dotline));
    start = x / 32;
    end = x1 / 32;
    for (row = y; row < y1; row++) {
        dots = x1 - x;
        pos = x;
        data = &image_data[row*stride];
        for (word = start; word <= end; word++) {
            reverse_dotline[word] = ~data[word];
        }
        while (dots >= 32) {
            bitext = reverse_dotline[pos / 32];
            temp = pos % 32;
            if (temp) {
                bitext <<= temp;
                Dotline_CopyU32_util(&image_data[row*stride], pos, bitext, 0xffffffff, false);
                dots -= 32 - temp;
                pos += 32 - temp;
            }
            else {
                Dotline_CopyU32_util(&image_data[row*stride], pos, bitext, 0xffffffff, false);
                dots -= 32;
                pos += 32;
            }
        }
        if (dots > 0) {
            bitext = reverse_dotline[pos / 32];
            Dotline_CopyU32_util(&image_data[row*stride], pos, bitext,
                            PointStretchWidth_littleEdian_util[dots], false);
        }
    }
}

static uint32_t Utf8_to_Unicode(uint8_t *bytes, uint32_t subsequent_bytes, uint32_t *explen_out)
{
    uint32_t i, unicode, explen;

    unicode = bytes[0];

    if ((bytes[0] & 0x80) == 0x00) {
        explen = 1;
        unicode &= 0x7F;
    }
    else if ((bytes[0] & 0xE0) == 0xC0) {
        explen = 2;
        unicode &= 0x1F;
    }
    else if ((bytes[0] & 0xF0) == 0xE0) {
        explen = 3;
        unicode &= 0x0F;
    }
    else if ((bytes[0] & 0xF8) == 0xF0) {
        explen = 4;
        unicode &= 0x07;
    }
    else
        return 0;

    *explen_out = explen;

    if (subsequent_bytes + 1 < explen)
        return explen;

    for (i = 1; i < explen; i++) {
        if ((bytes[i] & 0xC0) != 0x80)
            return 0;
        unicode <<= 6;
        unicode |= (bytes[i] & 0x3F);
    }

    return unicode;
}

static void DrawText(int x, int y, int rotation, int x_multipy, int y_multipy, int font_size, char *utf8)
{
    uint32_t explen = 0, i = 0;
    uint32_t unicode;

    CAIRO_ENTER_CRITICAL();

    pageBuffer->TextLeftMargin = x;
    cairo_translate(pageBuffer->cr, x, y); // x的位置通过对linebuffer左移右移来控制
    //TODO:rotation
    pageBuffer->HCharSize = x_multipy;
    pageBuffer->VCharSize = y_multipy;
    pageBuffer->setHeight(y);

    for (; *utf8 != 0x00; utf8 += explen, i++) {
        unicode = Utf8_to_Unicode((uint8_t *)utf8, 4, &explen);
        tspl_harfBuzz->appendUnicode(unicode);
    }
    tspl_harfBuzz->shape(1);
    tspl_lineBuffer->printLine();
    pageBuffer->setHeight(y + pageBuffer->TextLineIndexHeight);
    pageBuffer->TextLineIndexHeight = 0;
    pageBuffer->TextLeftMargin = 0;
    
    CAIRO_EXIT_CRITICAL();
}

static void DrawBitmap(int x, int y, int x1, int y1, const uint8_t *bitmap)
{
    uint32_t row, pos, dots;
    uint32_t bitext;

    COORDINATE_POINT_ASSERT(x, y, x1, y1);

    for (row = y; row < y1; row++) {
        dots = x1 - x;
        pos = x;
        while (dots >= 8) {
            bitext = *bitmap++;
            Dotline_CopyU32_CairoFormat(pageBuffer->dotlineAt(row), pos, bitext, 0x000000ff, true);
            dots -= 8;
            pos += 8;
        }
    }

    setHeight(y1);
}

static void EraseRegion(int x, int y, int x1, int y1)
{
    uint32_t row, pos, dots;
    uint32_t *data;

    COORDINATE_POINT_ASSERT(x, y, x1, y1);

    for (row = y; row < y1; row++) {
        dots = x1 - x;
        pos = x;
        data = pageBuffer->dotlineAt(row);

        while (dots >= 32) {
            Dotline_CopyU32_CairoFormat(data, pos, 0x00000000, 0xffffffff, false);
            dots -= 32;
            pos += 32;
        }
        if (dots > 0)
            Dotline_CopyU32_CairoFormat(data, pos, 0x00000000, PointStretchWidth_littleEdian_util[dots], false);
    }

    /* erase in valid region */
    if (getHeight() > y) {
        /* if the erase region cover all dotline, reduce the pageBuffer height*/
        if (x1 - x == pageBuffer->DOTS_PER_LINE) {
            setValidHeight(y);
            setHeight(y);
        }
    }
    else { 
        /* if erase out of valid region, do nothing */
    }
} 

static void ReverseRegion(int x, int y, int x1, int y1)
{
    uint32_t row, pos, dots, word_pos, bit_pos, mask;
    uint32_t *data;

    COORDINATE_POINT_ASSERT(x, y, x1, y1);

    for (row = y; row < y1; row++) {
        dots = x1 - x;
        pos = x;
        data = pageBuffer->dotlineAt(row);

        word_pos = pos / 32;
        bit_pos = pos % 32;
        mask = 0x00000001 << bit_pos;
        while (dots--) {
            if (data[word_pos] & mask)
                data[word_pos] &= ~mask; //对应位赋0
            else
                data[word_pos] |= mask; // 对应位置1

            mask <<= 1;
            if (mask == 0) {
                mask = 0x00000001;
                word_pos++;
            }
        }
    }
    
    setHeight(y1);
}

static void convert_bitmap2cairo_format(uint8_t *cairo_bitmap, int stride, 
                            const char *source_bitmap, int soce_w, int soce_h)
{
    unsigned int row,col;
    unsigned int index = 0;
    unsigned char mask = 0x01;
    /* 根据_stride制造符合cairo格式的矩阵 */
    for (row = 0; row < soce_h; row++) {
        for (col = 0; col < soce_w; col++) {
            if (*source_bitmap != 0) { /* 有效黑点 */
                cairo_bitmap[index] &= ~mask;
            }
            else {
                cairo_bitmap[index] |= mask;
            }
            
            mask <<= 1;
            if (mask == 0) {
                mask = 0x01;
                index++;
            }
            source_bitmap += 3;
        }
        while ((index % stride) != 0) /* 当前行要补全到与_stride字节数一致 */
            index++;
        mask = 0x01;
    }
}

static int DrawDmatrix(int x, int y, int width, int height, int xm, int row, int col, 
                        int option_num, char *expression)
{
    struct zint_symbol *symbol;

    CAIRO_ENTER_CRITICAL();

    if (x < 0 || y < 0) {
        CAIRO_EXIT_CRITICAL();
        return 1;
    }

    symbol = ZBarcode_Create();
    if (!symbol) {
        CAIRO_EXIT_CRITICAL();
        return 2;
    }
    // symbol->symbology = BARCODE_HIBC_DM;
    symbol->symbology = BARCODE_DATAMATRIX;
    
    int ret = ZBarcode_Encode_and_Buffer(symbol, (uint8_t *)expression, strlen(expression), 0);
    if (ret != 0) {
        LogError("zint error: %s", symbol->errtxt);
        ZBarcode_Delete(symbol);
        CAIRO_EXIT_CRITICAL();
        return 3;
    }

    int _stride = cairo_format_stride_for_width (CAIRO_FORMAT_A1, symbol->bitmap_width);
    int bitmap_w = symbol->bitmap_width;
    int bitmap_h = symbol->bitmap_height;
    int bitmap_size = _stride*symbol->bitmap_height;
    uint8_t bitmap_for_cairo[bitmap_size];
    memset(bitmap_for_cairo, 0, bitmap_size);
    localDbg("_stride = %d", _stride);

    convert_bitmap2cairo_format(bitmap_for_cairo, _stride, symbol->bitmap, bitmap_w, bitmap_h);
    
    cairo_surface_t *matrix_surface = 
        cairo_image_surface_create_for_data(bitmap_for_cairo, CAIRO_FORMAT_A1, bitmap_w, bitmap_h, _stride);
    cairo_pattern_t *matrix_pattern = 
        cairo_pattern_create_for_surface(matrix_surface);

    cairo_translate(pageBuffer->cr, x, y);
    double scale_x = (double)width/(double)bitmap_w;
    double scale_y = (double)height/(double)bitmap_h;
    localDbg("scale_x=%f, scale_y=%f", scale_x, scale_y);
    cairo_scale(pageBuffer->cr, scale_x, scale_x);
    cairo_set_source(pageBuffer->cr, matrix_pattern);
    cairo_paint(pageBuffer->cr);

    
    cairo_pattern_destroy(matrix_pattern);
    cairo_surface_destroy(matrix_surface);
    ZBarcode_Delete(symbol);
    CAIRO_EXIT_CRITICAL();

    setHeight(y+height);
    return 0;
}

static void get_primary(char *primary, char *source)
{
    char *comma_pos = NULL;
    char *p = primary;
    char *s = source;
    do {
        if (*s != ',' && *s != ' ') {
            *p++ = *s++;
        }
        else {
             /* 如果是逗号，则把当前位置记录下来 */
            if (*s == ',') {
                comma_pos = p;
            }
            s++;
        }
    } while (*s != '\0');

    if (!comma_pos)
        *primary = 0;
    else
        *comma_pos = 0;
}

static int DrawMaxicode(int x, int y, char *expression)
{
    struct zint_symbol *symbol;
    int ret = 0;
    enum parse_mode{
        SINGLE_MESSAGE,
        MULTIPY_PROPERTY
    };
    enum parse_mode expression_parse_mode;

    CAIRO_ENTER_CRITICAL();

    symbol = ZBarcode_Create();
    if (!symbol) {
        LogError("ZBarcode_Create fail!");
        CAIRO_EXIT_CRITICAL();
        return 1;
    }

    symbol->symbology = BARCODE_MAXICODE;

    switch (labelPrinter->maxicode_mode)
    {
    case MAXICODE_MODE_2:
    case MAXICODE_MODE_3:
        expression_parse_mode = MULTIPY_PROPERTY;
        break;
    case MAXICODE_MODE_4:
    case MAXICODE_MODE_5:
    case MAXICODE_MODE_6:
        expression_parse_mode = SINGLE_MESSAGE;
        break;
    default:
        break;
    }
    expression_parse_mode = expression_parse_mode;//unused
    // symbol->option_1 = 4;//若未设置默认就是mode 4
    get_primary(symbol->primary, expression);
    localDbg("symbol->primary=%s", symbol->primary);

    ret = ZBarcode_Encode_and_Buffer(symbol, (uint8_t *)expression, strlen(expression), 0);
    if (ret) {
        LogError("zint error: %s", symbol->errtxt);
        CAIRO_EXIT_CRITICAL();
        return 2;
    }

    localDbg("symbol->bitmap_width=%d, symbol->bitmap_height=%d, "
    "symbol->width=%d, symbol->height=%d",
    symbol->bitmap_width,symbol->bitmap_height,symbol->width,symbol->height);

    int _stride = cairo_format_stride_for_width (CAIRO_FORMAT_A1, symbol->bitmap_width);
    int bitmap_size = _stride*symbol->bitmap_height;
    uint8_t bitmap_for_cairo[bitmap_size];
    memset(bitmap_for_cairo, 0, bitmap_size);
    localDbg("_stride = %d, bitmap_size=%d", _stride, bitmap_size);

    convert_bitmap2cairo_format(bitmap_for_cairo, _stride, symbol->bitmap, symbol->bitmap_width, symbol->bitmap_height);

    cairo_surface_t *maxicode_surface = 
        cairo_image_surface_create_for_data(bitmap_for_cairo, CAIRO_FORMAT_A1, symbol->bitmap_width, symbol->bitmap_height, _stride);
    cairo_pattern_t *maxicode_pattern =
        cairo_pattern_create_for_surface(maxicode_surface);

    cairo_translate(pageBuffer->cr, x, y);
    cairo_set_source(pageBuffer->cr, maxicode_pattern);
    cairo_paint(pageBuffer->cr);

    cairo_pattern_destroy(maxicode_pattern);
    cairo_surface_destroy(maxicode_surface);
    setHeight(y+symbol->bitmap_height);
    CAIRO_EXIT_CRITICAL();

    return 0;
}

static uint32_t GenerateBarcode(const char *s_type, const char *data, uint32_t h, bool human_readable,
                                uint8_t **bitmap_for_cairo, uint32_t *bitmap_w, uint32_t *bitmap_h)
{
    struct zint_symbol *symbol;
    int type;
    uint32_t s_len;

    s_len = strlen(s_type);
    if (!strncmp("39", s_type, s_len))            type = BARCODE_CODE39;
    // else if (!strncmp("39C", s_type, s_len))      type = BARCODE_EXCODE39; //BARCODE_EXCODE39 not support, 39C means have check digits
    else if (!strncmp("128", s_type, s_len))      type = BARCODE_CODE128;
    else if (!strncmp("128M", s_type, s_len))     type = BARCODE_CODE128;
    else if (!strncmp("EAN13", s_type, s_len))    type = BARCODE_EANX;
    // else if (!strncmp("EAN13+2", s_type, s_len))       type = BARCODE_EANX_CHK;
    // else if (!strncmp("EAN13+5", s_type, s_len))       type = BARCODE_EANX_CHK;
    else if (!strncmp("25", s_type, s_len))       type = BARCODE_C25INTER;  // Interleaved 2 of 5
    // else if (!strncmp("25C", s_type, s_len))       type = BARCODE_C25INTER;
    else if (!strncmp("93", s_type, s_len))       type = BARCODE_CODE93;
    else if (!strncmp("UPCA", s_type, s_len))     type = BARCODE_UPCA;
    // else if (!strncmp("UPCA+2", s_type, s_len))     type = BARCODE_UPCA_CHK;
    // else if (!strncmp("UPCA+5", s_type, s_len))     type = BARCODE_UPCA_CC; 
    else if (!strncmp("EAN8", s_type, s_len))     type = BARCODE_EANX;
    // else if (!strncmp("EAN8+2", s_type, s_len))     type = BARCODE_EANX_CHK;
    // else if (!strncmp("EAN8+5", s_type, s_len))     type = BARCODE_EANX_CC; 
    else if (!strncmp("UPCE", s_type, s_len))     type = BARCODE_UPCE;
    // else if (!strncmp("UPCE+2", s_type, s_len))     type = BARCODE_UPCE_CHK;
    // else if (!strncmp("UPCE+5", s_type, s_len))     type = BARCODE_UPCE_CHK;
    else if (!strncmp("CODA", s_type, s_len))     type = BARCODE_CODABAR;   // Codabar
    else if (!strncmp("EAN128", s_type, s_len))   type = BARCODE_EAN14;
    else if (!strncmp("POST", s_type, s_len))     type = BARCODE_POSTNET;
    // else if (!strncmp("China Post Code", s_type, s_len))       type = BARCODE_C25INTER;
    else return 0;

    symbol = ZBarcode_Create();
    if (symbol == NULL)
        return 0;

    symbol->symbology = type;
    symbol->height = (h/3) ? h/3 : 1;
    if (!human_readable)
        symbol->show_hrt = 0;

    int ret = ZBarcode_Encode_and_Buffer(symbol, (uint8_t *)data, strlen(data), 0); //WARN_INVALID_OPTION
    if (ret != 0) {
        LogError("zint error: %s", symbol->errtxt);
        ZBarcode_Delete(symbol);
        return 0;
    }
    localDbg("symbol->bitmap_width=%d, symbol->bitmap_height=%d, "
        "symbol->width=%d, symbol->height=%d",
        symbol->bitmap_width,symbol->bitmap_height,symbol->width,symbol->height);

    int _stride = cairo_format_stride_for_width (CAIRO_FORMAT_A1, symbol->bitmap_width);
    *bitmap_w = symbol->bitmap_width;
    *bitmap_h = symbol->bitmap_height;
    int bitmap_size = _stride*symbol->bitmap_height;
    *bitmap_for_cairo = (uint8_t *)malloc(bitmap_size);
    memset(*bitmap_for_cairo, 0, bitmap_size);
    localDbg("_stride = %d", _stride);

    convert_bitmap2cairo_format(*bitmap_for_cairo, _stride, symbol->bitmap, *bitmap_w, *bitmap_h);
    
    ZBarcode_Delete(symbol);
    return bitmap_size;
}

static int DrawBarcode(int x, int y, const char *s_type, uint32_t h, bool human_readable, 
                        uint32_t rotation, uint8_t narrow, uint8_t wide, const char *data)
{
    uint8_t *bitmap_for_cairo = NULL;
    uint32_t bitmap_w, bitmap_h, bitmap_size;

    CAIRO_ENTER_CRITICAL();

    if (wide > 6)
        wide = 6;
    else if (wide < 1)
        wide = 1;

    // Barcode.ModuleSize = wide;

    if (x < 0 || y < 0) {
        CAIRO_EXIT_CRITICAL();
        return 1;
    }
    bitmap_size = GenerateBarcode(s_type, data, h, human_readable, &bitmap_for_cairo, &bitmap_w, &bitmap_h);
    if (!bitmap_size) {
        if (bitmap_for_cairo)
            free(bitmap_for_cairo);
        CAIRO_EXIT_CRITICAL();
        return 2;
    }
    // if (Barcode.SymbolWidth == 0)
    //     return 3;
    // if (x + Barcode.SymbolWidth > pageBuffer->DOTS_PER_LINE)
    //     return 4;
    if (y + h + MAX_CHAR_HEIGHT > MAX_PAGE_HEIGHT) {
        CAIRO_EXIT_CRITICAL();
        return 5;
    }

    // words = (Barcode.SymbolWidth - 1) / 32 + 1;
    // Barcode.Height = h;

    int _stride = cairo_format_stride_for_width (CAIRO_FORMAT_A1, bitmap_w);
    cairo_surface_t *bco_surface =
        cairo_image_surface_create_for_data(bitmap_for_cairo, CAIRO_FORMAT_A1,
                                            bitmap_w, bitmap_h, _stride);
    cairo_status_t surface_states = 
        cairo_surface_status(bco_surface);
    if (surface_states != 0) {
        localDbg("%s", cairo_status_to_string(surface_states));
        CAIRO_EXIT_CRITICAL();
        return 6;
    }
    cairo_pattern_t *bco_pattern = 
        cairo_pattern_create_for_surface(bco_surface);

    cairo_translate(pageBuffer->cr, x, y);

    switch (rotation) {
    case 0:
        cairo_set_source(pageBuffer->cr, bco_pattern);
        cairo_paint(pageBuffer->cr);
        setHeight(y + bitmap_h);
        break;
    case 90:
        cairo_rotate(pageBuffer->cr, M_PI_2);
        cairo_set_source(pageBuffer->cr, bco_pattern);
        cairo_paint(pageBuffer->cr);
        setHeight(y + bitmap_w);
        break;
    case 180:
        cairo_rotate(pageBuffer->cr, M_PI);
        cairo_set_source(pageBuffer->cr, bco_pattern);
        cairo_paint(pageBuffer->cr);
        setHeight(y);
        break;
    case 270:
        cairo_rotate(pageBuffer->cr, M_PI+M_PI_2);
        cairo_set_source(pageBuffer->cr, bco_pattern);
        cairo_paint(pageBuffer->cr);
        setHeight(y);
        break;
    default:
        break;
    }

    cairo_pattern_destroy(bco_pattern);
    cairo_surface_destroy(bco_surface);
    if (bitmap_for_cairo)
        free(bitmap_for_cairo);

    // invert_region(pageBuffer->Data, pageBuffer->DOTS_PER_LINE/32, x, y,
    //             pageBuffer->DOTS_PER_LINE, pageBuffer->ValidHeight);

    CAIRO_EXIT_CRITICAL();
    return 0;
}

static uint32_t PDF417Generate(const char *data, uint32_t len, uint8_t **bitmap_for_cairo, uint32_t *w, uint32_t *h)
{
	struct zint_symbol *symbol = NULL;

	if (len == 0)
        return 0;

	// if (data[0] != 48)
	// 	goto _exit;
	// data++;
	// len--;

	PDF417.SymbolRows = 0;
	PDF417.SymbolWidth = 0;

	symbol = ZBarcode_Create();
	if (symbol == NULL)
		return 0;

	symbol->symbology = (PDF417.Options == 0) ? BARCODE_PDF417 : BARCODE_PDF417TRUNC;

	symbol->option_1 = PDF417.ECLevel;
	if (PDF417.Columns == 0) {
		if (PDF417.Rows != 0)
			symbol->option_2 = (len - 1) / PDF417.Rows + 1;
		else
			symbol->option_2 = 0;
	}
	else
		symbol->option_2 = PDF417.Columns;

	int ret = ZBarcode_Encode_and_Buffer(symbol, (uint8_t *)data, len, 0);
    if (ret != 0) {
        localDbg("ZBarcode_Encode_and_Buffer failed! ret = %d", ret);
        ZBarcode_Delete(symbol);
        return 0;
    }
	if (symbol->rows == 0 || symbol->width == 0)
		return 0;
    localDbg("symbol->bitmap_width=%d, symbol->bitmap_height=%d, "
        "symbol->width=%d, symbol->height=%d"
        , symbol->bitmap_width,symbol->bitmap_height,symbol->width,symbol->height);

    int _stride = cairo_format_stride_for_width (CAIRO_FORMAT_A1, symbol->bitmap_width);
    *w = symbol->bitmap_width;
    *h = symbol->bitmap_height;
    int bitmap_size = _stride*symbol->bitmap_height;
    *bitmap_for_cairo = (uint8_t *)malloc(bitmap_size);
    memset(*bitmap_for_cairo, 0, bitmap_size);
    localDbg("_stride = %d\n", _stride);

    convert_bitmap2cairo_format(*bitmap_for_cairo, _stride, symbol->bitmap, *w, *h);

	if (symbol)
		ZBarcode_Delete(symbol);

    return bitmap_size;
}

static int DrawPDF417(int x, int y, int w, int h, uint32_t rotation, const PDF417_options *options, const char *expression)
{
    uint8_t *pdf417_bitmap = NULL;
    uint32_t bitmap_size, bitmap_w, bitmap_h;

    CAIRO_ENTER_CRITICAL();

    if (x < 0 || y < 0 || w < 0 || h < 0) {
        CAIRO_EXIT_CRITICAL();
        return 1;
    }
    PDF417.Options = options->t;
    PDF417.ECLevel = options->e;
    bitmap_size = PDF417Generate(expression, strlen(expression), &pdf417_bitmap, &bitmap_w, &bitmap_h);
    if (!bitmap_size) {
        if (pdf417_bitmap)
            free(pdf417_bitmap);
        CAIRO_EXIT_CRITICAL();
        return 2;
    }
    
    // if (PDF417.SymbolWidth == 0 || PDF417.SymbolRows == 0)
    //     return 2;
    // if (x + w > pageBuffer->DOTS_PER_LINE || y + h > MAX_PAGE_HEIGHT) {
    //     CAIRO_EXIT_CRITICAL();
    //     return 3;
    // }
    // if (PDF417.SymbolWidth * PDF417.ModuleSize > w || PDF417.SymbolRows * PDF417.RowHeight > h) {
    //     CAIRO_EXIT_CRITICAL();
    //     return 4;
    // }
    // if (rotate != 0 && rotate != 90 && rotate != 180 && rotate != 270)
    //     return 5;
    int rect_stride = cairo_format_stride_for_width(CAIRO_FORMAT_A1, w);
    uint8_t rectRegion_bitmap[rect_stride * h];
    memset(rectRegion_bitmap, 0, rect_stride * h);
    cairo_surface_t *rect_surface =
        cairo_image_surface_create_for_data(rectRegion_bitmap, CAIRO_FORMAT_A1, w, h, rect_stride);  
    cairo_t *rect_cr =
        cairo_create(rect_surface);

    int pdf_stride = cairo_format_stride_for_width(CAIRO_FORMAT_A1, bitmap_w);
    cairo_surface_t *pdf417_surface =
        cairo_image_surface_create_for_data(pdf417_bitmap, CAIRO_FORMAT_A1, bitmap_w, bitmap_h, pdf_stride);  
    cairo_pattern_t *pdf417_pattern =
        cairo_pattern_create_for_surface(pdf417_surface);
    
    if (options->m == 1) { /* 将二维码放在x,y,w,h的中间 */
        int left_corner_x = (w - bitmap_w) / 2;
        int left_corner_y = (h - bitmap_h) / 2;
        cairo_translate(rect_cr, left_corner_x, left_corner_y);
        cairo_set_source(rect_cr, pdf417_pattern);
        cairo_paint(rect_cr);
    }
    else { /* 将二维码放在x,y,w,h的左上角 */
        cairo_set_source(rect_cr, pdf417_pattern);
        cairo_paint(rect_cr);
    }
    // localDbg("get: %d, %d | w=%d,h=%d",
    //     cairo_image_surface_get_width(rect_surface),
    //     cairo_image_surface_get_height(rect_surface),
    //     w,h);
    // invert_region((uint32_t *)rectRegion_bitmap, rect_stride/4, 0, 0, w, h);

    cairo_pattern_destroy(pdf417_pattern);
    cairo_surface_destroy(pdf417_surface);
    if (pdf417_bitmap)
        free(pdf417_bitmap);

    cairo_pattern_t *rect_pattern =
        cairo_pattern_create_for_surface(rect_surface);

    cairo_translate(pageBuffer->cr, x, y);

    switch (rotation)
    {
    case 0:
        cairo_set_source(pageBuffer->cr, rect_pattern);
        cairo_paint(pageBuffer->cr);
        setHeight(y+h);
        break;
    case 90:
        cairo_rotate(pageBuffer->cr, M_PI_2);
        cairo_set_source(pageBuffer->cr, rect_pattern);
        cairo_paint(pageBuffer->cr);
        setHeight(y+w);
        break;
    case 180:
        cairo_rotate(pageBuffer->cr, M_PI);
        cairo_set_source(pageBuffer->cr, rect_pattern);
        cairo_paint(pageBuffer->cr);
        setHeight(y);
        break;
    case 270:
        cairo_rotate(pageBuffer->cr, M_PI+M_PI_2);
        cairo_set_source(pageBuffer->cr, rect_pattern);
        cairo_paint(pageBuffer->cr);
        setHeight(y);
        break;
    default:
        break;
    }
    
    cairo_destroy(rect_cr);
    cairo_pattern_destroy(rect_pattern);
    cairo_surface_destroy(rect_surface);

    CAIRO_EXIT_CRITICAL();
    return 0;
}

static PageBuffer thePageBuffer = {
    .init          = init,
    .clear         = clear,
    .kill          = kill,
    .print         = print,
    .getHeight     = getHeight,
    .setHeight     = setHeight,
    .setValidHeight= setValidHeight,
    .dotlineAt     = dotlineAt,
    .dump          = dump,
    .DrawRect      = DrawRect,
    .DrawEmptyRect = DrawEmptyRect,
    .DrawText      = DrawText,
    .DrawBitmap    = DrawBitmap,
    .EraseRegion   = EraseRegion,
    .ReverseRegion = ReverseRegion,
    .DrawDmatrix   = DrawDmatrix,
    .DrawMaxicode  = DrawMaxicode,
    .DrawBarcode   = DrawBarcode,
    .DrawPDF417    = DrawPDF417,
};

PageBuffer *pageBuffer = &thePageBuffer;