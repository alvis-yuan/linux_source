#include <sys/types.h>
#include <regex.h>
#include <dlfcn.h>
#include <hb.h>
#include <hb-ft.h>
#include <unistd.h>
#include "tspl_LineBuffer.h"
#include "tspl_util.h"
#include "tspl_HarfBuzz.h"
#include "tspl_PageBuffer.h"
#include "tspl_config.h"

#define FONTS_CONF_FILE     "/data/PUB/fonts/fonts.conf"
#define FONTS_CONF_FILE_ORG "/data/PUB/fonts/fonts.conf.org"

typedef struct _face_t face_t;
struct _face_t {
	face_t *next;
	char *filename;
	int faceindex;
	FT_Face face;
};

typedef struct _hb_instance_t hb_instance_t;
struct _hb_instance_t {
	hb_instance_t *next;
	bool enabled;
	uint8_t ct;
	uint8_t px;
	uint8_t ad;
	char script[32];
	char language[4];
	char font[64];
	int faceindex;
	hb_segment_properties_t segprop;
	FT_Face face;
	uint16_t threshold;
	uint16_t pixelsize_w;
	uint16_t pixelsize_h;
	uint16_t advance_x;
	uint16_t nominal_h;
	uint32_t (*ranges)[2];
};

/* FreeType exported functions */
static FT_Error (*FT_Init_FreeType_func)(FT_Library *) = NULL;
static FT_Error (*FT_Open_Face_func)(FT_Library, const FT_Open_Args *, FT_Long, FT_Face *) = NULL;
static FT_Error (*FT_Set_Pixel_Sizes_func)(FT_Face, FT_UInt, FT_UInt) = NULL;
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
static face_t *face_head = NULL, *face_tail = NULL;
static hb_instance_t *instance_head = NULL, *instance_tail = NULL;
static hb_buffer_t *hb_buf = NULL;
// static FILE *fonts_conf = NULL;

static int init(void)
{
	struct stat stbuf;
	void *ft_handle = NULL, *hb_handle = NULL;
	char *error;
	FT_Error e;

	if (stat(FONTS_CONF_FILE, &stbuf))
		return -1;
	memset(tspl_harfBuzz->Canvas, 0, sizeof(tspl_harfBuzz->Canvas));
	memset(&tspl_harfBuzz->CanvasBBox, 0, sizeof(tspl_harfBuzz->CanvasBBox));
	memset(tspl_harfBuzz->Unicodes, 0, sizeof(tspl_harfBuzz->Unicodes));
	tspl_harfBuzz->UnicodeLen = 0;
	tspl_harfBuzz->ActiveInst = NULL;
	tspl_harfBuzz->Initialized = false;

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

	tspl_harfBuzz->Initialized = true;

	if (stat(FONTS_CONF_FILE_ORG, &stbuf)) {
		system("cp -f " FONTS_CONF_FILE " " FONTS_CONF_FILE_ORG);
		sync();
	}

	return tspl_harfBuzz->loadConf();

_fail:
	if (hb_buf && !(*hb_buffer_allocation_successful_func)(hb_buf))
		(*hb_buffer_destroy_func)(hb_buf);
	if (hb_handle)
		dlclose(hb_handle);
	if (ft_handle)
		dlclose(ft_handle);
	return -1;
}

static void free_faces(void)
{
	face_t *face = face_head, *next;

	while (face) {
		next = face->next;
		(*FT_Done_Face_func)(face->face);
		if (face->filename)
			free(face->filename);
		free(face);
		face = next;
	}

	face_head = NULL;
	face_tail = NULL;
}

