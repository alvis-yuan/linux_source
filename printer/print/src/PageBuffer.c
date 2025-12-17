#include <dlfcn.h>
#include <math.h>
#include "libcommon.h"
#include "printer.h"
#include "Runtime_Data.h"
#include "PageBuffer.h"

static cairo_t *(*cairo_create_func)(cairo_surface_t *) = NULL;
static void (*cairo_destroy_func)(cairo_t *) = NULL;
static void (*cairo_save_func)(cairo_t *) = NULL;
static void (*cairo_restore_func)(cairo_t *) = NULL;
static cairo_status_t (*cairo_status_func)(cairo_t *) = NULL;
static unsigned int (*cairo_get_reference_count_func)(cairo_t *) = NULL;

static void (*cairo_set_operator_func)(cairo_t *, cairo_operator_t) = NULL;
static void (*cairo_set_source_func)(cairo_t *, cairo_pattern_t *) = NULL;
static void (*cairo_set_source_surface_func)(cairo_t *, cairo_surface_t *, double, double) = NULL;
static void (*cairo_rectangle_func)(cairo_t *, double, double, double, double) = NULL;
static void (*cairo_paint_func)(cairo_t *) = NULL;
static void (*cairo_fill_func)(cairo_t *) = NULL;

static int (*cairo_format_stride_for_width_func)(cairo_format_t, int) = NULL;
static cairo_surface_t *(*cairo_image_surface_create_func)(cairo_format_t, int, int) = NULL;
static cairo_surface_t *(*cairo_image_surface_create_for_data_func)(unsigned char *, cairo_format_t, int, int, int) = NULL;
static unsigned char *(*cairo_image_surface_get_data_func)(cairo_surface_t *) = NULL;
static int (*cairo_image_surface_get_stride_func)(cairo_surface_t *) = NULL;
static void (*cairo_surface_destroy_func)(cairo_surface_t *) = NULL;
static cairo_status_t (*cairo_surface_status_func)(cairo_surface_t *) = NULL;
static unsigned int (*cairo_surface_get_reference_count_func)(cairo_surface_t *) = NULL;

static cairo_pattern_t *(*cairo_pattern_create_rgb_func)(double, double, double) = NULL;
static cairo_pattern_t *(*cairo_pattern_create_for_surface_func)(cairo_surface_t *) = NULL;
static void (*cairo_pattern_destroy_func)(cairo_pattern_t *) = NULL;

static void (*cairo_translate_func)(cairo_t *, double, double) = NULL;
static void (*cairo_scale_func)(cairo_t *, double, double) = NULL;
static void (*cairo_rotate_func)(cairo_t *, double) = NULL;
static void (*cairo_identity_matrix_func)(cairo_t *) = NULL;
static void (*cairo_set_matrix_func)(cairo_t *, const cairo_matrix_t *) = NULL;

#define CAIRO_SAVE() \
do { (*cairo_save_func)(pageBuffer->Cr); } while (0)
#define CAIRO_RESTORE() \
do { (*cairo_restore_func)(pageBuffer->Cr); } while (0)
#define CAIRO_FORMAT \
((Settings.BitsPerDot == 0) ? CAIRO_FORMAT_A1 : CAIRO_FORMAT_A8)

static void dump_surface_data(const uint8_t *data, uint32_t width, uint32_t height, uint32_t stride)
{
	char buf[1024];
	uint32_t i, j, offset;

	LogDbg("w=%u, h=%u, stride=%u", width, height, stride);
	if (width == 0 || height == 0)
		return;

	width = (width - 1) / 8 + 1;
	for (i = 0; i < height; i++) {
		offset = 0;
		for (j = 0; j < width; j++) {
			offset += sprintf(buf + offset, "%02X", data[j]);
		}
		data += stride;
		LogDbg("%s", buf);
	}
}

static void paint_surface(cairo_t *cr, cairo_surface_t *surface, uint32_t x, uint32_t y)
{
	cairo_pattern_t *pattern;

	(*cairo_save_func)(cr);

	(*cairo_translate_func)(cr, x, y);
	pattern = (*cairo_pattern_create_for_surface_func)(surface);
	(*cairo_set_source_func)(cr, pattern);
	(*cairo_paint_func)(cr);
	(*cairo_pattern_destroy_func)(pattern);

	(*cairo_restore_func)(cr);
}

static void destroy_surface(cairo_surface_t **psurface)
{
#define surface (*psurface)

	while ((*cairo_surface_get_reference_count_func)(surface) > 0)
		(*cairo_surface_destroy_func)(surface);
	surface = NULL;

#undef surface
}

static void destroy_cr(cairo_t **pcr)
{
#define cr (*pcr)

	while ((*cairo_get_reference_count_func)(cr) > 0)
		(*cairo_destroy_func)(cr);
	cr = NULL;

#undef cr
}

