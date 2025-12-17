#include <sys/types.h>
#include <regex.h>
#include <dlfcn.h>
#include <hb.h>
#include <hb-ft.h>
#include "libcommon.h"
#include "LineBuffer.h"
#include "Runtime_Data.h"
#include "Util.h"
#include "HarfBuzz.h"

#define THIRD_PARTY_FONTS_CONF_FILE "/data/PUB/third_party/fonts/fonts.conf"

#define FONTS_CONF_FILE     "/data/PUB/fonts/fonts.conf"
#define FONTS_CONF_FILE_JP  "/data/PUB/fonts/fonts_jp.conf"
#define FONTS_SETTING_FILE  "/data/PUB/fonts/fonts_setting"

#define INSTANCE_MAX_FACES  3
#define MAX_CACHED_PIXELSIZE_TABLES 32

typedef struct _face_t face_t;
struct _face_t {
	face_t *next;
	char *filename;
	int faceindex;
	FT_Face face;
};

typedef struct _cached_pixelsize_table_t cached_pixelsize_table_t;
struct _cached_pixelsize_table_t {
	cached_pixelsize_table_t *prev;
	cached_pixelsize_table_t *next;
	uint16_t advance_x;
	uint16_t table[16];
};

typedef struct {
	cached_pixelsize_table_t *head;
	cached_pixelsize_table_t *tail;
	uint16_t count;
} cached_pixelsize_table_list_t;

typedef struct _hb_instance_t hb_instance_t;
struct _hb_instance_t {
	hb_instance_t *next;
	bool enabled;                       // 是否启用。
	uint8_t ct;                         // 字符类型。0: ASCII；1: CJK；2: 其他
	uint8_t px;                         // px与ad用于指定pixel size与advance_x的比例。
	uint8_t ad;
	char script[32];
	char language[4];
	char font[64];
	int faceindex;
	hb_segment_properties_t segprop;
	FT_Face faces[INSTANCE_MAX_FACES];
	FT_Face face;
	uint8_t face_preferred;
	uint8_t face_is_monospaced;
	uint16_t threshold;
	uint16_t charsize;                  // 通过setXXXCharSize设置的字符大小
	uint16_t pixelsize_w;               // 当前pixel size。
	uint16_t pixelsize_h;
	uint16_t advance_x;                 // 当前advance_x。
	uint16_t nominal_h;                 // 用于计算标称字符高度，避免lineBuffer整体高度不足。
	uint16_t *pixelsize_table;
	cached_pixelsize_table_list_t *table_list;
	uint32_t (*ranges)[2];
};

/* FreeType exported functions */
static FT_Error (*FT_Init_FreeType_func)(FT_Library *) = NULL;
static FT_Error (*FT_Open_Face_func)(FT_Library, const FT_Open_Args *, FT_Long, FT_Face *) = NULL;
static FT_Error (*FT_Set_Pixel_Sizes_func)(FT_Face, FT_UInt, FT_UInt) = NULL;
static FT_UInt  (*FT_Get_Char_Index_func)(FT_Face, FT_ULong) = NULL;
static FT_Error (*FT_Load_Glyph_func)(FT_Face, FT_UInt, FT_Int32) = NULL;
static FT_Error (*FT_Done_Face_func)(FT_Face) = NULL;
static FT_Error (*FT_Done_FreeType_func)(FT_Library) = NULL;
/* HarfBuzz exported functions */
static hb_buffer_t *(*hb_buffer_create_func)(void) = NULL;
static hb_bool_t (*hb_buffer_allocation_successful_func)(hb_buffer_t *) = NULL;
static hb_language_t (*hb_language_from_string_func)(const char *, int) = NULL;
static void (*hb_buffer_destroy_func)(hb_buffer_t *) = NULL;
static void (*hb_buffer_reset_func)(hb_buffer_t *) = NULL;
static void (*hb_buffer_set_segment_properties_func)(hb_buffer_t *, const hb_segment_properties_t *) = NULL;
static void (*hb_buffer_add_codepoints_func)(hb_buffer_t *, const hb_codepoint_t *, int, unsigned int, int) = NULL;
static hb_font_t *(*hb_ft_font_create_func)(FT_Face, hb_destroy_func_t) = NULL;
static void (*hb_font_destroy_func)(hb_font_t *) = NULL;
static void (*hb_shape_func)(hb_font_t *, hb_buffer_t *, const hb_feature_t *, unsigned int) = NULL;
static hb_glyph_info_t *(*hb_buffer_get_glyph_infos_func)(hb_buffer_t *, unsigned int *) = NULL;
static hb_glyph_position_t *(*hb_buffer_get_glyph_positions_func)(hb_buffer_t *, unsigned int *) = NULL;
static hb_direction_t (*hb_script_get_horizontal_direction_func)(hb_script_t) = NULL;

static FT_Library ft_library = NULL;
static face_t *third_party_face_head = NULL, *third_party_face_tail = NULL;
static face_t *face_head = NULL, *face_tail = NULL;
static hb_instance_t *instance_head = NULL, *instance_tail = NULL;
static hb_buffer_t *hb_buf = NULL;
static FILE *fonts_conf = NULL;

static const char *fonts_conf_file(void)
{
	switch (Settings.Locale) {
	case 1:
		if (access(FONTS_CONF_FILE_JP, F_OK) == 0)
			return FONTS_CONF_FILE_JP;
		break;
	default:
		break;
	}
	return FONTS_CONF_FILE;
}

