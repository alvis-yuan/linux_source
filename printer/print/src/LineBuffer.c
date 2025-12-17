#include <arpa/inet.h>
#include <string.h>
#include "libcommon.h"
#include "printer.h"
#include "HarfBuzz.h"
#include "PageBuffer.h"
#include "Runtime_Data.h"
#include "Util.h"
#include "LineBuffer.h"

static const uint32_t PointStretchWidth[32] = {
	0x00000000,
	0x80000000,
	0xC0000000,
	0xE0000000,
	0xF0000000,
	0xF8000000,
	0xFC000000,
	0xFE000000,
	0xFF000000,
	0xFF800000,
	0xFFC00000,
	0xFFE00000,
	0xFFF00000,
	0xFFF80000,
	0xFFFC0000,
	0xFFFE0000,
	0xFFFF0000,
	0xFFFF8000,
	0xFFFFC000,
	0xFFFFE000,
	0xFFFFF000,
	0xFFFFF800,
	0xFFFFFC00,
	0xFFFFFE00,
	0xFFFFFF00,
	0xFFFFFF80,
	0xFFFFFFC0,
	0xFFFFFFE0,
	0xFFFFFFF0,
	0xFFFFFFF8,
	0xFFFFFFFC,
	0xFFFFFFFE
};

static uint32_t bytes_per_line[4], words_per_line[4];
static uint32_t dots_per_line_vertical, size_of_data, size_of_underlines;
static int line_stride_index = 0;

#define LINE_STRIDE_BYTES (bytes_per_line[line_stride_index])
#define LINE_STRIDE_WORDS (words_per_line[line_stride_index])

static void RightShift(uint32_t startPos, uint32_t dotsToShift)
{
	uint32_t y, top, bottom, size;
	uint8_t *p;

	top = lineBuffer->firstLine();
	bottom = lineBuffer->lastLine();
	if (Settings.BitsPerDot == 0) {
		for (y = top; y <= bottom; y++)
			Dotline_Rshift(lineBuffer->dotlineAt(y), WORDS_PER_LINE, startPos, dotsToShift);
	}
	else {
		for (y = top; y <= bottom; y++) {
			if (startPos >= DOTS_PER_LINE)
				return;
			if (dotsToShift == 0)
				return;
			if (startPos + dotsToShift > DOTS_PER_LINE)
				dotsToShift = DOTS_PER_LINE - startPos;
			p = (uint8_t *)lineBuffer->dotlineAt(y) + startPos;
			size = DOTS_PER_LINE - startPos - dotsToShift;
			if (size > 0)
				memmove(p + dotsToShift, p, size);
			memset(p, 0, dotsToShift);
		}
	}
}

static uint8_t BitInverse(uint8_t v)
{
	uint8_t i, r = 0, mask_v = 0x01, mask_r = 0x80;

	if (v == 0x00 || v == 0xFF)
		return v;

	for (i = 0; i < 8; i++) {
		if (v & mask_v)
			r |= mask_r;
		mask_v <<= 1;
		mask_r >>= 1;
	}
	return r;
}

static void UpsideDown(void)
{
	uint32_t i, j, bytes;
	uint8_t *buf, tmp;

	if (Settings.BitsPerDot == 0) {
		bytes = BYTES_PER_LINE * lineBuffer->Height;
		buf = (uint8_t *)lineBuffer->firstDotline();
		for (i = 0, j = bytes - 1; i < (bytes >> 1); i++, j--) {
			tmp = buf[i];
			buf[i] = BitInverse(buf[j]);
			buf[j] = BitInverse(tmp);
		}
	}
	else {
		bytes = DOTS_PER_LINE * lineBuffer->Height;
		buf = (uint8_t *)lineBuffer->firstDotline();
		for (i = 0, j = bytes - 1; i < (bytes >> 1); i++, j--) {
			tmp = buf[i];
			buf[i] = buf[j];
			buf[j] = tmp;
		}
	}
}

static void PrintLineSpacing(uint32_t lineSpacing)
{
	if (lineSpacing == 0)
		return;

	if (Settings.Mode == STANDARD_MODE) {
		if (PrnBlankLines(lineSpacing) == PRN_BUF_FULL)
			RUNTIME_FLAG_SET(RTF_KERN_BUF_FULL);
	}
	else if (Settings.Mode == PAGE_MODE) {
		pageBuffer->feed(lineSpacing);
	}
}