static int init(void)
{
	void *cairo_handle = NULL;
	char *error;

	pageBuffer->resetPrintArea();
	pageBuffer->resetDirection();
	pageBuffer->Canvas = NULL;
	pageBuffer->Surface = NULL;
	pageBuffer->CanvasCr = NULL;
	pageBuffer->Cr = NULL;
	pageBuffer->Initialized = false;

	cairo_handle = dlopen("/usr/lib/libcairo.so", RTLD_NOW);
	if (!cairo_handle) {
		LogInfo("libcairo.so not available.");
		goto _fail;
	}

#define RESOLVE(name__) \
do { \
	dlerror(); \
	name__##_func = (typeof(name__##_func))dlsym(cairo_handle, #name__); \
	if (name__##_func == NULL || (error = dlerror()) != NULL) { \
		LogError("Failed to resolve " #name__); \
		goto _fail; \
	} \
} while (0)

	RESOLVE(cairo_create                       );
	RESOLVE(cairo_destroy                      );
	RESOLVE(cairo_save                         );
	RESOLVE(cairo_restore                      );
	RESOLVE(cairo_status                       );
	RESOLVE(cairo_get_reference_count          );

	RESOLVE(cairo_set_operator                 );
	RESOLVE(cairo_set_source                   );
	RESOLVE(cairo_set_source_surface           );
	RESOLVE(cairo_rectangle                    );
	RESOLVE(cairo_paint                        );
	RESOLVE(cairo_fill                         );

	RESOLVE(cairo_format_stride_for_width      );
	RESOLVE(cairo_image_surface_create         );
	RESOLVE(cairo_image_surface_create_for_data);
	RESOLVE(cairo_image_surface_get_data       );
	RESOLVE(cairo_image_surface_get_stride     );
	RESOLVE(cairo_surface_destroy              );
	RESOLVE(cairo_surface_status               );
	RESOLVE(cairo_surface_get_reference_count  );

	RESOLVE(cairo_pattern_create_rgb           );
	RESOLVE(cairo_pattern_create_for_surface   );
	RESOLVE(cairo_pattern_destroy              );

	RESOLVE(cairo_translate                    );
	RESOLVE(cairo_scale                        );
	RESOLVE(cairo_rotate                       );
	RESOLVE(cairo_identity_matrix              );
	RESOLVE(cairo_set_matrix                   );

#undef RESOLVE

	pageBuffer->Initialized = true;

	return 0;

_fail:
	if (cairo_handle)
		dlclose(cairo_handle);
	return -1;
}

static void create(void)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	cairo_status_t status;

	if (!pageBuffer->Initialized)
		return;

	pageBuffer->destroy();

	surface = (*cairo_image_surface_create_func)(CAIRO_FORMAT,
					DOTS_PER_LINE, PAGE_BUFFER_MAX_HEIGHT);
	status = (*cairo_surface_status_func)(surface);
	if (status != CAIRO_STATUS_SUCCESS) {
		LogError("status=%d", (int)status);
		return;
	}
	pageBuffer->Canvas = surface;

	cr = (*cairo_create_func)(surface);
	status = (*cairo_status_func)(cr);
	if (status != CAIRO_STATUS_SUCCESS) {
		LogError("status=%d", (int)status);
		goto _fail;
	}
	pageBuffer->CanvasCr = cr;

	surface = (*cairo_image_surface_create_func)(CAIRO_FORMAT,
					pageBuffer->PrintArea.w, pageBuffer->PrintArea.h);
	status = (*cairo_surface_status_func)(surface);
	if (status != CAIRO_STATUS_SUCCESS) {
		LogError("status=%d", (int)status);
		goto _fail;
	}
	pageBuffer->Surface = surface;

	cr = (*cairo_create_func)(surface);
	status = (*cairo_status_func)(cr);
	if (status != CAIRO_STATUS_SUCCESS) {
		LogError("status=%d", (int)status);
		goto _fail;
	}
	pageBuffer->Cr = cr;

	pageBuffer->setDirection(pageBuffer->Direction);

	LogInfo("x=%u, y=%u, w=%u, h=%u, h_max=%u, dir=%u",
		pageBuffer->PrintArea.x, pageBuffer->PrintArea.y,
		pageBuffer->PrintArea.w, pageBuffer->PrintArea.h,
		pageBuffer->PrintArea.h_max, pageBuffer->Direction);
	return;

_fail:
	if (pageBuffer->CanvasCr) {
		(*cairo_destroy_func)(pageBuffer->CanvasCr);
		pageBuffer->CanvasCr = NULL;
	}
	if (pageBuffer->Surface) {
		(*cairo_surface_destroy_func)(pageBuffer->Surface);
		pageBuffer->Surface = NULL;
	}
	if (pageBuffer->Canvas) {
		(*cairo_surface_destroy_func)(pageBuffer->Canvas);
		pageBuffer->Canvas = NULL;
	}
}