static int init(void)
{
	struct stat stbuf;
	void *ft_handle = NULL, *hb_handle = NULL;
	char *error;
	FT_Error e;

	if (stat(fonts_conf_file(), &stbuf))
		return -1;

	memset(harfBuzz->Canvas, 0, sizeof(harfBuzz->Canvas));
	memset(&harfBuzz->CanvasBBox, 0, sizeof(harfBuzz->CanvasBBox));
	memset(harfBuzz->Unicodes, 0, sizeof(harfBuzz->Unicodes));
	harfBuzz->UnicodeLen = 0;
	harfBuzz->ActiveInst = NULL;
	harfBuzz->Initialized = false;

	ft_handle = dlopen("/usr/lib/libfreetype.so", RTLD_NOW);
	if (!ft_handle) {
		LogInfo("libfreetype.so not available.");
		goto _fail;
	}

	hb_handle = dlopen("/usr/lib/libharfbuzz.so", RTLD_NOW);
	if (!hb_handle) {
		LogInfo("libharfbuzz.so not available.");
		goto _fail;
	}

#define RESOLVE(handle__,name__) \
do { \
	dlerror(); \
	name__##_func = (typeof(name__##_func))dlsym(handle__, #name__); \
	if (name__##_func == NULL || (error = dlerror()) != NULL) { \
		LogError("Failed to resolve " #name__); \
		goto _fail; \
	} \
} while (0)

	RESOLVE(ft_handle, FT_Init_FreeType                  );
	RESOLVE(ft_handle, FT_Open_Face                      );
	RESOLVE(ft_handle, FT_Set_Pixel_Sizes                );
	RESOLVE(ft_handle, FT_Get_Char_Index                 );
	RESOLVE(ft_handle, FT_Load_Glyph                     );
	RESOLVE(ft_handle, FT_Done_Face                      );
	RESOLVE(ft_handle, FT_Done_FreeType                  );

	RESOLVE(hb_handle, hb_buffer_create                  );
	RESOLVE(hb_handle, hb_buffer_allocation_successful   );
	RESOLVE(hb_handle, hb_language_from_string           );
	RESOLVE(hb_handle, hb_buffer_destroy                 );
	RESOLVE(hb_handle, hb_buffer_reset                   );
	RESOLVE(hb_handle, hb_buffer_set_segment_properties  );
	RESOLVE(hb_handle, hb_buffer_add_codepoints          );
	RESOLVE(hb_handle, hb_ft_font_create                 );
	RESOLVE(hb_handle, hb_font_destroy                   );
	RESOLVE(hb_handle, hb_shape                          );
	RESOLVE(hb_handle, hb_buffer_get_glyph_infos         );
	RESOLVE(hb_handle, hb_buffer_get_glyph_positions     );
	RESOLVE(hb_handle, hb_script_get_horizontal_direction);

#undef RESOLVE

	e = (*FT_Init_FreeType_func)(&ft_library);
	if (e) {
		LogError("FT_Init_FreeType failed (error=%d).", e);
		goto _fail;
	}

	hb_buf = (*hb_buffer_create_func)();
	if (!(*hb_buffer_allocation_successful_func)(hb_buf)) {
		LogError("hb_buffer_create failed.");
		goto _fail;
	}

	harfBuzz->Initialized = true;

	harfBuzz->loadThirdPartyConf();

	return harfBuzz->loadConf();

_fail:
	if (hb_buf && !(*hb_buffer_allocation_successful_func)(hb_buf))
		(*hb_buffer_destroy_func)(hb_buf);
	if (hb_handle)
		dlclose(hb_handle);
	if (ft_handle)
		dlclose(ft_handle);
	return -1;
}

static void free_faces(face_t **phead, face_t **ptail)
{
	face_t *face = (*phead), *next;

	while (face) {
		next = face->next;
		(*FT_Done_Face_func)(face->face);
		if (face->filename)
			free(face->filename);
		free(face);
		face = next;
	}

	(*phead) = NULL;
	(*ptail) = NULL;
}

static void free_instances(void)
{
	hb_instance_t *inst = instance_head, *next;
	cached_pixelsize_table_t *tbl, *tbltmp;

	while (inst) {
		next = inst->next;
		if (inst->table_list) {
			tbl = inst->table_list->head;
			while (tbl) {
				tbltmp = tbl->next;
				free(tbl);
				tbl = tbltmp;
			}
			free(inst->table_list);
		}
		if (inst->ranges)
			free(inst->ranges);
		free(inst);
		inst = next;
	}

	instance_head = NULL;
	instance_tail = NULL;
}

static FT_Face open_third_party_face(const char *filename, int faceindex)
{
	FT_Open_Args openargs;
	FT_Face face;
	FT_Error e;
	struct stat stbuf;
	char pathname[128];
	face_t *f;

	snprintf(pathname, sizeof(pathname) - 1, "/data/PUB/third_party/fonts/%s", filename);
	if (stat(pathname, &stbuf))
		return NULL;
	if (stbuf.st_size == 0 || !S_ISREG(stbuf.st_mode))
		return NULL;

	memset(&openargs, 0, sizeof(openargs));
	openargs.flags = FT_OPEN_PATHNAME;
	openargs.pathname = pathname;
	e = (*FT_Open_Face_func)(ft_library, &openargs, faceindex, &face);
	if (e) {
		LogError("FT_Open_Face failed (pathname=\"%s\", error=%d).", pathname, e);
		return NULL;
	}

	f = (face_t *)malloc(sizeof(face_t));
	if (!f)
		goto _fail;
	f->next = NULL;
	f->filename = strdup(filename);
	if (!f->filename)
		goto _fail;
	f->faceindex = faceindex;
	f->face = face;

	if (third_party_face_tail)
		third_party_face_tail->next = f;
	else
		third_party_face_head = f;
	third_party_face_tail = f;

	LogInfo("%s (%d) loaded.", pathname, faceindex);
	return face;

_fail:
	if (f)
		free(f);
	(*FT_Done_Face_func)(face);
	return NULL;
}

static FT_Face get_face(const char *filename, int faceindex)
{
	FT_Open_Args openargs;
	FT_Face face;
	FT_Error e;
	struct stat stbuf;
	char pathname[128];
	face_t *f = face_head;

	while (f) {
		if (strcmp(filename, f->filename) == 0 && faceindex == f->faceindex)
			return f->face;
		f = f->next;
	}

	snprintf(pathname, sizeof(pathname) - 1, "/data/PUB/fonts/%s", filename);
	if (stat(pathname, &stbuf))
		return NULL;
	if (stbuf.st_size == 0 || !S_ISREG(stbuf.st_mode))
		return NULL;

	memset(&openargs, 0, sizeof(openargs));
	openargs.flags = FT_OPEN_PATHNAME;
	openargs.pathname = pathname;
	e = (*FT_Open_Face_func)(ft_library, &openargs, faceindex, &face);
	if (e) {
		LogError("FT_Open_Face failed (pathname=\"%s\", error=%d).", pathname, e);
		return NULL;
	}

	f = (face_t *)malloc(sizeof(face_t));
	if (!f)
		goto _fail;
	f->next = NULL;
	f->filename = strdup(filename);
	if (!f->filename)
		goto _fail;
	f->faceindex = faceindex;
	f->face = face;

	if (face_tail)
		face_tail->next = f;
	else
		face_head = f;
	face_tail = f;

	LogInfo("%s (%d) loaded.", pathname, faceindex);
	return face;

_fail:
	if (f)
		free(f);
	(*FT_Done_Face_func)(face);
	return NULL;
}

static char *str_mid(const char *str, char *substr, int so, int eo)
{
	int len;

	len = eo - so;
	if (so >= 0 && len > 0) {
		memcpy(substr, str + so, len);
		substr[len] = 0;
	}
	else
		substr[0] = 0;
	return substr;
}

static uint32_t determine_ascii_px_by_adv(hb_instance_t *inst, uint32_t adv)
{
	hb_buffer_t *buf;
	hb_font_t *font;
	hb_glyph_position_t *glyph_pos;
	hb_segment_properties_t segprop;
	uint32_t i, unicodes[4], glyph_count, x_advance;

	/* Create an HarfBuzz buffer */
	buf = (*hb_buffer_create_func)();
	if (!(*hb_buffer_allocation_successful_func)(buf)) {
		LogError("hb_buffer_create failed.");
		return adv;
	}

	segprop.direction = HB_DIRECTION_LTR;
	segprop.script = HB_SCRIPT_LATIN;
	segprop.language = (*hb_language_from_string_func)("la", -1);
	unicodes[0] = '0';
	unicodes[1] = '!';
	unicodes[2] = 'W';

	for (i = (adv << 1); i >= adv; i--) {
		(*hb_buffer_reset_func)(buf);
		(*hb_buffer_set_segment_properties_func)(buf, &segprop);
		(*hb_buffer_add_codepoints_func)(buf, (const hb_codepoint_t *)unicodes, 3, 0, 3);

		(*FT_Set_Pixel_Sizes_func)(inst->face, i, i);
		font = (*hb_ft_font_create_func)(inst->face, NULL);
		(*hb_shape_func)(font, buf, NULL, 0);
		(*hb_font_destroy_func)(font);

		glyph_pos = (*hb_buffer_get_glyph_positions_func)(buf, &glyph_count);
		inst->face_is_monospaced = ((glyph_pos[1].x_advance >> 6) == (glyph_pos[2].x_advance >> 6)) ? 1 : 0;
		x_advance = glyph_pos[0].x_advance >> 6;
		if (x_advance <= adv)
			break;
	}

	(*hb_buffer_destroy_func)(buf);
	return i;
}

static uint32_t round_div(uint32_t dividend, uint32_t divisor)
{
	uint32_t q, r;

	q = dividend / divisor;
	r = dividend % divisor;
	if (r * 10 / divisor > 4)
		q++;
	return q;
}

static cached_pixelsize_table_t *get_cached_pixelsize_table(hb_instance_t *inst, uint16_t size)
{
	cached_pixelsize_table_t *tbl = inst->table_list->head;

	while (tbl) {
		if (tbl->advance_x == size) {
			/* Move it to the head of the list */
			if (inst->table_list->head != tbl) {
				/* Remove it from the list */
				tbl->prev->next = tbl->next;
				if (tbl->next)
					tbl->next->prev = tbl->prev;
				else
					inst->table_list->tail = tbl->prev;
				/* Insert it to the head of the list */
				tbl->prev = NULL;
				tbl->next = inst->table_list->head;
				inst->table_list->head->prev = tbl;
				inst->table_list->head = tbl;
			}
			return tbl;
		}
		tbl = tbl->next;
	}

	if (inst->table_list->count < MAX_CACHED_PIXELSIZE_TABLES) {
		tbl = (cached_pixelsize_table_t *)calloc(1, sizeof(cached_pixelsize_table_t));
		inst->table_list->count++;
	}
	else {
		/* Remove the tail of the list */
		tbl = inst->table_list->tail;
		tbl->prev->next = NULL;
		inst->table_list->tail = tbl->prev;
	}

	uint32_t i, pxw, pxh;

	for (i = 0; i < 8; i++) {
		pxw = determine_ascii_px_by_adv(inst, size * (i + 1));
		pxh = round_div(pxw * 5, 4);
		LogInfo("%d) w=%u, h=%u", i, pxw, pxh);
		tbl->table[i    ] = pxw;
		tbl->table[i + 8] = pxh;
	}
	tbl->advance_x = size;

	/* Insert it to the head of the list */
	tbl->prev = NULL;
	tbl->next = inst->table_list->head;
	if (inst->table_list->head)
		inst->table_list->head->prev = tbl;
	else
		inst->table_list->tail = tbl;
	inst->table_list->head = tbl;

	return tbl;
}

static void set_pixelsize_table(hb_instance_t *inst)
{
	cached_pixelsize_table_t *tbl;

	if (inst->advance_x == 0)
		inst->advance_x = 12;

	tbl = get_cached_pixelsize_table(inst, inst->advance_x);
	inst->pixelsize_table = tbl->table;
	inst->pixelsize_w = inst->pixelsize_table[0];
	inst->pixelsize_h = inst->pixelsize_table[8];
	inst->nominal_h = (inst->pixelsize_h > 2) ? inst->pixelsize_h - 2 : inst->pixelsize_h;
}

static int loadThirdPartyConf(void)
{
	char str[256], substr[256];
	char font_str[64];
	regmatch_t matches[4];
	regex_t regline;
	FILE *file;
	int faceindex;
	bool ok;

	if (!harfBuzz->Initialized)
		return -1;

	free_faces(&third_party_face_head, &third_party_face_tail);

	file = fopen(THIRD_PARTY_FONTS_CONF_FILE, "r");
	if (!file)
		return -1;

	// DejaVuSansMono.ttf 0
	regcomp(&regline,
		"^\\([^ ]\\+\\) \\+"
		"\\([0-9]\\)\n$", 0);

	while (fgets(str, sizeof(str), file)) {
		if (regexec(&regline, str, 4, matches, 0))
			continue;

		/* Field: font */
		str_mid(str, font_str, matches[1].rm_so, matches[1].rm_eo);

		/* Field: face index */
		faceindex = str_to_int(str_mid(str, substr, matches[2].rm_so, matches[2].rm_eo), 10, &ok);
		if (!ok)
			continue;

		open_third_party_face(font_str, faceindex);
	}

	regfree(&regline);
	fclose(file);

	return 0;
}

static int loadConf(void)
{
	char str[1024], substr[1024];
	char script_str[32], language_str[4], font_str[64];
	char *strcurr, *strnext;
	regmatch_t matches[14];
	regex_t regline, regrange;
	FILE *file;
	int i, r0, r1, ct, px, ad, faceindex, threshold, pixelsize_w, pixelsize_h, advance_x;
	bool ok, enabled;
	hb_script_t script;
	hb_language_t language;
	uint32_t ranges[40][2], rangecount;
	hb_instance_t *inst;

	if (!harfBuzz->Initialized)
		return -1;

	free_instances();
	free_faces(&face_head, &face_tail);

	file = fopen(fonts_conf_file(), "r");
	if (!file)
		return -1;

	// 0 0 0/0 LATIN la DejaVuSansMono.ttf 0 96 21 26 12 0x0020-0x036F,0x1AB0-0x1AFF,0x1D00-0x1EFF,0x2C60-0x2C7F,0xA720-0xA7FF,0xAB30-0xAB6F
	regcomp(&regline,
		"^\\([0-1]\\) \\+"
		"\\([0-9]\\+\\) \\+"
		"\\([0-9]\\+\\)/"
		"\\([0-9]\\+\\) \\+"
		"\\([A-Z_]\\+\\) \\+"
		"\\([a-z]\\{2,3\\}\\) \\+"
		"\\([^ ]\\+\\) \\+"
		"\\([0-9]\\) \\+"
		"\\([0-9]\\+\\) \\+"
		"\\([0-9]\\+\\) \\+"
		"\\([0-9]\\+\\) \\+"
		"\\([0-9]\\+\\) \\+"
		"\\(.\\+\\)\n$", 0);
	regcomp(&regrange,
		"\\(0x[0-9A-F]\\+\\)-\\(0x[0-9A-F]\\+\\)", 0);

	while (fgets(str, sizeof(str), file)) {
		if (regexec(&regline, str, 14, matches, 0))
			continue;

		/* Field: enabled */
		str_mid(str, substr, matches[1].rm_so, matches[1].rm_eo);
		enabled = (substr[0] == '0') ? false : true;

		/* Field: ct */
		ct = str_to_int(str_mid(str, substr, matches[2].rm_so, matches[2].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: px */
		px = str_to_int(str_mid(str, substr, matches[3].rm_so, matches[3].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: ad */
		ad = str_to_int(str_mid(str, substr, matches[4].rm_so, matches[4].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: script */
		str_mid(str, script_str, matches[5].rm_so, matches[5].rm_eo);
		script = HB_SCRIPT_INVALID;
#define P(sc__) \
do { \
	if (strcmp(script_str, #sc__) == 0) { \
		script = HB_SCRIPT_##sc__; \
		goto _script_matched; \
	} \
} while (0)
		P(COMMON);
		P(INHERITED);
		P(UNKNOWN);
		P(ARABIC);
		P(ARMENIAN);
		P(BENGALI);
		P(CYRILLIC);
		P(DEVANAGARI);
		P(GEORGIAN);
		P(GREEK);
		P(GUJARATI);
		P(GURMUKHI);
		P(HANGUL);
		P(HAN);
		P(HEBREW);
		P(HIRAGANA);
		P(KANNADA);
		P(KATAKANA);
		P(LAO);
		P(LATIN);
		P(MALAYALAM);
		P(ORIYA);
		P(TAMIL);
		P(TELUGU);
		P(THAI);
		P(TIBETAN);
		P(BOPOMOFO);
		P(BRAILLE);
		P(CANADIAN_SYLLABICS);
		P(CHEROKEE);
		P(ETHIOPIC);
		P(KHMER);
		P(MONGOLIAN);
		P(MYANMAR);
		P(OGHAM);
		P(RUNIC);
		P(SINHALA);
		P(SYRIAC);
		P(THAANA);
		P(YI);
		P(DESERET);
		P(GOTHIC);
		P(OLD_ITALIC);
		P(BUHID);
		P(HANUNOO);
		P(TAGALOG);
		P(TAGBANWA);
		P(CYPRIOT);
		P(LIMBU);
		P(LINEAR_B);
		P(OSMANYA);
		P(SHAVIAN);
		P(TAI_LE);
		P(UGARITIC);
		P(BUGINESE);
		P(COPTIC);
		P(GLAGOLITIC);
		P(KHAROSHTHI);
		P(NEW_TAI_LUE);
		P(OLD_PERSIAN);
		P(SYLOTI_NAGRI);
		P(TIFINAGH);
		P(BALINESE);
		P(CUNEIFORM);
		P(NKO);
		P(PHAGS_PA);
		P(PHOENICIAN);
		P(CARIAN);
		P(CHAM);
		P(KAYAH_LI);
		P(LEPCHA);
		P(LYCIAN);
		P(LYDIAN);
		P(OL_CHIKI);
		P(REJANG);
		P(SAURASHTRA);
		P(SUNDANESE);
		P(VAI);
		P(AVESTAN);
		P(BAMUM);
		P(EGYPTIAN_HIEROGLYPHS);
		P(IMPERIAL_ARAMAIC);
		P(INSCRIPTIONAL_PAHLAVI);
		P(INSCRIPTIONAL_PARTHIAN);
		P(JAVANESE);
		P(KAITHI);
		P(LISU);
		P(MEETEI_MAYEK);
		P(OLD_SOUTH_ARABIAN);
		P(OLD_TURKIC);
		P(SAMARITAN);
		P(TAI_THAM);
		P(TAI_VIET);
		P(BATAK);
		P(BRAHMI);
		P(MANDAIC);
		P(CHAKMA);
		P(MEROITIC_CURSIVE);
		P(MEROITIC_HIEROGLYPHS);
		P(MIAO);
		P(SHARADA);
		P(SORA_SOMPENG);
		P(TAKRI);
		P(BASSA_VAH);
		P(CAUCASIAN_ALBANIAN);
		P(DUPLOYAN);
		P(ELBASAN);
		P(GRANTHA);
		P(KHOJKI);
		P(KHUDAWADI);
		P(LINEAR_A);
		P(MAHAJANI);
		P(MANICHAEAN);
		P(MENDE_KIKAKUI);
		P(MODI);
		P(MRO);
		P(NABATAEAN);
		P(OLD_NORTH_ARABIAN);
		P(OLD_PERMIC);
		P(PAHAWH_HMONG);
		P(PALMYRENE);
		P(PAU_CIN_HAU);
		P(PSALTER_PAHLAVI);
		P(SIDDHAM);
		P(TIRHUTA);
		P(WARANG_CITI);
		P(AHOM);
		P(ANATOLIAN_HIEROGLYPHS);
		P(HATRAN);
		P(MULTANI);
		P(OLD_HUNGARIAN);
		P(SIGNWRITING);
		P(ADLAM);
		P(BHAIKSUKI);
		P(MARCHEN);
		P(OSAGE);
		P(TANGUT);
		P(NEWA);
#undef P
		continue;

_script_matched:
		/* Field: language */
		str_mid(str, language_str, matches[6].rm_so, matches[6].rm_eo);
		language = (*hb_language_from_string_func)(language_str, -1);

		/* Field: font */
		str_mid(str, font_str, matches[7].rm_so, matches[7].rm_eo);

		/* Field: face index */
		faceindex = str_to_int(str_mid(str, substr, matches[8].rm_so, matches[8].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: threshold */
		threshold = str_to_int(str_mid(str, substr, matches[9].rm_so, matches[9].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: pixel size (width) */
		pixelsize_w = str_to_int(str_mid(str, substr, matches[10].rm_so, matches[10].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: pixel size (height) */
		pixelsize_h = str_to_int(str_mid(str, substr, matches[11].rm_so, matches[11].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: advance (x) */
		advance_x = str_to_int(str_mid(str, substr, matches[12].rm_so, matches[12].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: ranges */
		str_mid(str, substr, matches[13].rm_so, matches[13].rm_eo);
		rangecount = 0;
		i = 0;
		while (regexec(&regrange, substr + i, 3, matches, 0) == 0) {
			r0 = str_to_int(str_mid(substr + i, str, matches[1].rm_so, matches[1].rm_eo), 16, &ok);
			if (!ok || r0 < 1)
				continue;
			r1 = str_to_int(str_mid(substr + i, str, matches[2].rm_so, matches[2].rm_eo), 16, &ok);
			if (!ok || r1 < r0)
				continue;
			ranges[rangecount][0] = r0;
			ranges[rangecount][1] = r1;
			if (++rangecount == 40)
				break;
			i += matches[2].rm_eo;
		}
		if (rangecount == 0)
			continue;

		inst = (hb_instance_t *)calloc(1, sizeof(hb_instance_t));
		if (!inst)
			break;
		inst->next = NULL;
		inst->enabled = enabled;
		inst->ct = ct;
		inst->px = px;
		inst->ad = ad;
		strcpy(inst->script, script_str);
		strcpy(inst->language, language_str);
		strcpy(inst->font, font_str);
		inst->faceindex = faceindex;
		inst->segprop.direction = (*hb_script_get_horizontal_direction_func)(script);
		inst->segprop.script = script;
		inst->segprop.language = language;
		i = 1;
		strcurr = font_str;
		while (i < INSTANCE_MAX_FACES && strcurr) {
			strnext = strchr(strcurr, ',');
			if (strnext)
				(*strnext++) = 0;
			LogDbg("%d) %s", i, strcurr);
			inst->faces[i] = get_face(strcurr, faceindex);
			if (inst->faces[i])
				i++;
			strcurr = strnext;
		}
		inst->face = inst->faces[1];
		inst->face_preferred = 1;
		inst->face_is_monospaced = 1;
		inst->threshold = threshold;
		inst->charsize = 0;
		inst->pixelsize_w = pixelsize_w;
		inst->pixelsize_h = pixelsize_h;
		inst->advance_x = advance_x;
		if (ct == 0)
			inst->nominal_h = (pixelsize_h > 2) ? pixelsize_h - 2 : pixelsize_h;
		else
			inst->nominal_h = 2;
		inst->ranges = (uint32_t (*)[2])calloc((rangecount + 1) * 2, sizeof(uint32_t));
		if (!inst->ranges) {
			free(inst);
			break;
		}
		memcpy(inst->ranges, ranges, rangecount * 2 * sizeof(uint32_t));

		if (ct == 0 && inst->face) {
			inst->table_list = (cached_pixelsize_table_list_t *)calloc(1, sizeof(cached_pixelsize_table_list_t));
			set_pixelsize_table(inst);
		}

		if (instance_tail)
			instance_tail->next = inst;
		else
			instance_head = inst;
		instance_tail = inst;

		LogInfo("Instance '%c%c%c%c' created.",
			(char)(((uint32_t)script >> 24) & 0xFF),
			(char)(((uint32_t)script >> 16) & 0xFF),
			(char)(((uint32_t)script >> 8 ) & 0xFF),
			(char)(((uint32_t)script      ) & 0xFF));
	}

	regfree(&regrange);
	regfree(&regline);
	fclose(file);

	if (access(FONTS_SETTING_FILE, F_OK) != 0)
		return 0;

	file = fopen(FONTS_SETTING_FILE, "r");
	if (!file)
		return 0;

	// 0 1 24
	regcomp(&regline,
		"^\\([0-9]\\) \\+"
		"\\([0-1]\\) \\+"
		"\\([0-9]\\+\\)\n$", 0);

	while (fgets(str, sizeof(str), file)) {
		if (regexec(&regline, str, 4, matches, 0))
			continue;

		/* Field: ct */
		ct = str_to_int(str_mid(str, substr, matches[1].rm_so, matches[1].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: enabled */
		str_mid(str, substr, matches[2].rm_so, matches[2].rm_eo);
		enabled = (substr[0] == '0') ? false : true;

		/* Field: px */
		px = str_to_int(str_mid(str, substr, matches[3].rm_so, matches[3].rm_eo), 10, &ok);
		if (!ok)
			continue;

		switch (ct) {
		case 0:
			harfBuzz->setAsciiCharEnabledState(enabled);
			harfBuzz->setAsciiCharSize(px);
			break;
		case 1:
			harfBuzz->setCjkCharEnabledState(enabled);
			harfBuzz->setCjkCharSize(px);
			break;
		case 2:
			harfBuzz->setOtherCharSize(px);
			break;
		default:
			break;
		}
	}

	regfree(&regline);
	fclose(file);

	return 0;
}

static int saveConf(void)
{
	FILE *file;
	hb_instance_t *inst;
	int enabled[3], size[3];
	int i;

	memset(enabled, 0, sizeof(enabled));
	memset(size, 0, sizeof(size));

	inst = instance_head;
	while (inst) {
		switch (inst->ct) {
		case 0:
			enabled[inst->ct] = (inst->enabled) ? 1 : 0;
			size[inst->ct] = inst->advance_x;
			break;
		case 1:
			enabled[inst->ct] = (inst->enabled) ? 1 : 0;
			size[inst->ct] = (inst->advance_x == 0) ? inst->pixelsize_w : inst->advance_x;
			break;
		case 2:
			enabled[inst->ct] = (inst->enabled) ? 1 : 0;
			size[inst->ct] = inst->pixelsize_w;
			break;
		}
		inst = inst->next;
	}

	file = fopen(FONTS_SETTING_FILE, "w");
	if (!file)
		return -1;

	for (i = 0; i < 3; i++) {
		if (size[i] == 0)
			continue;
		fprintf(file, "%d %d %d\n", i, enabled[i], size[i]);
	}
	fflush(file);
	fclose(file);

	sync();

	return 0;
}

static bool is_in_range(const hb_instance_t *inst, uint32_t unicode)
{
	uint32_t i = 0;

	while (inst->ranges[i][0] != 0) {
		if (inst->ranges[i][0] <= unicode && unicode <= inst->ranges[i][1])
			return true;
		i++;
	}
	return false;
}

static int appendUnicode(uint32_t unicode)
{
	hb_instance_t *inst = instance_head;

	while (inst) {
		if ((Settings.BitsPerDot > 0 || inst->enabled) && inst->face) {
			if (is_in_range(inst, unicode)) {
				if (harfBuzz->ActiveInst != NULL && harfBuzz->ActiveInst != inst)
					harfBuzz->shape(11);
				if (inst->segprop.direction == HB_DIRECTION_RTL) {
					/* if (lineBuffer->isEmpty())
						lineBuffer->setFlag(LINE_BUFFER_FLAG_LINE_RTL); */
					lineBuffer->setFlag(LINE_BUFFER_FLAG_CHAR_RTL);
				}
				if (inst->face_preferred + 1 < INSTANCE_MAX_FACES &&
					inst->faces[inst->face_preferred + 1]) { // 有备选字库
					if ((*FT_Get_Char_Index_func)(inst->face, unicode) == 0) { // 该字符在当前字库未定义
						harfBuzz->shape(13);
						inst->face = inst->faces[inst->face_preferred + 1];
					}
				}
				harfBuzz->Unicodes[harfBuzz->UnicodeLen++] = unicode;
				harfBuzz->ActiveInst = inst;
				if (inst->face != inst->faces[inst->face_preferred])
					harfBuzz->shape(15);
				if (harfBuzz->UnicodeLen == HARFBUZZ_MAX_UNICODE_LEN)
					harfBuzz->shape(12);
				return 0;
			}
		}
		inst = inst->next;
	}

	harfBuzz->shape(0);
	return -1;
}

static void draw_glyph(FT_GlyphSlot slot, int x, int y, uint16_t threshold)
{
	int i, j, p, q, x_max, y_max;
	uint8_t v;

	x += slot->bitmap_left;
	y -= slot->bitmap_top;
	x_max = x + slot->bitmap.width - 1;
	y_max = y + slot->bitmap.rows - 1;

	if (x < 0)
		x = 0;
	/* x_max 预留一个点的位置给加粗使用 */
	if (Settings.BitsPerDot == 0) {
		if (x_max >= HARFBUZZ_CANVAS_WIDTH - 1)
			x_max = HARFBUZZ_CANVAS_WIDTH - 2;
	}
	else {
		if (x_max >= (HARFBUZZ_CANVAS_WIDTH >> 3) - 1)
			x_max = (HARFBUZZ_CANVAS_WIDTH >> 3) - 2;
	}
	if (y < 0)
		y = 0;
	if (y_max >= HARFBUZZ_CANVAS_HEIGHT)
		y_max = HARFBUZZ_CANVAS_HEIGHT - 1;

	if (x < harfBuzz->CanvasBBox.x_min)
		harfBuzz->CanvasBBox.x_min = x;
	if (x_max > harfBuzz->CanvasBBox.x_max)
		harfBuzz->CanvasBBox.x_max = x_max;
	if (y < harfBuzz->CanvasBBox.y_min)
		harfBuzz->CanvasBBox.y_min = y;
	if (y_max > harfBuzz->CanvasBBox.y_max)
		harfBuzz->CanvasBBox.y_max = y_max;

	if (Settings.BitsPerDot == 0) {
		for (i = x, p = 0; i <= x_max; i++, p++) {
			for (j = y, q = 0; j <= y_max; j++, q++) {
				if (slot->bitmap.buffer[q * slot->bitmap.pitch + p] >= threshold) {
					harfBuzz->Canvas[j][i >> 3] |= (1 << (i & 7));
					if (Settings.EmphasizedMode[CHAR_TYPE_CJK])
						harfBuzz->Canvas[j][(i + 1) >> 3] |= (1 << ((i + 1) & 7));
				}
			}
		}
	}
	else {
		if (Settings.BlackWhiteReverseMode == 0 && Settings.Color != 0) {
			for (i = x, p = 0; i <= x_max; i++, p++) {
				for (j = y, q = 0; j <= y_max; j++, q++) {
					v = slot->bitmap.buffer[q * slot->bitmap.pitch + p] >> 1;
					if (harfBuzz->Canvas[j][i] < v)
						harfBuzz->Canvas[j][i] = v;
					if (Settings.EmphasizedMode[CHAR_TYPE_CJK])
						harfBuzz->Canvas[j][i + 1] = v;
				}
			}
		}
		else {
			for (i = x, p = 0; i <= x_max; i++, p++) {
				for (j = y, q = 0; j <= y_max; j++, q++) {
					v = slot->bitmap.buffer[q * slot->bitmap.pitch + p];
					if (harfBuzz->Canvas[j][i] < v)
						harfBuzz->Canvas[j][i] = v;
					if (Settings.EmphasizedMode[CHAR_TYPE_CJK])
						harfBuzz->Canvas[j][i + 1] = v;
				}
			}
		}
	}
}

static void shape(int cause)
{
	hb_instance_t *inst;
	hb_font_t *font;
	hb_glyph_info_t *glyph_info;
	hb_glyph_position_t *glyph_pos;
	FT_Error error;
	uint32_t i, glyph_count, cluster, pxw, pxh, half_adv;
	int cursor_x, cursor_y, x_offset, y_offset, x_advance, y_advance;
	int cluster_geox[HARFBUZZ_MAX_UNICODE_LEN][2], cluster_advx[HARFBUZZ_MAX_UNICODE_LEN][2], cluster_index;
	int x_min, x_max;

	if (harfBuzz->ActiveInst == NULL)
		return;
	if (harfBuzz->UnicodeLen == 0) {
		harfBuzz->ActiveInst = NULL;
		return;
	}

	inst = (hb_instance_t *)harfBuzz->ActiveInst;

#if 0
	char tmp[256];
	int x, l = 0;
	for (x = 0; x < harfBuzz->UnicodeLen; x++)
		l += snprintf(tmp + l, 255 - l, "%04X ", harfBuzz->Unicodes[x]);
	LogDbg("cause=%d: %s", cause, tmp);
#endif

	(*hb_buffer_reset_func)(hb_buf);
	(*hb_buffer_set_segment_properties_func)(hb_buf, &inst->segprop);
	(*hb_buffer_add_codepoints_func)(hb_buf, (const hb_codepoint_t *)(harfBuzz->Unicodes),
		harfBuzz->UnicodeLen, 0, harfBuzz->UnicodeLen);

	if (inst->ct == 0) {
		pxw = inst->pixelsize_table[Settings.CharHSize[CHAR_TYPE_CJK] - 1];
		pxh = inst->pixelsize_table[Settings.CharVSize[CHAR_TYPE_CJK] + 7];
	}
	else {
		pxw = (inst->pixelsize_w                                             ) * Settings.CharHSize[CHAR_TYPE_CJK];
		pxh = (inst->pixelsize_h == 0 ? inst->pixelsize_w : inst->pixelsize_h) * Settings.CharVSize[CHAR_TYPE_CJK];
	}
	(*FT_Set_Pixel_Sizes_func)(inst->face, pxw, pxh);
	font = (*hb_ft_font_create_func)(inst->face, NULL);
	(*hb_shape_func)(font, hb_buf, NULL, 0);
	(*hb_font_destroy_func)(font);

	glyph_info = (*hb_buffer_get_glyph_infos_func)(hb_buf, &glyph_count);
	glyph_pos = (*hb_buffer_get_glyph_positions_func)(hb_buf, &glyph_count);

	memset(harfBuzz->Canvas, 0, sizeof(harfBuzz->Canvas));
	harfBuzz->CanvasBBox.x_min = 32;
	harfBuzz->CanvasBBox.x_max = 32;
	harfBuzz->CanvasBBox.y_min = 192 - inst->nominal_h * Settings.CharVSize[CHAR_TYPE_CJK];
	harfBuzz->CanvasBBox.y_max = 191;
	if (harfBuzz->CanvasBBox.y_min < 0)
		harfBuzz->CanvasBBox.y_min = 0;
	cursor_x = 32;
	cursor_y = 191;

	cluster = (uint32_t)(-1);
	memset(cluster_geox, 0, sizeof(cluster_geox));
	memset(cluster_advx, 0, sizeof(cluster_advx));
	cluster_index = -1;

	for (i = 0; i < glyph_count; i++) {
		if (glyph_info[i].cluster != cluster) {
			cluster = glyph_info[i].cluster;
			cluster_index++;
			cluster_geox[cluster_index][0] = HARFBUZZ_CANVAS_WIDTH; /* Min */
			cluster_geox[cluster_index][1] = 0; /* Max */
			cluster_advx[cluster_index][0] = HARFBUZZ_CANVAS_WIDTH; /* Min */
			cluster_advx[cluster_index][1] = 0; /* Max */
		}

		x_offset = glyph_pos[i].x_offset >> 6;
		x_advance = glyph_pos[i].x_advance >> 6;
		if (inst->face_is_monospaced &&
			inst->advance_x != 0 && x_advance > 0 &&
			inst->advance_x * Settings.CharHSize[CHAR_TYPE_CJK] != x_advance) {
			x_offset = 0;
			half_adv = inst->advance_x >> 1;
			if (x_advance < half_adv + 2)
				x_advance = round_div(x_advance, half_adv) * half_adv;
			else
				x_advance = round_div(x_advance, inst->advance_x) * inst->advance_x;
		}
		y_offset = glyph_pos[i].y_offset >> 6;
		y_advance = glyph_pos[i].y_advance >> 6;

		error = (*FT_Load_Glyph_func)(inst->face, glyph_info[i].codepoint, FT_LOAD_RENDER);
		if (error) {
			LogError("FT_Load_Glyph failed (error=%d).", error);
			continue;
		}

#if 0
		LogDbg("[%3u] %5u %3d %3d %3d %3d %3d %3d %3d %3d",
			glyph_info[i].cluster,
			glyph_info[i].codepoint,
			x_offset,
			y_offset,
			x_advance,
			y_advance,
			inst->face->glyph->bitmap_left,
			inst->face->glyph->bitmap_top,
			inst->face->glyph->bitmap.width,
			inst->face->glyph->bitmap.rows);
#endif

		x_min = cursor_x + x_offset + inst->face->glyph->bitmap_left;
		x_max = x_min + inst->face->glyph->bitmap.width - 1;
		if (x_min < cluster_geox[cluster_index][0])
			cluster_geox[cluster_index][0] = x_min;
		if (x_max > cluster_geox[cluster_index][1])
			cluster_geox[cluster_index][1] = x_max;

		draw_glyph(inst->face->glyph,
			cursor_x + x_offset, cursor_y + y_offset, inst->threshold);

		if (cursor_x < cluster_advx[cluster_index][0])
			cluster_advx[cluster_index][0] = cursor_x;
		cursor_x += x_advance;
		cursor_y += y_advance;
		if (cursor_x - 1 > cluster_advx[cluster_index][1])
			cluster_advx[cluster_index][1] = cursor_x - 1;
	}
	if (inst->advance_x == 0 && cluster_index >= 0) {
		if (cluster_geox[cluster_index][0] < cluster_advx[cluster_index][0])
			cluster_advx[cluster_index][0] = cluster_geox[cluster_index][0];
		if (cluster_geox[cluster_index][1] > cluster_advx[cluster_index][1])
			cluster_advx[cluster_index][1] = cluster_geox[cluster_index][1];
	}
	cluster_index++;

#if 0
	LogDbg("x=[%d,%d], y=[%d,%d]",
		harfBuzz->CanvasBBox.x_min, harfBuzz->CanvasBBox.x_max,
		harfBuzz->CanvasBBox.y_min, harfBuzz->CanvasBBox.y_max);
#endif

#if 0
	for (i = 0; i < (uint32_t)cluster_index; i++)
		LogDbg("[%3u] %4d %4d, %4d %4d", i + 1,
			cluster_advx[i][0], cluster_advx[i][1],
			cluster_geox[i][0], cluster_geox[i][1]);
#endif

	if (lineBuffer->Flags & LINE_BUFFER_FLAG_LINE_RTL ||
		inst->segprop.script == HB_SCRIPT_ARABIC ||
		inst->segprop.script == HB_SCRIPT_HEBREW) {
		x_min = cluster_advx[0][0];
		x_max = cluster_advx[cluster_index - 1][1] + 1;
		if (lineBuffer->appendHarfBuzzCanvas(x_min, x_max) > 0) {
			lineBuffer->printLine();
			lineBuffer->appendHarfBuzzCanvas(x_min, x_max);
		}
	}
	else {
		x_min = cluster_advx[0][0];
		for (i = 0; i < (uint32_t)cluster_index; i++) {
			if (inst->advance_x == 0) {
				while (i < (uint32_t)(cluster_index - 1) && cluster_advx[i][1] > cluster_advx[i + 1][0])
					i++;
			}
			x_max = cluster_advx[i][1] + 1;
			if (lineBuffer->appendHarfBuzzCanvas(x_min, x_max) > 0) {
				lineBuffer->printLine();
				lineBuffer->appendHarfBuzzCanvas(x_min, x_max);
			}
			x_min = x_max;
		}
	}

	lineBuffer->clearFlag(LINE_BUFFER_FLAG_CHAR_RTL);

	inst->face = inst->faces[inst->face_preferred];

	harfBuzz->UnicodeLen = 0;
	harfBuzz->ActiveInst = NULL;
}

static void enableThirdPartyFace(uint8_t ct, uint8_t index)
{
	face_t *face;
	hb_instance_t *inst;
	uint8_t i;

	face = third_party_face_head;
	i = 0;
	while (face && i < index) {
		face = face->next;
		i++;
	}
	if (!face)
		return;

	harfBuzz->shape(16);

	inst = instance_head;
	while (inst) {
		if (inst->ct == ct) {
			inst->faces[0] = face->face;
			inst->face_preferred = 0;
			inst->face = inst->faces[0];
			if (ct == 0)
				harfBuzz->setAsciiCharSize(inst->advance_x);
		}
		inst = inst->next;
	}
}

static void disableThirdPartyFace(uint8_t ct)
{
	hb_instance_t *inst;

	harfBuzz->shape(17);

	inst = instance_head;
	while (inst) {
		if (inst->ct == ct) {
			inst->faces[0] = NULL;
			inst->face_preferred = 1;
			inst->face = inst->faces[1];
			if (ct == 0)
				harfBuzz->setAsciiCharSize(inst->advance_x);
		}
		inst = inst->next;
	}
}

static void setAsciiCharEnabledState(bool enabled)
{
	hb_instance_t *inst;

	inst = instance_head;
	while (inst) {
		if (inst->ct == 0)
			inst->enabled = enabled;
		inst = inst->next;
	}
}

static void setCjkCharEnabledState(bool enabled)
{
	hb_instance_t *inst;

	inst = instance_head;
	while (inst) {
		if (inst->ct == 1)
			inst->enabled = enabled;
		inst = inst->next;
	}
}

static void setAsciiCharSize(uint32_t size)
{
	hb_instance_t *inst;

	inst = instance_head;
	while (inst) {
		if (inst->ct == 0 && inst->face && inst->charsize != size) {
			inst->advance_x = size;
			set_pixelsize_table(inst);
			inst->charsize = size;
		}
		inst = inst->next;
	}
}

static void setCjkCharSize(uint32_t size)
{
	hb_instance_t *inst;
	uint32_t px, ad;

	inst = instance_head;
	while (inst) {
		if (inst->ct == 1 && inst->charsize != size) {
			if (inst->ad == 0) {
				px = size;
				ad = 0;
			}
			else {
				px = round_div(size * inst->px, inst->ad);
				ad = size;
			}
			LogInfo("px=%u, ad=%u", px, ad);
			inst->pixelsize_w = px;
			inst->advance_x = ad;
			inst->nominal_h = 2;
			inst->charsize = size;
		}
		inst = inst->next;
	}
}

static void setOtherCharSize(uint32_t size)
{
	hb_instance_t *inst;

	inst = instance_head;
	while (inst) {
		if (inst->ct == 2 && inst->charsize != size) {
			inst->pixelsize_w = size;
			inst->nominal_h = 2;
			inst->charsize = size;
		}
		inst = inst->next;
	}
}

static void *getFontConfItem(void *prev, host_cmd_font_conf_item_t *item)
{
	hb_instance_t *inst;
	int i = 0;

	if (prev)
		inst = ((hb_instance_t *)prev)->next;
	else
		inst = instance_head;

	if (!inst)
		return NULL;

	item->enabled = inst->enabled;
	item->ct = inst->ct;
	item->px = inst->px;
	item->ad = inst->ad;
	strcpy(item->script, inst->script);
	strcpy(item->language, inst->language);
	strcpy(item->font, inst->font);
	item->faceindex = inst->faceindex;
	item->threshold = inst->threshold;
	item->pixelsize_w = inst->pixelsize_w;
	item->pixelsize_h = inst->pixelsize_h;
	item->advance_x = inst->advance_x;
	while (inst->ranges[i][0] != 0) {
		item->ranges[i][0] = inst->ranges[i][0];
		item->ranges[i][1] = inst->ranges[i][1];
		i++;
	}
	item->ranges[i][0] = 0;

	return inst;
}

static void setFontConfItemStart(void)
{
	if (fonts_conf)
		fclose(fonts_conf);
	fonts_conf = fopen(fonts_conf_file(), "w");
}

static void appendFontConfItem(const host_cmd_font_conf_item_t *item)
{
	char str[1024];
	int i = 0, len;

	if (!fonts_conf)
		return;

	len = snprintf(str, sizeof(str) - 1, "%d %3u %2u/%-2u %-31s %-3s %-61s %d %3u %2u %2u %2u ",
			(item->enabled) ? 1 : 0,
			item->ct,
			item->px,
			item->ad,
			item->script,
			item->language,
			item->font,
			item->faceindex,
			item->threshold,
			item->pixelsize_w,
			item->pixelsize_h,
			item->advance_x);
	while (item->ranges[i][0] != 0) {
		len += snprintf(str + len, sizeof(str) - len - 1, "0x%04X-0x%04X,",
				item->ranges[i][0], item->ranges[i][1]);
		i++;
	}
	str[len - 1] = '\n';

	fwrite(str, 1, len, fonts_conf);
	fflush(fonts_conf);
}

static void setFontConfItemEnd(void)
{
	if (!fonts_conf)
		return;

	fflush(fonts_conf);
	fclose(fonts_conf);
	fonts_conf = NULL;

	sync();
}

static void resetFontConf(void)
{
	remove(FONTS_SETTING_FILE);
	sync();
}

static HarfBuzz theHarfBuzz = {
	.init = init,
	.loadThirdPartyConf = loadThirdPartyConf,
	.loadConf = loadConf,
	.saveConf = saveConf,
	.appendUnicode = appendUnicode,
	.shape = shape,
	.enableThirdPartyFace = enableThirdPartyFace,
	.disableThirdPartyFace = disableThirdPartyFace,
	.setAsciiCharEnabledState = setAsciiCharEnabledState,
	.setCjkCharEnabledState = setCjkCharEnabledState,
	.setAsciiCharSize = setAsciiCharSize,
	.setCjkCharSize = setCjkCharSize,
	.setOtherCharSize = setOtherCharSize,
	.getFontConfItem = getFontConfItem,
	.setFontConfItemStart = setFontConfItemStart,
	.appendFontConfItem = appendFontConfItem,
	.setFontConfItemEnd = setFontConfItemEnd,
	.resetFontConf = resetFontConf
};

HarfBuzz *harfBuzz = &theHarfBuzz;