static void FillBlack(uint32_t dots, uint32_t height)
{
	uint32_t d, p, row;

	for (row = MAX_LINE_HEIGHT - height; row < MAX_LINE_HEIGHT; row++) {
		d = dots;
		p = lineBuffer->EndPos;
		while (d >= 32) {
			Dotline_CopyU32(lineBuffer->dotlineAt(row), p,
				0xFFFFFFFF, 0xFFFFFFFF, false);
			d -= 32;
			p += 32;
		}
		if (d > 0) {
			Dotline_CopyU32(lineBuffer->dotlineAt(row), p,
				PointStretchWidth[d], PointStretchWidth[d], false);
		}
	}
}

static void DrawUnderline(uint32_t pos, uint32_t dots, uint32_t unl)
{
	uint32_t d, p, row;

	if ((unl & 0xF0) && !Settings.isClockwiseRotation() && !Settings.BlackWhiteReverseMode) {
		for (row = 0; (unl & 0x03) != 0; row++) {
			d = dots;
			p = pos;
			while (d >= 32) {
				Dotline_SetDots(lineBuffer->Underlines + row * LINE_STRIDE_WORDS,
					p, 0xFFFFFFFF, 0xFFFFFFFF, false);
				d -= 32;
				p += 32;
			}
			if (d > 0) {
				Dotline_SetDots(lineBuffer->Underlines + row * LINE_STRIDE_WORDS,
					p, PointStretchWidth[d], PointStretchWidth[d], false);
			}
			unl &= ~(1 << row);
		}
		lineBuffer->UnderlineCount = row;
	}
}

/*
 * Draw left or right character spacing.
 */
static void DrawCharSpacing(uint32_t dots, uint32_t height, uint32_t unl)
{
	if (dots > 0) {
		DrawUnderline(lineBuffer->EndPos, dots, unl);
//		if (Settings.BlackWhiteReverseMode)
//			FillBlack(dots, height);
		lineBuffer->EndPos += dots;
	}
}

static void Align(void)
{
	uint32_t shiftDots;

	if (lineBuffer->EndPos <= Settings.leftMargin())
		return;

	if (lineBuffer->EndPos >= Settings.rightMargin())
		return;

	if (Settings.Alignment == 1) /* Align center */
		shiftDots = (Settings.rightMargin() - lineBuffer->EndPos) >> 1;
	else if (Settings.Alignment == 2) /* Align right */
		shiftDots = (Settings.rightMargin() - lineBuffer->EndPos);
	else
		return;

	RightShift(0, shiftDots);
}

static void BlackWhiteReverse(void)
{
	uint32_t x, x1, x2, y, mask1, mask2, top, bottom, diff;

	if (lineBuffer->BWReverseRange[0] >= lineBuffer->BWReverseRange[1])
		goto _exit;

	top = lineBuffer->firstLine();
	bottom = lineBuffer->lastLine();

	if (Settings.BitsPerDot == 0) {
		uint32_t *dotline;

		x1 = (lineBuffer->BWReverseRange[0]    ) >> 5;
		x2 = (lineBuffer->BWReverseRange[1] - 1) >> 5;
		diff = x2 - x1;

		if (diff == 0) {
			mask1 = 0;
			for (x = (lineBuffer->BWReverseRange[0] & 0x1F);
				x <= ((lineBuffer->BWReverseRange[1] - 1) & 0x1F);
				x++)
				mask1 |= 1 << (31 - x);
			for (y = top; y <= bottom; y++) {
				dotline = lineBuffer->dotlineAt(y);
				dotline[x1] ^= mask1;
			}
		}
		else {
			mask1 = 0;
			for (x = (lineBuffer->BWReverseRange[0] & 0x1F); x <= 31; x++)
				mask1 |= 1 << (31 - x);
			mask2 = 0;
			for (x = 0; x <= ((lineBuffer->BWReverseRange[1] - 1) & 0x1F); x++)
				mask2 |= 1 << (31 - x);
			for (y = top; y <= bottom; y++) {
				dotline = lineBuffer->dotlineAt(y);
				dotline[x1] ^= mask1;
				dotline[x2] ^= mask2;
				for (x = x1 + 1; x < x2; x++)
					dotline[x] ^= 0xFFFFFFFF;
			}
		}
	}
	else {
		uint8_t *dotline;

		for (y = top; y <= bottom; y++) {
			dotline = (uint8_t *)lineBuffer->dotlineAt(y);
			for (x = lineBuffer->BWReverseRange[0]; x < lineBuffer->BWReverseRange[1]; x++)
				dotline[x] = (255 - dotline[x]) >> Settings.Color;
		}
	}

_exit:
	lineBuffer->BWReverseRange[0] = 65535;
	lineBuffer->BWReverseRange[1] = 0;
}

