#include <cairo.h>
#include "tspl_api.h"
#include "tspl_parser.h"
#include "tspl_config.h"
#include "tspl_label_func.h"
#include "tspl_PageBuffer.h"

tspl_sdk_event_t sdk_event = {};
tspl_event_data_t sdk_data = {};

int tspl_sdk_init(tspl_sdk_event_callback cb, uint32_t dots_per_line)
{
    int ret = 0;
    labelPrinter->event_callback = cb;
    sdk_event.id = TSPL_EVENT_MAX;
    sdk_event.data = &sdk_data;

    ret = pageBuffer->init(dots_per_line);
    pageBuffer->clear();
    return ret;
}

void tspl_sdk_destroy(void)
{
    pageBuffer->kill();
}

void tspl_swap_int32(uint32_t *word, uint32_t len)
{
    uint32_t i;
    uint8_t *pbyte;
    static const uint8_t table[256] = {
        0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90,
        0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0, 0x08, 0x88, 0x48, 0xc8,
        0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8,
        0x78, 0xf8, 0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
        0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4, 0x0c, 0x8c,
        0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc,
        0x3c, 0xbc, 0x7c, 0xfc, 0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2,
        0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
        0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a,
        0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa, 0x06, 0x86, 0x46, 0xc6,
        0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6,
        0x76, 0xf6, 0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
        0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 0x01, 0x81,
        0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1,
        0x31, 0xb1, 0x71, 0xf1, 0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9,
        0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
        0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95,
        0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 0x0d, 0x8d, 0x4d, 0xcd,
        0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd,
        0x7d, 0xfd, 0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
        0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3, 0x0b, 0x8b,
        0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb,
        0x3b, 0xbb, 0x7b, 0xfb, 0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7,
        0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
        0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f,
        0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
    };


    for (i = 0; i < len; i++) {
        word[i] = ((word[i] & 0x000000ff) << 24) +
                  ((word[i] & 0x0000ff00) << 8) +
                  ((word[i] & 0x00ff0000) >> 8) +
                  ((word[i] & 0xff000000) >> 24);
    }
    for (i = 0; i < len; i++) {
        pbyte = (uint8_t *)&word[i];
        pbyte[0] = table[pbyte[0]];
        pbyte[1] = table[pbyte[1]];
        pbyte[2] = table[pbyte[2]];
        pbyte[3] = table[pbyte[3]];
    } 
}

#if 0
#define TEST_SCRIPT_LEN 8192

char script_buf[][TEST_SCRIPT_LEN] = {
    {

    // "PRINTF \"hello, iam printf\"\n"
#if 0
    "SIZE 48 MM, 20.55  MM\n"
    "GAP 26mm,12mm\n"
    "BLINE 25mm,24mm\n"
    "OFFSET 25.4mm\n"
    "SPEED 2\n"
    "DENSITY 15\n"
    "DIRECTION 1\n"
    "FEED 100\n"
    "FORMFEED\n"
    "HOME\n"
    "CUT\n"
    "LIMITFEED 10mm\n"
    "REFERENCE 10,10\n"
    "BOX 0,0,200,200,10\n"
    "PRINT 2\n"
    "REFERENCE 0,0\n"
    "BOX 0,0,200,200,10\n"
    "PRINT 1,1\n"
#else
    // "BAR 300,300,310,600"
    // "BOX 0,0,300,300,20\n"
    // "ERASE 0,0,150,150\n"
    // "BAR 0,10,200,10\n"
    // "PDF417 300,300,240,50,0,P0,E1,U100,400,10,M1,\"abcdef112345dsadasadsadsa\"\n"
    // "PDF417 300,300,240,50,90,P0,E1,U100,400,10,M1,\"abcdef\"\n"
    // "PDF417 300,300,240,50,180,P0,E1,U100,400,10,M1,\"abcdef112345dsadasadsaddsadsasa\"\n"
    // "PDF417 300,300,240,50,270,P0,E1,U100,400,10,M1,\"abcdef\"\n"
    // // "REVERSE 0,0,500,800\n"
    // "BARCODE 200,200,\"EAN13\",30,1,90,2,2,\"000012345678\"\n"
    // "BARCODE 0,0,\"25\",30,1,0,2,2,\"123456789999\"\n"
    // "BARCODE 0,0,\"93\",30,1,0,2,2,\"12345678\"\n"
    // "BARCODE 0,0,\"UPCA\",30,1,0,2,2,\"00012345678\"\n"
    // "BARCODE 0,0,\"EAN8\",30,1,0,2,2,\"1234567\"\n"
    // "BARCODE 0,0,\"EAN13\",30,1,0,2,2,\"12\"\n"
    // "BARCODE 0,0,\"POST\",30,1,0,2,2,\"2000\"\n"
    // "BARCODE 0,0,\"CODA\",30,1,0,2,2,\"A12345678A\"\n"
    // "BARCODE 0,0,\"UPCE\",30,1,0,2,2,\"234567\"\n"
    // "BARCODE 0,0,\"CODA\",30,1,0,2,2,\"A12345678A\"\n"
    // "BARCODE 300,200,\"39\",30,1,180,2,2,\"12345678\"\n"
    // "TEXT 0,0,\"3\",0,1,1,\"年圣诞节啊hyi哦的撒撒谎京东i撒谎覅哦啊睡哦发\"\n"
    // "TEXT 0,400,\"3\",0,1,1,\"年圣诞节啊hyi哦的撒撒谎京东i撒谎覅哦啊睡哦发\"\n"
    // "TEXT 0,600,\"3\",0,1,1,\"年圣诞节啊hyi哦的撒撒谎京东i撒谎覅哦啊睡哦发\"\n"
    // "BARCODE 300,10,\"128M\",100,1,90,2,2,\"!104!096ABCD!101EFGH\"\n"
    // "BARCODE 170,300,\"128M\",30,1,90,2,2,\"12345678\"\n"
    // "BARCODE 170,300,\"128M\",30,1,180,2,2,\"12345678\"\n"
    // "BARCODE 170,300,\"128M\",30,1,270,2,2,\"12345678\"\n"

    // "BOX 100,250,150,290,5"
    // "BAR 100,300,50,50"
    // "DMATRIX 100,50,200,200,\"7878778666\""
    // "REVERSE 0,0,500,200\n"
    "MAXICODE 0,100,\"300,840,06810,7317,12345678\""
    // "REVERSE 0,0,500,200\n"
    "PRINT 1,1\n"
#endif
    },
    {
        // "AA=65\n"
        // "WORD$=CHR$(AA)\n"
        // "WORDONE$=\"123hello world!!!\""
        // // "A$=W$+\"abcdefg iam\"\n"
        // "TEXT 0,0,\"3\",0,1,1,WORD$+WORDONE$\n"
        "OPEN \"test_open.txt\", 0\n"
        // "READ 0,ITEM$,PRICE,QTY\n"
        // "TEXT 0,25,\"3\",0,1,1,ITEM$+STR$(PRICE)+STR$(QTY)\n"
        // "TEXT 0,50,\"3\",0,1,1,\"LOF()=\"+STR$(LOF(\"test_open.txt\"))\n"
        "Z$=FREAD$(0,6)\n"
        "PRINTF Z$\n"
        "SEEK 0, 1\n"
        "Z$=FREAD$(0,600)\n"
        "PRINTF Z$\n"
        "PRINT 1\n"
        "END\n"
    }
};


typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t gap_betweenPaper;
    uint32_t gap_offset; // 什么意思？
    uint32_t bline_height;
    uint32_t bline_offset;//黑标偏移量（距离开始处的距离）
    uint32_t offset;
    uint32_t speed;
    uint32_t density;
    uint32_t direction;
    uint32_t feed;
    uint32_t limitfeed_dotline;
} PrintServer_t;
PrintServer_t printServer = {};
#endif

#if 0
static void tspl_sdk_event_handler(tspl_sdk_event_t *event)
{
    switch (event->id)
    {
    case TSPL_EVENT_SIZE:
        if (event->status) {
            printServer.width = event->data->two_param.m;
            printServer.height = event->data->two_param.n;
        }
        break;
    case TSPL_EVENT_GAP:
        if (event->status) {
            printServer.gap_betweenPaper = event->data->two_param.m; // 两张纸的间距
            printServer.gap_offset = event->data->two_param.n; // 不清楚这个参数是指什么
        }
        break;
    case TSPL_EVENT_BLINE:
        if (event->status) {
            printServer.bline_height = event->data->two_param.m;
            printServer.bline_offset = event->data->two_param.n;
        }
        break;
    case TSPL_EVENT_OFFSET:
        if (event->status) {
            printServer.offset = event->data->one_param.n;
        }
        break;
    case TSPL_EVENT_SPEED:
        if (event->status) {
            printServer.speed = event->data->one_param.n;
        }
        break;
    case TSPL_EVENT_DENSITY:
        if (event->status) {
            printServer.density = event->data->one_param.n;
        }
        break;
    case TSPL_EVENT_DIRECTION:
        if (event->status) {
            printServer.direction = event->data->one_param.n;
        }
        break;
    case TSPL_EVENT_FEED:
        if (event->status) {
            printServer.feed = event->data->one_param.n;
        }
        break;
    case TSPL_EVENT_FORMFEED:
        if (event->status) {
        }
        break;
    case TSPL_EVENT_HOME:
        if (event->status) {
        }
        break;
    case TSPL_EVENT_PRINT:
        if (event->status) {
            uint32_t *bitmap = event->data->zero_param.BitMap.bitmap;
            uint32_t height = event->data->zero_param.BitMap.validHeight;
            int stride = cairo_format_stride_for_width(CAIRO_FORMAT_A1, 384);
            cairo_surface_t *surface= 
                cairo_image_surface_create_for_data((uint8_t *)bitmap, CAIRO_FORMAT_A1, 384, height, stride);
            cairo_surface_write_to_png(surface, PNGDirName"test.png");
            cairo_surface_destroy(surface);
        }
        break;
    case TSPL_EVENT_CUT:
        if (event->status) {
        }
        break;
    case TSPL_EVENT_LIMITFEED:
        if (event->status) {
            printServer.limitfeed_dotline = event->data->one_param.n * 8;
        }
        break;
#if 0
    case TSPL_EVENT_BAR:
        break;
    case TSPL_EVENT_BARCODE:
        break;
    case TSPL_EVENT_BITMAP:
        break;
    case TSPL_EVENT_BOX:
        break;
    case TSPL_EVENT_ERASE:
        break;
    case TSPL_EVENT_DMATRIX:
        break;
    case TSPL_EVENT_MAXICODE:
        break;
    case TSPL_EVENT_PDF417:
        break;
    case TSPL_EVENT_PUTPCX:
        break;
    case TSPL_EVENT_REVERSE:
        break;
    case TSPL_EVENT_TEXT:
        break;
#endif
    default:
        break;
    }
}
#endif

/* 使用示例 */
#if 0
int main(int argc, char *argv[])
{
    tspl_sdk_init(tspl_sdk_event_handler, 384);

    TSPLPARSER_MEM(script_buf[1], TEST_SCRIPT_LEN);

    tspl_sdk_destroy();
    return 0;
}
#endif