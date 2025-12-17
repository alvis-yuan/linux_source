#include <string.h>
#include "libcommon.h"
#include "Runtime_Data.h"
#include "WordSet.h"
#include "CharBuffer.h"

static void dump(void)
{
	uint32_t i;

	LogDbg("width=%u, height=%u", charBuffer->Width, charBuffer->Height);
	for (i = 0; i < 24; i++)
		LogDbg("%08X", charBuffer->Data[i]);
}

static void clear(void)
{
	memset(charBuffer->Data, 0, sizeof(charBuffer->Data));
	memset(charBuffer->Data64, 0, sizeof(charBuffer->Data64));
	charBuffer->Width = 0;
	charBuffer->Height = 0;
	charBuffer->Top = 0;
	charBuffer->AdvanceX = 0;
	charBuffer->Left = 0;
	charBuffer->CharType = CHAR_TYPE_ASC;
	charBuffer->UseData64 = 0;
}

static void readWordSetASC(uint8_t code)
{
	uint32_t rc;

	charBuffer->clear();
	rc = ReadWordSet_ASC(charBuffer, code);
	charBuffer->Width  = ((rc      ) & 0xFFFF);
	charBuffer->Height = ((rc >> 16) & 0xFFFF);
	charBuffer->AdvanceX = charBuffer->Width;
	charBuffer->CharType = CHAR_TYPE_ASC;
}

static void readWordSetCodePage(uint8_t code)
{
	uint32_t rc;

	charBuffer->clear();
	rc = ReadWordSet_CodePage(charBuffer, code);
	charBuffer->Width  = ((rc      ) & 0xFFFF);
	charBuffer->Height = ((rc >> 16) & 0xFFFF);
	charBuffer->AdvanceX = charBuffer->Width;
	charBuffer->CharType = CHAR_TYPE_ASC;
}

static void readWordSetUnicode(uint32_t unicode)
{
	uint32_t rc;

	charBuffer->clear();
	rc = ReadWordSet_Unicode(charBuffer, unicode);
	charBuffer->Width  = ((rc      ) & 0xFFFF);
	charBuffer->Height = ((rc >> 16) & 0xFFFF);
}

static void clockwiseRotation(void)
{
	uint32_t buf[24], mask;
	int w, h, row, col, r1;

	memset(buf, 0, sizeof(buf));
	w = charBuffer->Width;
	h = charBuffer->Height;
	for (row = 23, mask = (1 << (32 - w)); row >= 24 - w; row--, mask <<= 1) {
		for (col = 31, r1 = 23; col >= 32 - h; col--, r1--) {
			if (charBuffer->Data[r1] & mask)
				buf[row] |= (1 << col);
		}
	}
	memcpy(charBuffer->Data, buf, sizeof(buf));

	charBuffer->Width = h;
	charBuffer->Height = w;
}

static void blackWhiteReverse(void)
{
	uint32_t mask, i, row;
	uint64_t mask64;

	if (!charBuffer->UseData64) {
		for (i = 1, mask = 0x80000000; i < charBuffer->Width; i++)
			mask |= (mask >> 1);
		for (i = 0, row = charBuffer->Top; i < charBuffer->Height; i++, row++)
			charBuffer->Data[row] = (~charBuffer->Data[row]) & mask;
	}
	else {
		for (i = 1, mask64 = 0x8000000000000000ULL; i < charBuffer->Width; i++)
			mask64 |= (mask64 >> 1);
		for (i = 0, row = charBuffer->Top; i < charBuffer->Height; i++, row++)
			charBuffer->Data64[row] = (~charBuffer->Data64[row]) & mask64;
	}
}

static CharBuffer theBuffer = {
	.dump = dump,
	.clear = clear,
	.readWordSetASC = readWordSetASC,
	.readWordSetCodePage = readWordSetCodePage,
	.readWordSetUnicode = readWordSetUnicode,
	.clockwiseRotation = clockwiseRotation,
	.blackWhiteReverse = blackWhiteReverse
};

CharBuffer *charBuffer = &theBuffer;