static int init(void)
{
	dots_per_line_vertical = (PAGE_BUFFER_MAX_HEIGHT > DOTS_PER_LINE) ? PAGE_BUFFER_MAX_HEIGHT : DOTS_PER_LINE;
	/*
	 *    BitsPerDot Page Mode
	 *    ---------- ---------
	 * [0]         0         N
	 * [1]         0         Y
	 * [2]        >0         N
	 * [3]        >0         Y
	 */
	bytes_per_line[0] = BYTES_PER_LINE;
	words_per_line[0] = WORDS_PER_LINE;
	bytes_per_line[1] = dots_per_line_vertical >> 3;
	words_per_line[1] = bytes_per_line[1] / sizeof(uint32_t);
	bytes_per_line[2] = bytes_per_line[0] << 3;
	words_per_line[2] = words_per_line[0] << 3;
	bytes_per_line[3] = bytes_per_line[1] << 3;
	words_per_line[3] = words_per_line[1] << 3;
	/* Allocate 1 byte for 1 dot for grayscale printing */
	size_of_data = TOTAL_LINE_HEIGHT * dots_per_line_vertical;
	size_of_underlines = 2 * dots_per_line_vertical;
	lineBuffer->Data = (uint32_t *)calloc(size_of_data / sizeof(uint32_t), sizeof(uint32_t));
	lineBuffer->Underlines = (uint32_t *)calloc(size_of_underlines / sizeof(uint32_t), sizeof(uint32_t));
	return (lineBuffer->Data && lineBuffer->Underlines) ? 0 : -1;
}

static int isEmpty(void)
{
	return (lineBuffer->EndPos <= Settings.leftMargin()) ? 1 : 0;
}

static void clear(void)
{
	memset(lineBuffer->Data, 0, size_of_data);
	memset(lineBuffer->Underlines, 0, size_of_underlines);
	lineBuffer->EndPos = Settings.leftMargin();
	lineBuffer->Height = 0;
	lineBuffer->Top = TOTAL_LINE_HEIGHT;
	lineBuffer->Bottom = 0;
	lineBuffer->UnderlineCount = 0;
	lineBuffer->CursorPos = 0;
	lineBuffer->BWReverseRange[0] = 65535;
	lineBuffer->BWReverseRange[1] = 0;
}

