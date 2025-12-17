#ifndef __RUNTIME_DATA_H__
#define __RUNTIME_DATA_H__

#include <stdbool.h>
#include <stdint.h>
#include "Config.h"

#define CJK_WORDSET_GB18030		(0)
#define CJK_WORDSET_BIG5		(1)
#define CJK_WORDSET_SHIFT_JIS	(11)
#define CJK_WORDSET_JIS0208		(12)
#define CJK_WORDSET_KSC5601		(21)
#define CJK_WORDSET_NONE		(128)

#define PDF417_MAX_MODULES		(32768)	/* Rows * Columns */
#define QR_CODE_MAX_MODULES		(177)	/* Version 40 */
#define DATA_BUFFER_SIZE		(4096)

#define NV_GRAPHICS_DIR			"/data/PUB/nv_images/"
#define NV_GRAPHICS_MEM_CAP		(1048576) /* 1MB */

enum {
	STANDARD_MODE = 0,
	PAGE_MODE,
	USER_SETTING_MODE
};

enum {
	CHAR_TYPE_ASC = 0,
	CHAR_TYPE_CJK,
	CHAR_TYPE_MAX
};

typedef struct {
	uint16_t LeftMargin;
	uint16_t PrintAreaWidth;
	uint16_t FixedLeftMargin;
	uint16_t FixedPrintAreaWidth;
	uint16_t PrintHorizontalAccuracy;
	uint16_t PrintVerticalAccuracy;
	uint16_t LineSpacing[2]; /* [0/1]: standard/page mode */

	uint16_t TabPos[32];

	uint8_t LeftCharSpacing[CHAR_TYPE_MAX];
	uint8_t RightCharSpacing[2][CHAR_TYPE_MAX]; /* [0/1]: standard/page mode */
	uint8_t CharHSize[CHAR_TYPE_MAX];
	uint8_t CharVSize[CHAR_TYPE_MAX];
	uint8_t UnderlineMode[CHAR_TYPE_MAX];
	uint8_t EmphasizedMode[CHAR_TYPE_MAX];
	uint8_t UserDefEnabled[CHAR_TYPE_MAX];

	uint8_t InternationalCharSet;
	uint8_t CJKMode;
	uint8_t CodePage;
	uint8_t ASC_WordSet;
	uint8_t CJK_WordSet;
	uint8_t Utf8_WordSet;
	uint8_t Locale;

	uint32_t Mode                  : 3;
	uint32_t Alignment             : 2;
	uint32_t ClockwiseRotationMode : 1;
	uint32_t UpsideDownMode        : 1;
	uint32_t BlackWhiteReverseMode : 1;
	uint32_t RightToLeftMode       : 1;
	uint32_t TSPLMode              : 1;
	uint32_t Color                 : 2;

	struct {
		int sa;
		int sb;
		int sc;
		int sd;
		int se;
		int sf;
		int sg;
		int sh;
	} PaperLayout;

	int FeedAndCutOnCoverClosed;
	uint8_t BlackMarkLocation;

	int FeedAndCutOnElectrify;//增加上电是否需要先走纸后切刀
	int CuttingAutoLogo; //设置切刀前后追加logo
	/*
	 * CutOption:
	 * 0: Normal
	 * 1: Partial cut always
	 * 2: Full cut always
	 * 3: Never cut
	 */
	uint32_t CutOption : 2;
	/*
	 * BitsPerDot:
	 * 0: Print in mono mode
	 * 1: Print in 2^2 grayscale mode
	 * ...
	 * 7: Print in 2^8 grayscale mode
	 */
	uint32_t BitsPerDot : 3;
	uint8_t PaperSizeAdaptive;

	struct {
		uint8_t Enabled;
		uint16_t StdPrtEng;
		uint16_t BlackRatio;
	} DualColor;

	struct {
		/*
		 *  3  2               1
		 *  1  8               5      8 7      0
		 * +----+--+----------+--------+--------+
		 * |type|al| dots back|kc1 or n|kc2 or m|
		 * +----+--+----------+--------+--------+
		 *      type: 0-disable; 1-mono; 2-grayscale
		 * alignment: 0-left; 1-center; 2-right
		 * dots back: dots to feed backwards before printing
		 *   kc1/kc2: key code of NV mono image
		 *         n: index of NV grayscale image
		 *         m: depth of NV grayscale image
		 */
		struct {
			uint32_t kc2_m     : 8;
			uint32_t kc1_n     : 8;
			uint32_t dots_back : 10;
			uint32_t alignment : 2;
			uint32_t type      : 4;
		} Header;

		/*
		 *  3  2               1
		 *  1  8               5      8 7      0
		 * +----+--+----------+--------+--------+
		 * |type|al|   spare  |kc1 or n|kc2 or m|
		 * +----+--+----------+--------+--------+
		 *      type: 0-disable; 1-mono; 2-grayscale
		 * alignment: 0-left; 1-center; 2-right
		 *   kc1/kc2: key code of NV mono image
		 *         n: index of NV grayscale image
		 *         m: depth of NV grayscale image
		 */
		struct {
			uint32_t kc2_m     : 8;
			uint32_t kc1_n     : 8;
			uint32_t spare     : 10;
			uint32_t alignment : 2;
			uint32_t type      : 4;
		} Footer;
	} HeaderFooterImage;

	uint16_t (*leftMargin)(void);
	uint16_t (*printAreaWidth)(void);
	uint16_t (*rightMargin)(void);
	uint16_t (*lineSpacing)(void);
	uint8_t  (*rightCharSpacing)(int char_type);
	bool (*isClockwiseRotation)(void);
	bool (*isUpsideDown)(void);
} Settings_t;

