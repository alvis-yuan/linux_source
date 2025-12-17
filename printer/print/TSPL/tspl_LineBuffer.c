#include <arpa/inet.h>
#include <string.h>
#include <cairo.h>
#include "tspl_HarfBuzz.h"
#include "tspl_util.h"
#include "tspl_LineBuffer.h"
#include "tspl_PageBuffer.h"
#include "tspl_config.h"

static uint32_t size_of_data;

static int init(void)
{
	size_of_data = TOTAL_LINE_HEIGHT * pageBuffer->WORDS_PER_LINE * sizeof(uint32_t);
	tspl_lineBuffer->Data = (uint32_t *)calloc(TOTAL_LINE_HEIGHT * pageBuffer->WORDS_PER_LINE, sizeof(uint32_t));
	tspl_lineBuffer->Underlines = (uint32_t *)calloc(2 * pageBuffer->WORDS_PER_LINE, sizeof(uint32_t));
	return (tspl_lineBuffer->Data && tspl_lineBuffer->Underlines) ? 0 : -1;
}

static void clear(void)
{
	memset(tspl_lineBuffer->Data, 0, size_of_data);
	memset(tspl_lineBuffer->Underlines, 0, 2 * pageBuffer->WORDS_PER_LINE * sizeof(uint32_t));
	tspl_lineBuffer->EndPos = 0;
	tspl_lineBuffer->Height = 0;
	tspl_lineBuffer->Top = TOTAL_LINE_HEIGHT;
	tspl_lineBuffer->Bottom = 0;
	tspl_lineBuffer->UnderlineCount = 0;
	tspl_lineBuffer->CursorPos = 0;
}

static int appendHarfBuzzCanvas(uint32_t x1, uint32_t x2, int cluster_y[2])
{
	uint32_t x, y, lm, em, pos, width, start, top, bottom;
	uint32_t *dotline;
	uint8_t mask;

	lm = 0;
	em = pageBuffer->DOTS_PER_LINE - pageBuffer->TextLeftMargin;

	if (tspl_lineBuffer->EndPos < lm)
		tspl_lineBuffer->EndPos = lm;

	width = x2 - x1;
	if ((tspl_lineBuffer->EndPos + width) > em) {
		if (tspl_lineBuffer->EndPos > lm)
			return 1;
		if (width > pageBuffer->DOTS_PER_LINE)
			return -1;
		if (tspl_lineBuffer->EndPos == 0) {
			LogError("x is set too large to hold a single word");
			return -2;
		}
		if ((pageBuffer->DOTS_PER_LINE - lm) < width)
			tspl_lineBuffer->EndPos = (pageBuffer->DOTS_PER_LINE - width);
	}

	if (tspl_lineBuffer->Flags & LINE_BUFFER_FLAG_LINE_RTL) {
		top = tspl_lineBuffer->firstLine();
		bottom = tspl_lineBuffer->lastLine();
		for (y = top; y <= bottom; y++)
			Dotline_Rshift_util(tspl_lineBuffer->dotlineAt(y), pageBuffer->WORDS_PER_LINE, 0, width);
		start = lm;
	}
	else {
		start = tspl_lineBuffer->EndPos;
	}

	for (y = cluster_y[0]; y <= cluster_y[1]; y++) {
		pos = start;
		dotline = tspl_lineBuffer->dotlineAt(y);
		mask = (1 << (x1 & 7));
		for (x = x1; x < x2; x++) {
			if (tspl_harfBuzz->Canvas[y][x >> 3] & mask) {
				Dotline_CopyU32_CairoFormat(dotline, pos,
					PointStretchWidth_littleEdian_util[1], PointStretchWidth_littleEdian_util[1], true);
			}
			pos++;
			mask <<= 1;
			if (mask == 0)
				mask = 1;
		}
	}
	tspl_lineBuffer->EndPos += width;
	tspl_lineBuffer->updateHeight(cluster_y[0], cluster_y[1]);

	return 0;
}

static void setHeight(uint32_t height)
{
	tspl_lineBuffer->Top = MAX_LINE_HEIGHT - height;
	tspl_lineBuffer->Bottom = BASE_LINE;
	tspl_lineBuffer->Height = height;
}