static int append(CharBuffer *cbuf)
{
	uint64_t MarkFlag64;
	uint32_t y, lm, lsp, rsp, hsize, vsize, unl;
	uint32_t vIndex, hIndex, vN, MarkFlag;
	uint32_t lineIndex, dots, pos, width, top, bottom;

	if (cbuf->Width == 0 || cbuf->Width > 64)
		return -1;
	if (cbuf->Height == 0 || cbuf->Height > 64)
		return -1;

	lm    = Settings.leftMargin();
	lsp   = Settings.LeftCharSpacing[cbuf->CharType];
	rsp   = Settings.rightCharSpacing(cbuf->CharType);
	hsize = Settings.CharHSize[cbuf->CharType];
	vsize = Settings.CharVSize[cbuf->CharType];
	unl   = Settings.UnderlineMode[cbuf->CharType];

	if (lineBuffer->EndPos < lm)
		lineBuffer->EndPos = lm;

	if (cbuf->Left < 0) {
		width = (-cbuf->Left) * hsize;
		if (lineBuffer->EndPos < (lm + width))
			return -1;
	}
	else {
		width = (cbuf->AdvanceX + lsp) * hsize;
		if ((lineBuffer->EndPos + width) > Settings.rightMargin()) {
			if (lineBuffer->EndPos > lm)
				return 1;
			if (width > DOTS_PER_LINE)
				return -1;
			if ((DOTS_PER_LINE - lm) < width)
				lineBuffer->EndPos = (DOTS_PER_LINE - width);
		}
	}

	if (lineBuffer->Flags & LINE_BUFFER_FLAG_LINE_RTL) {
		if (lineBuffer->CursorPos == 0)
			lineBuffer->CursorPos = lm;
		RightShift(lineBuffer->CursorPos, width);
		if (lineBuffer->BWReverseRange[0] < lineBuffer->BWReverseRange[1]) {
			lineBuffer->BWReverseRange[0] += width;
			lineBuffer->BWReverseRange[1] += width;
		}
	}
	else {
		lineBuffer->CursorPos = lineBuffer->EndPos;
	}

	if (Settings.BlackWhiteReverseMode) {
		if (lineBuffer->CursorPos < lineBuffer->BWReverseRange[0])
			lineBuffer->BWReverseRange[0] = lineBuffer->CursorPos;
		if (lineBuffer->CursorPos + width > lineBuffer->BWReverseRange[1])
			lineBuffer->BWReverseRange[1] = lineBuffer->CursorPos + width;
	}
	else {
		BlackWhiteReverse();
	}

	/*
	 * Draw left character spacing.
	 */
	if (cbuf->Left >= 0)
		DrawCharSpacing(lsp * hsize, cbuf->Height * vsize, unl);

	/*
	 * Draw character.
	 */
	if (cbuf->Left >= 0)
		pos = lineBuffer->CursorPos + cbuf->Left;
	else
		pos = lineBuffer->CursorPos + cbuf->Left * hsize;

	if (!cbuf->UseData64) {
		MarkFlag = 0x80000000;
		top = MAX_LINE_HEIGHT - (24 - cbuf->Top) * vsize;
		for (hIndex = 0; hIndex < cbuf->Width; hIndex++) {
			lineIndex = top;
			for (vIndex = 0, y = cbuf->Top; vIndex < cbuf->Height; vIndex++, y++) {
				if (cbuf->Data[y] & MarkFlag) {
					for (vN = 0; vN < vsize; vN++) {
						Dotline_SetDots(lineBuffer->dotlineAt(lineIndex), pos,
							PointStretchWidth[hsize], PointStretchWidth[hsize], true);
						lineIndex++;
					}
				}
				else {
					lineIndex += vsize;
				}
			}
			MarkFlag >>= 1;
			if (cbuf->Left >= 0)
				DrawUnderline(pos, hsize, unl);
			pos += hsize;
		}
	}
	else {
		MarkFlag64 = 0x8000000000000000ULL;
		top = MAX_LINE_HEIGHT - (48 - cbuf->Top) * vsize;
		for (hIndex = 0; hIndex < cbuf->Width; hIndex++) {
			lineIndex = top;
			for (vIndex = 0, y = cbuf->Top; vIndex < cbuf->Height; vIndex++, y++) {
				if (cbuf->Data64[y] & MarkFlag64) {
					for (vN = 0; vN < vsize; vN++) {
						Dotline_SetDots(lineBuffer->dotlineAt(lineIndex), pos,
							PointStretchWidth[hsize], PointStretchWidth[hsize], true);
						lineIndex++;
					}
				}
				else {
					lineIndex += vsize;
				}
			}
			MarkFlag64 >>= 1;
			if (cbuf->Left >= 0)
				DrawUnderline(pos, hsize, unl);
			pos += hsize;
		}
	}

//	if (cbuf->Left >= 0)
//		lineBuffer->EndPos = pos;
	width = (cbuf->AdvanceX + lsp) * hsize;
	lineBuffer->CursorPos += width;
	lineBuffer->EndPos += width;

	/*
	 * Draw right character spacing.
	 */
	if (cbuf->Left >= 0 && rsp > 0) {
		dots = rsp * hsize;
		if (lineBuffer->EndPos + dots > Settings.rightMargin())
			dots = Settings.rightMargin() - lineBuffer->EndPos;
		DrawCharSpacing(dots, cbuf->Height * vsize, unl);
	}

	/*
	 * Update height of the line buffer.
	 */
	dots = cbuf->Height * vsize;
	bottom = top + dots - 1;
	lineBuffer->updateHeight(top, bottom);

	return 0;
}