typedef struct {
	uint8_t code_page;
	uint8_t asc_wordset;
	uint8_t cjk_wordset;
	uint8_t utf8_wordset;
	uint8_t locale;
	uint32_t fine_mode;
} print_log_config_t;

typedef struct {
	uint8_t ModuleSize;
	uint8_t Height;
	uint8_t HRIPos;
	uint8_t HRIFont;
	uint16_t SymbolWidth;
	struct {
		uint32_t Body[PAGE_BUFFER_MAX_HEIGHT >> 5];
		uint32_t Ext[PAGE_BUFFER_MAX_HEIGHT >> 5];
		uint32_t HRI[(PAGE_BUFFER_MAX_HEIGHT >> 5) * MAX_CHAR_HEIGHT];
	} Bitmap;
} Barcode_t;

typedef struct {
	uint8_t ModuleSize;
	uint8_t Columns;
	uint8_t Rows;
	uint8_t RowHeight;
	uint8_t ECLevel;
	uint8_t Options;
	uint16_t SymbolRows;
	uint16_t SymbolWidth;
	struct {
		uint8_t Modules[PDF417_MAX_MODULES / 8];
	} Bitmap;
} PDF417_t;

typedef struct {
	uint8_t ModuleSize;
	uint8_t Model;
	uint8_t ECLevel;
	uint16_t SymbolSize;
	struct {
		uint8_t Modules[QR_CODE_MAX_MODULES][(QR_CODE_MAX_MODULES - 1) / 8 + 1];
	} Bitmap;
} QRCode_t;

/*
 * Runtime Flags
 */
#define RTF_KERN_BUF_FULL		(1 << 0)

#define RUNTIME_FLAG_SET(f)		(Runtime_Flag |=  (f))
#define RUNTIME_FLAG_CLEAR(f)	(Runtime_Flag &= ~(f))
#define RUNTIME_FLAG_IS_SET(f)	(Runtime_Flag &   (f))

/*
 * NV Graphics Data Formats
 */
enum {
	NV_GRAPHICS_DATA_RASTER_FORMAT = 0,
	NV_GRAPHICS_DATA_COLUMN_FORMAT
};

typedef struct {
	uint8_t kc1;
	uint8_t kc2;
	uint32_t file_size;
} nv_graphics_file_t;

/*
 * Downloaded Bit Image
 */
typedef struct {
	uint8_t x; /* bytes in horizontal direction */
	uint8_t y; /* bytes in vertical direction */
	uint8_t data[BYTES_PER_LINE_MAX * 255 * 8];
} downloaded_bit_image_t;

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
void load_uint32(const char *key, uint32_t *pv, uint32_t def);
void load_int(const char *key, int *pv, int def);

extern void DefineNVGraphicsData_Init(uint8_t kc1, uint8_t kc2, uint16_t width, uint16_t height, int format);
extern void DefineNVGraphicsData_Append(uint8_t *data, uint32_t len);
extern int  GetNVGraphicsFileInfo(nv_graphics_file_t **files, uint32_t *count);
extern void SavePrintTaskBitmap_SetEnabledState(bool enabled);
extern void SavePrintTaskBitmap_Begin(void);
extern void SavePrintTaskBitmap_Append(const void *data, uint32_t len);
extern void SavePrintTaskBitmap_End(void);
extern int PrintHeaderLogo(unsigned char full_cut);
extern int PrintFooterLogo(void);
extern void RuntimeDataInit(void);
extern void PersistentDataInit(void);
extern void feed_one_line(void);

/*----------------------------------------------*
 | GLOBAL VARIABLES                             |
 *----------------------------------------------*/
extern const uint8_t CODE_PAGE_MAPPINGS[64];
extern uint8_t DataBuffer[DATA_BUFFER_SIZE];
extern Settings_t Settings;
extern print_log_config_t print_log_config;
extern Barcode_t Barcode;
extern PDF417_t PDF417;
extern QRCode_t QRCode;
extern downloaded_bit_image_t DownloadedBitImage;
extern uint8_t MemorySwitches[8];
extern volatile uint32_t Runtime_Flag;
extern unsigned char cutter_flag;
extern unsigned char print_chn_id_filter;
#endif // __RUNTIME_DATA_H__