static void free_instances(void)
{
	hb_instance_t *inst = instance_head, *next;

	while (inst) {
		next = inst->next;
		if (inst->ranges)
			free(inst->ranges);
		free(inst);
		inst = next;
	}

	instance_head = NULL;
	instance_tail = NULL;
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

static int loadConf(void)
{
	char str[1024], substr[1024];
	char script_str[32], language_str[4], font_str[64];
	regmatch_t matches[14];
	regex_t regline, regrange;
	FILE *file;
	int i, r0, r1, ct, px, ad, faceindex, threshold, pixelsize_w, pixelsize_h, advance_x;
	bool ok, enabled;
	hb_script_t script;
	hb_language_t language;
	uint32_t ranges[40][2], rangecount;
	hb_instance_t *inst;

	if (!tspl_harfBuzz->Initialized)
		return -1;

	free_instances();
	free_faces();

	file = fopen(FONTS_CONF_FILE, "r");
	if (!file)
		return -1;

	// 0 0 0/0 LATIN la DejaVuSansMono.ttf 0 96 20 25 0 0x0020-0x036F,0x1AB0-0x1AFF,0x1D00-0x1EFF,0x2C60-0x2C7F,0xA720-0xA7FF,0xAB30-0xAB6F
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
		ct = str_to_int_util(str_mid(str, substr, matches[2].rm_so, matches[2].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: px */
		px = str_to_int_util(str_mid(str, substr, matches[3].rm_so, matches[3].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: ad */
		ad = str_to_int_util(str_mid(str, substr, matches[4].rm_so, matches[4].rm_eo), 10, &ok);
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
		faceindex = str_to_int_util(str_mid(str, substr, matches[8].rm_so, matches[8].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: threshold */
		threshold = str_to_int_util(str_mid(str, substr, matches[9].rm_so, matches[9].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: pixel size (width) */
		pixelsize_w = str_to_int_util(str_mid(str, substr, matches[10].rm_so, matches[10].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: pixel size (height) */
		pixelsize_h = str_to_int_util(str_mid(str, substr, matches[11].rm_so, matches[11].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: advance (x) */
		advance_x = str_to_int_util(str_mid(str, substr, matches[12].rm_so, matches[12].rm_eo), 10, &ok);
		if (!ok)
			continue;

		/* Field: ranges */
		str_mid(str, substr, matches[13].rm_so, matches[13].rm_eo);
		rangecount = 0;
		i = 0;
		while (regexec(&regrange, substr + i, 3, matches, 0) == 0) {
			r0 = str_to_int_util(str_mid(substr + i, str, matches[1].rm_so, matches[1].rm_eo), 16, &ok);
			if (!ok || r0 < 1)
				continue;
			r1 = str_to_int_util(str_mid(substr + i, str, matches[2].rm_so, matches[2].rm_eo), 16, &ok);
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
		inst->face = get_face(font_str, faceindex);
		inst->threshold = threshold;
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

	// LogDbg("instance_head: language=%s, enabled=%d, face=%ld, inst->ranges=%d-%d", 
	// 	inst->language, inst->enabled, (long int)inst->face, inst->ranges[0][0], inst->ranges[0][1]);


	while (inst) {
		if (inst->enabled && inst->face) {
			if (is_in_range(inst, unicode)) {
				if (tspl_harfBuzz->ActiveInst != NULL && tspl_harfBuzz->ActiveInst != inst)
					tspl_harfBuzz->shape(11);
				tspl_harfBuzz->Unicodes[tspl_harfBuzz->UnicodeLen++] = unicode;
				tspl_harfBuzz->ActiveInst = inst;
				if (tspl_harfBuzz->UnicodeLen == HARFBUZZ_MAX_UNICODE_LEN)
					tspl_harfBuzz->shape(12);
				// LogDbg("in range");
				return 0;
			}
		}
		inst = inst->next;
	}

	tspl_harfBuzz->shape(0);
	return -1;
}

static void draw_glyph(FT_GlyphSlot slot, int x, int y, uint16_t threshold)
{
	int i, j, p, q, x_max, y_max;

	x += slot->bitmap_left;
	y -= slot->bitmap_top;
	x_max = x + slot->bitmap.width;
	y_max = y + slot->bitmap.rows;

	for (i = x, p = 0; i < x_max; i++, p++) {
		for (j = y, q = 0; j < y_max; j++, q++) {
			if (i < 0 || i >= HARFBUZZ_CANVAS_WIDTH ||
				j < 0 || j >= HARFBUZZ_CANVAS_HEIGHT)
				continue;
			if (i < tspl_harfBuzz->CanvasBBox.x_min)
				tspl_harfBuzz->CanvasBBox.x_min = i;
			else if (i > tspl_harfBuzz->CanvasBBox.x_max)
				tspl_harfBuzz->CanvasBBox.x_max = i;
			if (j < tspl_harfBuzz->CanvasBBox.y_min)
				tspl_harfBuzz->CanvasBBox.y_min = j;
			else if (j > tspl_harfBuzz->CanvasBBox.y_max)
				tspl_harfBuzz->CanvasBBox.y_max = j;
			if (slot->bitmap.buffer[q * slot->bitmap.pitch + p] >= threshold)
				tspl_harfBuzz->Canvas[j][i >> 3] |= (1 << (i & 7));
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
	uint32_t i, glyph_count, cluster;
	int cursor_x, cursor_y, x_offset, y_offset, x_advance, y_advance;
	int cluster_geox[HARFBUZZ_MAX_UNICODE_LEN][2], cluster_advx[HARFBUZZ_MAX_UNICODE_LEN][2], cluster_index;
	int cluster_y[HARFBUZZ_MAX_UNICODE_LEN][2];//[0]y_min [1]y_max
	int x_min, x_max;

	if (tspl_harfBuzz->ActiveInst == NULL)
		return;
	if (tspl_harfBuzz->UnicodeLen == 0) {
		tspl_harfBuzz->ActiveInst = NULL;
		return;
	}

	inst = (hb_instance_t *)tspl_harfBuzz->ActiveInst;

#if 0
	char tmp[256];
	int x, l = 0;
	for (x = 0; x < tspl_harfBuzz->UnicodeLen; x++)
		l += snprintf(tmp + l, 255 - l, "%04X ", tspl_harfBuzz->Unicodes[x]);
	LogDbg("cause=%d: %s", cause, tmp);
#endif

	if (inst->segprop.direction == HB_DIRECTION_RTL)
		tspl_lineBuffer->setFlag(LINE_BUFFER_FLAG_LINE_RTL);

	(*hb_buffer_reset_func)(hb_buf);
	(*hb_buffer_set_segment_properties_func)(hb_buf, &inst->segprop);
	(*hb_buffer_add_codepoints_func)(hb_buf, (const hb_codepoint_t *)(tspl_harfBuzz->Unicodes),
		tspl_harfBuzz->UnicodeLen, 0, tspl_harfBuzz->UnicodeLen);

	(*FT_Set_Pixel_Sizes_func)(inst->face,
		(inst->pixelsize_w                                             ) * pageBuffer->HCharSize,
		(inst->pixelsize_h == 0 ? inst->pixelsize_w : inst->pixelsize_h) * pageBuffer->VCharSize);
	font = (*hb_ft_font_create_func)(inst->face, NULL);
	(*hb_shape_func)(font, hb_buf, NULL, 0);
	(*hb_font_destroy_func)(font);

	glyph_info = (*hb_buffer_get_glyph_infos_func)(hb_buf, &glyph_count);
	glyph_pos = (*hb_buffer_get_glyph_positions_func)(hb_buf, &glyph_count);

	memset(tspl_harfBuzz->Canvas, 0, sizeof(tspl_harfBuzz->Canvas));
	tspl_harfBuzz->CanvasBBox.x_min = 32;
	tspl_harfBuzz->CanvasBBox.x_max = 32;
	tspl_harfBuzz->CanvasBBox.y_min = 192 - inst->nominal_h * pageBuffer->VCharSize;
	tspl_harfBuzz->CanvasBBox.y_max = 191;
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
		if (inst->advance_x != 0) {
			x_offset = 0;
			x_advance = x_advance * inst->advance_x / inst->pixelsize_w;// 例如配置文件里设置x_advance为25，但实际只想前进24，使字体看起来更饱满
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

		/* 单独记录下每个字符集合的高度信息，用于appendHarfBuzzCanvas指示Linebuffer的高度 */
		/* 并且保证每个字符高度不得小于24，防止因为某些特殊情况（一行全是'-'）出现撑不开lineBuffer->Height的情况 */
		cluster_y[cluster_index][0] = cursor_y + y_offset - ((inst->face->glyph->bitmap_top < 24) ? (24) : (inst->face->glyph->bitmap_top));
		cluster_y[cluster_index][1] = (cursor_y + y_offset - inst->face->glyph->bitmap_top) // 基于基本的字符高度进行字符底部位置的计算
										+ inst->face->glyph->bitmap.rows;

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
		tspl_harfBuzz->CanvasBBox.x_min, tspl_harfBuzz->CanvasBBox.x_max,
		tspl_harfBuzz->CanvasBBox.y_min, tspl_harfBuzz->CanvasBBox.y_max);
#endif

#if 0
	for (i = 0; i < (uint32_t)cluster_index; i++)
		LogDbg("[%3u] %4d %4d, %4d %4d", i + 1,
			cluster_advx[i][0], cluster_advx[i][1],
			cluster_geox[i][0], cluster_geox[i][1]);
#endif

	if (inst->segprop.script == HB_SCRIPT_ARABIC ||
		inst->segprop.script == HB_SCRIPT_HEBREW) {
		x_min = tspl_harfBuzz->CanvasBBox.x_min;
		x_max = tspl_harfBuzz->CanvasBBox.x_max;
		int _cluster_y[2];
		_cluster_y[0] = tspl_harfBuzz->CanvasBBox.y_min;
		_cluster_y[1] = tspl_harfBuzz->CanvasBBox.y_max;
		if (tspl_lineBuffer->appendHarfBuzzCanvas(x_min, x_max, _cluster_y) > 0) {
			tspl_lineBuffer->printLine();
			tspl_lineBuffer->appendHarfBuzzCanvas(x_min, x_max, _cluster_y);
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
			if (tspl_lineBuffer->appendHarfBuzzCanvas(x_min, x_max, cluster_y[i]) > 0) {
				tspl_lineBuffer->printLine();
				tspl_lineBuffer->appendHarfBuzzCanvas(x_min, x_max, cluster_y[i]);
			}
			x_min = x_max;
		}
	}

	tspl_harfBuzz->UnicodeLen = 0;
	tspl_harfBuzz->ActiveInst = NULL;
}

static HarfBuzz theHarfBuzz = {
	.init = init,
	.loadConf = loadConf,
	.appendUnicode = appendUnicode,
	.shape = shape,
};

HarfBuzz *tspl_harfBuzz = &theHarfBuzz;