static int _appendHarfBuzzCanvas(uint32_t x1, uint32_t x2)
{
	uint32_t x, y, lm, pos, width;
	uint32_t *dotline;
	uint8_t mask;

	lm = Settings.leftMargin();

	if (lineBuffer->EndPos < lm)
		lineBuffer->EndPos = lm;

	width = x2 - x1;
	if ((lineBuffer->EndPos + width) > Settings.rightMargin()) {
		if (lineBuffer->EndPos > lm)
			return 1;
		if (width > DOTS_PER_LINE)
			return -1;
		if ((DOTS_PER_LINE - lm) < width)
			lineBuffer->EndPos = (DOTS_PER_LINE - width);
	}

	if (lineBuffer->Flags & LINE_BUFFER_FLAG_LINE_RTL) {
		lineBuffer->CursorPos = lm;
		if (lineBuffer->CursorPos < lineBuffer->EndPos) {
			RightShift(lineBuffer->CursorPos, width);
			if (lineBuffer->CursorPos <= lineBuffer->BWReverseRange[0] &&
				lineBuffer->BWReverseRange[0] < lineBuffer->BWReverseRange[1]) {
				lineBuffer->BWReverseRange[0] += width;
				lineBuffer->BWReverseRange[1] += width;
			}
		}
	}
	else {
		if (lineBuffer->Flags & LINE_BUFFER_FLAG_CHAR_RTL) {
			if (lineBuffer->CursorPos == 0)
				lineBuffer->CursorPos = lm;
			if (lineBuffer->CursorPos < lineBuffer->EndPos) {
				RightShift(lineBuffer->CursorPos, width);
				if (lineBuffer->CursorPos <= lineBuffer->BWReverseRange[0] &&
					lineBuffer->BWReverseRange[0] < lineBuffer->BWReverseRange[1]) {
					lineBuffer->BWReverseRange[0] += width;
					lineBuffer->BWReverseRange[1] += width;
				}
			}
		}
		else {
			lineBuffer->CursorPos = lineBuffer->EndPos;
		}
	}

	if (Settings.BlackWhiteReverseMode) {
		if (lineBuffer->CursorPos < lineBuffer->BWReverseRange[0])
			lineBuffer->BWReverseRange[0] = lineBuffer->CursorPos;
		if (lineBuffer->CursorPos + width > lineBuffer->BWReverseRange[1])
			lineBuffer->BWReverseRange[1] = lineBuffer->CursorPos + width;
	}
	else {
		BlackWhiteReverse();
	}

	for (y = harfBuzz->CanvasBBox.y_min; y <= harfBuzz->CanvasBBox.y_max; y++) {
		pos = lineBuffer->CursorPos;
		dotline = lineBuffer->dotlineAt(y);
		mask = (1 << (x1 & 7));
		for (x = x1; x < x2; x++) {
			if (harfBuzz->Canvas[y][x >> 3] & mask) {
				Dotline_CopyU32(dotline, pos,
					PointStretchWidth[1], PointStretchWidth[1], true);
			}
			pos++;
			mask <<= 1;
			if (mask == 0)
				mask = 1;
		}
	}
	DrawUnderline(lineBuffer->CursorPos, width, Settings.UnderlineMode[CHAR_TYPE_CJK]);
	if (lineBuffer->Flags & LINE_BUFFER_FLAG_CHAR_RTL) {
		;
	}
	else {
		lineBuffer->CursorPos += width;
	}
	lineBuffer->EndPos += width;
	lineBuffer->updateHeight(harfBuzz->CanvasBBox.y_min, harfBuzz->CanvasBBox.y_max);

	return 0;
}

static int _appendHarfBuzzCanvasMbpd(uint32_t x1, uint32_t x2)
{
	uint32_t y, lm, pos, width;
	uint32_t *dotline;

	lm = Settings.leftMargin();

	if (lineBuffer->EndPos < lm)
		lineBuffer->EndPos = lm;

	width = x2 - x1;
	if ((lineBuffer->EndPos + width) > Settings.rightMargin()) {
		if (lineBuffer->EndPos > lm)
			return 1;
		if (width > DOTS_PER_LINE)
			return -1;
		if ((DOTS_PER_LINE - lm) < width)
			lineBuffer->EndPos = (DOTS_PER_LINE - width);
	}

	if (lineBuffer->Flags & LINE_BUFFER_FLAG_LINE_RTL) {
		lineBuffer->CursorPos = lm;
		if (lineBuffer->CursorPos < lineBuffer->EndPos) {
			RightShift(lineBuffer->CursorPos, width);
			if (lineBuffer->CursorPos <= lineBuffer->BWReverseRange[0] &&
				lineBuffer->BWReverseRange[0] < lineBuffer->BWReverseRange[1]) {
				lineBuffer->BWReverseRange[0] += width;
				lineBuffer->BWReverseRange[1] += width;
			}
		}
	}
	else {
		if (lineBuffer->Flags & LINE_BUFFER_FLAG_CHAR_RTL) {
			if (lineBuffer->CursorPos == 0)
				lineBuffer->CursorPos = lm;
			if (lineBuffer->CursorPos < lineBuffer->EndPos) {
				RightShift(lineBuffer->CursorPos, width);
				if (lineBuffer->CursorPos <= lineBuffer->BWReverseRange[0] &&
					lineBuffer->BWReverseRange[0] < lineBuffer->BWReverseRange[1]) {
					lineBuffer->BWReverseRange[0] += width;
					lineBuffer->BWReverseRange[1] += width;
				}
			}
		}
		else {
			lineBuffer->CursorPos = lineBuffer->EndPos;
		}
	}

	if (Settings.BlackWhiteReverseMode) {
		if (lineBuffer->CursorPos < lineBuffer->BWReverseRange[0])
			lineBuffer->BWReverseRange[0] = lineBuffer->CursorPos;
		if (lineBuffer->CursorPos + width > lineBuffer->BWReverseRange[1])
			lineBuffer->BWReverseRange[1] = lineBuffer->CursorPos + width;
	}
	else {
		BlackWhiteReverse();
	}

	for (y = harfBuzz->CanvasBBox.y_min; y <= harfBuzz->CanvasBBox.y_max; y++) {
		pos = lineBuffer->CursorPos;
		dotline = lineBuffer->dotlineAt(y);
		memcpy((uint8_t *)dotline + pos, &harfBuzz->Canvas[y][x1], width);
		pos += width;
	}
	DrawUnderline(lineBuffer->CursorPos, width, Settings.UnderlineMode[CHAR_TYPE_CJK]);
	if (lineBuffer->Flags & LINE_BUFFER_FLAG_CHAR_RTL) {
		;
	}
	else {
		lineBuffer->CursorPos += width;
	}
	lineBuffer->EndPos += width;
	lineBuffer->updateHeight(harfBuzz->CanvasBBox.y_min, harfBuzz->CanvasBBox.y_max);

	return 0;
}