static void updateHeight(uint32_t top, uint32_t bottom)
{
	if (tspl_lineBuffer->Top > top)
		tspl_lineBuffer->Top = top;
	if (tspl_lineBuffer->Bottom < bottom)
		tspl_lineBuffer->Bottom = bottom;
	tspl_lineBuffer->Height = tspl_lineBuffer->Bottom - tspl_lineBuffer->Top + 1;
}

static void setFlag(uint32_t flag)
{
	tspl_lineBuffer->Flags |= flag;
}

static void clearFlag(uint32_t flag)
{
	tspl_lineBuffer->Flags &= ~flag;
}

static void print(uint32_t dotsToFeed)
{
	uint32_t lineCount, lines;
	uint8_t *data;

	if (tspl_lineBuffer->Height > 0) {
		for (lines = 0; lines < tspl_lineBuffer->UnderlineCount; lines++)
			Dotline_Or_util(tspl_lineBuffer->dotlineAt(MAX_LINE_HEIGHT + lines),
				tspl_lineBuffer->Underlines + lines * pageBuffer->WORDS_PER_LINE);
		tspl_lineBuffer->updateHeight(tspl_lineBuffer->Top, BASE_LINE + tspl_lineBuffer->UnderlineCount);
	}

	dotsToFeed = (dotsToFeed > tspl_lineBuffer->Height) ? (dotsToFeed - tspl_lineBuffer->Height) : 0;

	if (tspl_lineBuffer->Height > 0) {
		lineCount = tspl_lineBuffer->Height;
		data = (uint8_t *)tspl_lineBuffer->firstDotline();


		CAIRO_ENTER_CRITICAL();

		int _stride = cairo_format_stride_for_width(CAIRO_FORMAT_A1, pageBuffer->DOTS_PER_LINE);
		cairo_surface_t *surface =
			cairo_image_surface_create_for_data(data, CAIRO_FORMAT_A1, pageBuffer->DOTS_PER_LINE, lineCount, _stride);
		cairo_pattern_t *pattern = cairo_pattern_create_for_surface(surface);

		cairo_translate(pageBuffer->cr, 0, pageBuffer->TextLineIndexHeight);
		cairo_set_source(pageBuffer->cr, pattern);
		cairo_paint(pageBuffer->cr);
		
		cairo_pattern_destroy(pattern);
		cairo_surface_destroy(surface);

		pageBuffer->TextLineIndexHeight += lineCount + dotsToFeed;
		CAIRO_EXIT_CRITICAL();
	}

	tspl_lineBuffer->clear();
}

static void printLine(void)
{
	print(pageBuffer->LineSpace);
}


static uint32_t *firstDotline(void)
{
	return tspl_lineBuffer->dotlineAt(tspl_lineBuffer->firstLine());
}

static uint32_t *dotlineAt(uint32_t row)
{
	return tspl_lineBuffer->Data + row * pageBuffer->WORDS_PER_LINE;
}

static uint32_t firstLine(void)
{
	if (tspl_lineBuffer->Top < TOTAL_LINE_HEIGHT)
		return tspl_lineBuffer->Top;
	return (MAX_LINE_HEIGHT - tspl_lineBuffer->Height);
}

static uint32_t lastLine(void)
{
	if (tspl_lineBuffer->Bottom > 0)
		return tspl_lineBuffer->Bottom;
	return (tspl_lineBuffer->firstLine() + tspl_lineBuffer->Height - 1);
}

static LineBuffer theBuffer = {
	.Flags = 0,
	.init = init,
	.clear = clear,
	.appendHarfBuzzCanvas = appendHarfBuzzCanvas,
	.setHeight = setHeight,
	.updateHeight = updateHeight,
	.setFlag = setFlag,
	.clearFlag = clearFlag,
	.print = print,
	.printLine = printLine,
	.firstDotline = firstDotline,
	.dotlineAt = dotlineAt,
	.firstLine = firstLine,
	.lastLine = lastLine
};

LineBuffer *tspl_lineBuffer = &theBuffer;