static void destroy(void)
{
	if (!pageBuffer->Initialized)
		return;

	if (pageBuffer->Cr) {
		destroy_cr(&pageBuffer->Cr);
		LogInfo("Context destroyed.");
	}

	if (pageBuffer->CanvasCr) {
		destroy_cr(&pageBuffer->CanvasCr);
		LogInfo("Context destroyed.");
	}

	if (pageBuffer->Surface) {
		destroy_surface(&pageBuffer->Surface);
		LogInfo("Surface destroyed.");
	}

	if (pageBuffer->Canvas) {
		destroy_surface(&pageBuffer->Canvas);
		LogInfo("Canvas destroyed.");
	}
}

static void print(void)
{
	uint8_t *data, *dotline;
	int stride, type;
	uint32_t row, bytes;

	if (!pageBuffer->Cr)
		return;

	paint_surface(pageBuffer->CanvasCr, pageBuffer->Surface,
		pageBuffer->PrintArea.x, pageBuffer->PrintArea.y);

	data = (*cairo_image_surface_get_data_func)(pageBuffer->Canvas);
	stride = (*cairo_image_surface_get_stride_func)(pageBuffer->Canvas);
	LogDbg("stride=%d", stride);

	if (pageBuffer->PrintArea.h_max == 0)
		pageBuffer->PrintArea.h_max = pageBuffer->PrintArea.h;
	if (pageBuffer->PrintArea.h_max > PAGE_BUFFER_MAX_HEIGHT)
		pageBuffer->PrintArea.h_max = PAGE_BUFFER_MAX_HEIGHT;

	to_cairo_endian(data, DOTS_PER_LINE, pageBuffer->PrintArea.h_max, stride);

	if (Settings.BitsPerDot == 0) {
		bytes = BYTES_PER_LINE;
		type = PRN_DATA_MONO;
	}
	else {
		bytes = DOTS_PER_LINE;
		type = PRN_DATA_GRAYSCALE;
	}
	dotline = (uint8_t *)malloc(bytes);
	memset(dotline, 0, bytes);
	for (row = 0; row < pageBuffer->PrintArea.h_max; row++) {
		memcpy(dotline, data, bytes);
		if (PrnDotLine(data, bytes, type) == PRN_BUF_FULL)
			RUNTIME_FLAG_SET(RTF_KERN_BUF_FULL);
		data += stride;
	}
	free(dotline);
}

static void clear(void)
{
	if (!pageBuffer->Cr)
		return;

	(*cairo_save_func)(pageBuffer->Cr);
	(*cairo_identity_matrix_func)(pageBuffer->Cr);
	(*cairo_set_operator_func)(pageBuffer->Cr, CAIRO_OPERATOR_CLEAR);
	(*cairo_paint_func)(pageBuffer->Cr);
	(*cairo_restore_func)(pageBuffer->Cr);

	(*cairo_save_func)(pageBuffer->CanvasCr);
	(*cairo_identity_matrix_func)(pageBuffer->CanvasCr);
	(*cairo_rectangle_func)(pageBuffer->CanvasCr,
		pageBuffer->PrintArea.x, pageBuffer->PrintArea.y,
		pageBuffer->PrintArea.w, pageBuffer->PrintArea.h);
	(*cairo_set_operator_func)(pageBuffer->CanvasCr, CAIRO_OPERATOR_CLEAR);
	(*cairo_fill_func)(pageBuffer->CanvasCr);
	(*cairo_restore_func)(pageBuffer->CanvasCr);
}

static void setPrintArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	cairo_status_t status;
	uint16_t h_max;

	if (x >= DOTS_PER_LINE || y >= PAGE_BUFFER_MAX_HEIGHT)
		return;
	if (w == 0 || h == 0)
		return;
	if (x + w > DOTS_PER_LINE)
		w = DOTS_PER_LINE - x;
	if (y + h > PAGE_BUFFER_MAX_HEIGHT)
		h = PAGE_BUFFER_MAX_HEIGHT - y;
	h_max = y + h;

	if (pageBuffer->Cr) {
		paint_surface(pageBuffer->CanvasCr, pageBuffer->Surface,
			pageBuffer->PrintArea.x, pageBuffer->PrintArea.y);

		destroy_cr(&pageBuffer->Cr);
		destroy_surface(&pageBuffer->Surface);

		surface = (*cairo_image_surface_create_func)(CAIRO_FORMAT, w, h);
		status = (*cairo_surface_status_func)(surface);
		if (status != CAIRO_STATUS_SUCCESS) {
			LogError("status=%d", (int)status);
			pageBuffer->destroy();
			return;
		}
		pageBuffer->Surface = surface;

		cr = (*cairo_create_func)(surface);
		status = (*cairo_status_func)(cr);
		if (status != CAIRO_STATUS_SUCCESS) {
			LogError("status=%d", (int)status);
			pageBuffer->destroy();
			return;
		}
		pageBuffer->Cr = cr;

		if (h_max > pageBuffer->PrintArea.h_max)
			pageBuffer->PrintArea.h_max = h_max;
	}

	pageBuffer->PrintArea.x = x;
	pageBuffer->PrintArea.y = y;
	pageBuffer->PrintArea.w = w;
	pageBuffer->PrintArea.h = h;

	pageBuffer->setDirection(pageBuffer->Direction);

	LogInfo("x=%u, y=%u, w=%u, h=%u, h_max=%u, dir=%u",
		pageBuffer->PrintArea.x, pageBuffer->PrintArea.y,
		pageBuffer->PrintArea.w, pageBuffer->PrintArea.h,
		pageBuffer->PrintArea.h_max, pageBuffer->Direction);
}