static int appendHarfBuzzCanvas(uint32_t x1, uint32_t x2)
{
	if (Settings.BitsPerDot == 0)
		return _appendHarfBuzzCanvas(x1, x2);
	return _appendHarfBuzzCanvasMbpd(x1, x2);
}

static void setHeight(uint32_t height)
{
	lineBuffer->Top = MAX_LINE_HEIGHT - height;
	lineBuffer->Bottom = BASE_LINE;
	lineBuffer->Height = height;
}

static void updateHeight(uint32_t top, uint32_t bottom)
{
	if (lineBuffer->Top > top)
		lineBuffer->Top = top;
	if (lineBuffer->Bottom < bottom)
		lineBuffer->Bottom = bottom;
	lineBuffer->Height = lineBuffer->Bottom - lineBuffer->Top + 1;
}

static void setFlag(uint32_t flag)
{
	lineBuffer->Flags |= flag;
}

static void clearFlag(uint32_t flag)
{
	lineBuffer->Flags &= ~flag;
}

static void print(uint32_t dotsToFeed)
{
	uint32_t lineCount, lines, bytes;
	uint8_t *data;

	BlackWhiteReverse();

	if (lineBuffer->Height > 0 && lineBuffer->UnderlineCount > 0) {
		for (lines = 0; lines < lineBuffer->UnderlineCount; lines++)
			Dotline_Or(lineBuffer->dotlineAt(MAX_LINE_HEIGHT + lines),
				lineBuffer->Underlines + lines * LINE_STRIDE_WORDS, LINE_STRIDE_WORDS);
		lineBuffer->updateHeight(lineBuffer->Top, BASE_LINE + lineBuffer->UnderlineCount);
	}

	dotsToFeed = (dotsToFeed > lineBuffer->Height) ? (dotsToFeed - lineBuffer->Height) : 0;

	if (Settings.isUpsideDown())
		PrintLineSpacing(dotsToFeed);

	if (lineBuffer->Height > 0) {
		Align();

		if (Settings.isUpsideDown())
			UpsideDown();

		if (Settings.Mode == STANDARD_MODE) {
			lineCount = lineBuffer->Height;
			data = (uint8_t *)lineBuffer->firstDotline();
			if (Settings.BitsPerDot == 0) {
				while (lineCount > 0) {
					lines = (lineCount > 8) ? 8 : lineCount;
					bytes = BYTES_PER_LINE * lines;
					if (PrnDotLine(data, bytes, PRN_DATA_MONO) == PRN_BUF_FULL)
						RUNTIME_FLAG_SET(RTF_KERN_BUF_FULL);
					data += bytes;
					lineCount -= lines;
				}
			}
			else {
				while (lineCount > 0) {
					if (PrnDotLine(data, DOTS_PER_LINE, PRN_DATA_GRAYSCALE) == PRN_BUF_FULL)
						RUNTIME_FLAG_SET(RTF_KERN_BUF_FULL);
					data += DOTS_PER_LINE;
					lineCount--;
				}
			}
		}
		else if (Settings.Mode == PAGE_MODE) {
			pageBuffer->paint((uint8_t *)lineBuffer->firstDotline(),
							pageBuffer->widthOfTextDir(), lineBuffer->Height, LINE_STRIDE_BYTES, true);
			pageBuffer->feed(lineBuffer->Height);
		}
	}

	if (!Settings.isUpsideDown())
		PrintLineSpacing(dotsToFeed);

	lineBuffer->clear();
}

/*
 * Print the data in the line buffer and feed one line,
 * based on the current line spacing.
 * After printing, the line buffer is cleared and the
 * print position moves to the beginning of the line.
 * When a left margin is set in standard mode, the print
 * position is set to the left margin.
 */
static void printLine(void)
{
	print(Settings.lineSpacing());
}

static void printRaster(uint32_t height)
{
	uint8_t *data;

	if (height == 0 || lineBuffer->Height != 1)
		return;

	Align();

	if (Settings.isUpsideDown())
		UpsideDown();

	data = (uint8_t *)lineBuffer->firstDotline();

	if (Settings.Mode == STANDARD_MODE) {
		PrnDuplicateLines(data, height);
	}
	else if (Settings.Mode == PAGE_MODE) {
		to_cairo_endian(data, pageBuffer->widthOfTextDir(), 1, LINE_STRIDE_BYTES);
		while (height-- > 0) {
			pageBuffer->paint(data, pageBuffer->widthOfTextDir(), 1, LINE_STRIDE_BYTES, false);
			pageBuffer->feed(1);
		}
	}

	lineBuffer->clear();
}

static void printBarcode(void)
{
	uint32_t i;

	if (Barcode.SymbolWidth == 0)
		return;

	if (!lineBuffer->isEmpty()) /* 打印缓冲不为空，不能打印 */
		return;

	if (Barcode.HRIPos & 0x01) {
		CopyMonoToLineBuffer(lineBuffer->dotlineAt(MAX_LINE_HEIGHT - 26),
			Barcode.Bitmap.HRI, lineBuffer->dotsPerLine(), false, 24);
		for (i = MAX_LINE_HEIGHT - 14; i < MAX_LINE_HEIGHT; i++) {
			CopyMonoToLineBuffer(lineBuffer->dotlineAt(i),
				Barcode.Bitmap.Ext, lineBuffer->dotsPerLine(), true, 1);
		}
		lineBuffer->EndPos += Barcode.SymbolWidth;
		lineBuffer->setHeight(26);
		lineBuffer->print(0);
	}

	CopyMonoToLineBuffer(lineBuffer->dotlineAt(BASE_LINE),
		Barcode.Bitmap.Body, lineBuffer->dotsPerLine(), false, 1);
	lineBuffer->EndPos += Barcode.SymbolWidth;
	lineBuffer->setHeight(1);
	lineBuffer->printRaster(Barcode.Height);

	if (Barcode.HRIPos & 0x02) {
		CopyMonoToLineBuffer(lineBuffer->dotlineAt(MAX_LINE_HEIGHT - 24),
			Barcode.Bitmap.HRI, lineBuffer->dotsPerLine(), false, 24);
		for (i = MAX_LINE_HEIGHT - 26; i < MAX_LINE_HEIGHT - 12; i++) {
			CopyMonoToLineBuffer(lineBuffer->dotlineAt(i),
				Barcode.Bitmap.Ext, lineBuffer->dotsPerLine(), true, 1);
		}
		lineBuffer->EndPos += Barcode.SymbolWidth;
		lineBuffer->setHeight(26);
		lineBuffer->print(0);
	}
}