static void setDirection(uint8_t dir)
{
	if (pageBuffer->Cr) {
		(*cairo_identity_matrix_func)(pageBuffer->Cr);
		switch (dir) {
		case 1:
			(*cairo_translate_func)(pageBuffer->Cr, 0, pageBuffer->PrintArea.h);
			(*cairo_rotate_func)(pageBuffer->Cr, -M_PI_2);
			break;
		case 2:
			(*cairo_translate_func)(pageBuffer->Cr, pageBuffer->PrintArea.w, pageBuffer->PrintArea.h);
			(*cairo_rotate_func)(pageBuffer->Cr, M_PI);
			break;
		case 3:
			(*cairo_translate_func)(pageBuffer->Cr, pageBuffer->PrintArea.w, 0);
			(*cairo_rotate_func)(pageBuffer->Cr, M_PI_2);
			break;
		default:
			break;
		}
	}
	pageBuffer->Direction = dir;
}

static void resetPrintArea(void)
{
	pageBuffer->PrintArea.x = 0;
	pageBuffer->PrintArea.y = 0;
	pageBuffer->PrintArea.w = DOTS_PER_LINE;
	pageBuffer->PrintArea.h = 1600;
	pageBuffer->PrintArea.h_max = 0;
}

static void resetDirection(void)
{
	pageBuffer->Direction = 0;
}

static void feed(int dots)
{
	if (pageBuffer->Cr)
		(*cairo_translate_func)(pageBuffer->Cr, 0, dots);
}

static void paint(uint8_t *data, uint32_t width, uint32_t height, uint32_t stride, bool convert)
{
	cairo_surface_t *surface;
	cairo_status_t status;

	if (!pageBuffer->Cr)
		return;

	if (convert)
		to_cairo_endian(data, width, height, stride);

	surface = (*cairo_image_surface_create_for_data_func)(
					data, CAIRO_FORMAT, width, height, stride);
	status = (*cairo_surface_status_func)(surface);
	if (status != CAIRO_STATUS_SUCCESS)
		return;

	paint_surface(pageBuffer->Cr, surface, 0, 0);

	(*cairo_surface_destroy_func)(surface);
}

static uint16_t widthOfTextDir(void)
{
	return (pageBuffer->Direction == 1 || pageBuffer->Direction == 3) ? pageBuffer->PrintArea.h : pageBuffer->PrintArea.w;
}

static uint16_t heightOfTextDir(void)
{
	return (pageBuffer->Direction == 1 || pageBuffer->Direction == 3) ? pageBuffer->PrintArea.w : pageBuffer->PrintArea.h;
}

static PageBuffer theBuffer = {
	.init = init,
	.create = create,
	.destroy = destroy,
	.print = print,
	.clear = clear,
	.setPrintArea = setPrintArea,
	.setDirection = setDirection,
	.resetPrintArea = resetPrintArea,
	.resetDirection = resetDirection,
	.feed = feed,
	.paint = paint,
	.widthOfTextDir = widthOfTextDir,
	.heightOfTextDir = heightOfTextDir
};

void to_cairo_endian(uint8_t *data, uint32_t width, uint32_t height, uint32_t stride)
{
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
	uint32_t i, j, bytes, words, *d;

	if (Settings.BitsPerDot > 0)
		return;
	if (width == 0 || height == 0)
		return;

	bytes = ((width - 1) >> 3) + 1;
	words = ((width - 1) >> 5) + 1;

	for (i = 0; i < height; i++) {
		d = (uint32_t *)data;
		for (j = 0; j < words; j++) {
			(*d) = htonl(*d);
			d++;
		}
		for (j = 0; j < bytes; j++) {
			data[j] = table[data[j]];
		}
		data += stride;
	}
}

PageBuffer *pageBuffer = &theBuffer;