static void printPDF417(void)
{
	uint32_t i, j, k, module_size, width, pos, offset, bytes_per_row;
	uint32_t *lineData;
	uint8_t mask;

	if (PDF417.SymbolRows == 0 || PDF417.SymbolWidth == 0)
		return;

	if (!lineBuffer->isEmpty()) /* 打印缓冲不为空，不能打印 */
		return;

	module_size = PDF417.ModuleSize;
	width = module_size * PDF417.SymbolWidth;
	while (Settings.leftMargin() + width > Settings.rightMargin()) {
		if (module_size == 1)
			return;
		module_size--;
		width = module_size * PDF417.SymbolWidth;
	}

	lineData = lineBuffer->dotlineAt(BASE_LINE);

	bytes_per_row = (PDF417.SymbolWidth - 1) / 8 + 1;
	offset = 0;

	for (i = 0; i < PDF417.SymbolRows; i++) {
		for (j = 0, k = 0, pos = lineBuffer->EndPos, mask = 0x80; j < PDF417.SymbolWidth; j++) {
			if (PDF417.Bitmap.Modules[offset + k] & mask)
				Dotline_SetDots(lineData, pos, PointStretchWidth[module_size],
					PointStretchWidth[module_size], false);
			mask >>= 1;
			if (mask == 0) {
				k++;
				mask = 0x80;
			}
			pos += module_size;
		}
		offset += bytes_per_row;
		lineBuffer->EndPos += pos;
		lineBuffer->setHeight(1);
		lineBuffer->printRaster(PDF417.RowHeight);
	}
}

static void printQRCode(void)
{
	uint32_t i, j, k, module_size, width, pos;
	uint32_t *lineData;
	uint8_t mask;

	if (QRCode.SymbolSize == 0)
		return;

	if (!lineBuffer->isEmpty()) /* 打印缓冲不为空，不能打印 */
		return;

	module_size = QRCode.ModuleSize;
	width = module_size * QRCode.SymbolSize;
	while (Settings.leftMargin() + width > Settings.rightMargin()) {
		if (module_size == 1)
			return;
		module_size--;
		width = module_size * QRCode.SymbolSize;
	}

	lineData = lineBuffer->dotlineAt(BASE_LINE);

	for (i = 0; i < QRCode.SymbolSize; i++) {
		for (j = 0, k = 0, pos = lineBuffer->EndPos, mask = 0x80; j < QRCode.SymbolSize; j++) {
			if (QRCode.Bitmap.Modules[i][k] & mask)
				Dotline_SetDots(lineData, pos, PointStretchWidth[module_size],
					PointStretchWidth[module_size], false);
			mask >>= 1;
			if (mask == 0) {
				k++;
				mask = 0x80;
			}
			pos += module_size;
		}
		lineBuffer->EndPos += pos;
		lineBuffer->setHeight(1);
		lineBuffer->printRaster(module_size);
	}
}

static uint32_t *firstDotline(void)
{
	return lineBuffer->dotlineAt(lineBuffer->firstLine());
}

static uint32_t *dotlineAt(uint32_t row)
{
	return lineBuffer->Data + row * LINE_STRIDE_WORDS;
}

static uint32_t firstLine(void)
{
	if (lineBuffer->Top < TOTAL_LINE_HEIGHT)
		return lineBuffer->Top;
	return (MAX_LINE_HEIGHT - lineBuffer->Height);
}

static uint32_t lastLine(void)
{
	if (lineBuffer->Bottom > 0)
		return lineBuffer->Bottom;
	return (lineBuffer->firstLine() + lineBuffer->Height - 1);
}

static void updateLineStride(void)
{
	if (Settings.BitsPerDot == 0)
		line_stride_index = (Settings.Mode == PAGE_MODE) ? 1 : 0;
	else
		line_stride_index = (Settings.Mode == PAGE_MODE) ? 3 : 2;
}

static uint32_t bytesPerLine(void)
{
	return LINE_STRIDE_BYTES;
}

static uint32_t wordsPerLine(void)
{
	return LINE_STRIDE_WORDS;
}

static uint32_t dotsPerLine(void)
{
	return (Settings.Mode == PAGE_MODE) ? dots_per_line_vertical : DOTS_PER_LINE;
}

static LineBuffer theBuffer = {
	.Flags = 0,
	.init = init,
	.isEmpty = isEmpty,
	.clear = clear,
	.append = append,
	.appendHarfBuzzCanvas = appendHarfBuzzCanvas,
	.setHeight = setHeight,
	.updateHeight = updateHeight,
	.setFlag = setFlag,
	.clearFlag = clearFlag,
	.print = print,
	.printLine = printLine,
	.printRaster = printRaster,
	.printBarcode = printBarcode,
	.printPDF417 = printPDF417,
	.printQRCode = printQRCode,
	.firstDotline = firstDotline,
	.dotlineAt = dotlineAt,
	.firstLine = firstLine,
	.lastLine = lastLine,
	.updateLineStride = updateLineStride,
	.bytesPerLine = bytesPerLine,
	.wordsPerLine = wordsPerLine,
	.dotsPerLine = dotsPerLine
};

LineBuffer *lineBuffer = &theBuffer;
