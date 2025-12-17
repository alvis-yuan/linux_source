#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "tspl_config.h"
#include "tspl_util.h"
#include "tspl_PageBuffer.h"

const uint32_t PointStretchWidth_util[32] = {
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

const uint32_t PointStretchWidth_littleEdian_util[32] = {
	0x00000000,
	0x00000001,
	0x00000003,
	0x00000007,
	0x0000000F,
	0x0000001F,
	0x0000003F,
	0x0000007F,
	0x000000FF,
	0x000001FF,
	0x000003FF,
	0x000007FF,
	0x00000FFF,
	0x00001FFF,
	0x00003FFF,
	0x00007FFF,
	0x0000FFFF,
	0x0001FFFF,
	0x0003FFFF,
	0x0007FFFF,
	0x000FFFFF,
	0x001FFFFF,
	0x003FFFFF,
	0x007FFFFF,
	0x00FFFFFF,
	0x01FFFFFF,
	0x03FFFFFF,
	0x07FFFFFF,
	0x0FFFFFFF,
	0x1FFFFFFF,
	0x3FFFFFFF,
	0x7FFFFFFF,
};

/*
 * 将buf看作一个连续的缓冲区，最高位为第0位。该函数把data复制到buf，
 * data的最高位与buf的第bit_pos位对齐。mask指定data的哪些位会复制到buf。
 *
 *   0  1  2  3  4  5  6  7                             n
 * +--+--+--+--+--+--+--+--+     +--+--+--+--+--+--+--+--+
 * |  |  |  |  |  |  |  |  | ... |  |  |  |  |  |  |  |  | buf
 * +--+--+--+--+--+--+--+--+     +--+--+--+--+--+--+--+--+
 *                     ^
 *                     |
 *                   +--+--+--+--+--+--+--+--+
 *                   |  |  |  |  |  |  |  |  | data
 *                   +--+--+--+--+--+--+--+--+
 */
void Dotline_CopyU32_util(uint32_t *buf, uint32_t bit_pos, uint32_t data, uint32_t mask, bool bitwise_or)
{
    uint32_t word_pos;
    uint32_t d0, d1, m0, m1;

    if (bit_pos >= pageBuffer->DOTS_PER_LINE || mask == 0)
        return;

    word_pos = (bit_pos >> 5);
    bit_pos = 31 - (bit_pos & 0x1F);

    data &= mask;
    d0 = data;
    m0 = mask;
    d1 = 0;
    m1 = 0;

    if (bit_pos < 31) {
        bit_pos = 31 - bit_pos;
        d1 = (d0 << (32 - bit_pos));
        m1 = (m0 << (32 - bit_pos));
        d0 >>= bit_pos;
        m0 >>= bit_pos;
    }

    if (!bitwise_or)
        buf[word_pos] &= ~m0;
    buf[word_pos] |= d0;

    if (m1 != 0 && ++word_pos < pageBuffer->WORDS_PER_LINE) {
        if (!bitwise_or)
            buf[word_pos] &= ~m1;
        buf[word_pos] |= d1;
    }
}

void Dotline_CopyU32_CairoFormat(uint32_t *buf, uint32_t pos, uint32_t data, uint32_t mask, bool bitwise_or)
{
    uint32_t word_pos, bit_pos;
    uint32_t d0,m0,d1,m1; //d0是buf前一个word的数据，d1是buf后一个word的数据

    if (pos >= pageBuffer->DOTS_PER_LINE || mask == 0)
        return;

    word_pos = pos >> 5;
    bit_pos = pos & 0x1F;
    data &= mask;
    m0 = d0 = m1 = d1 = 0;

    if (bit_pos) {
        d0 = data & PointStretchWidth_littleEdian_util[32 - bit_pos];
        d0 <<= bit_pos;
        m0 = mask & PointStretchWidth_littleEdian_util[32 - bit_pos];
        m0 <<= bit_pos;
        if (!bitwise_or)
            buf[word_pos] &= ~m0;
        buf[word_pos++] |= d0;
        
        d1 = data & PointStretchWidth_util[bit_pos];
        d1 >>= 32 - bit_pos;
        m1 = mask & PointStretchWidth_util[bit_pos];
        m1 >>= 32 - bit_pos;
        if (!bitwise_or)
            buf[word_pos] &= ~m1;
        buf[word_pos] |= d1;
    } else {
        if (!bitwise_or)
            buf[word_pos] &= ~mask;
        buf[word_pos] |= data;
    }
}

void Dotline_Or_util(uint32_t *dst, const uint32_t *src)
{
	uint32_t i;

	for (i = 0; i < pageBuffer->WORDS_PER_LINE; i++)
		dst[i] |= src[i];
}

void Dotline_Rshift_util(uint32_t *p_data, uint16_t length, uint16_t start_bit, uint16_t bits)
{
    uint16_t words, start_index;
    int16_t i, src, dst;
    uint32_t lbits, mask, prev, next, fixed = 0;

    if (length == 0 || bits == 0)
        return;

    if (start_bit > 0) {
        start_index = (start_bit >> 5);
        if (start_index + 1 > length)
            return;
        p_data += start_index;
        length -= start_index;
        start_bit &= 0x1F;
        for (words = 0, mask = 0x80000000; words < start_bit; words++, mask >>= 1)
            fixed |= mask;
        fixed &= (*p_data);
        (*p_data) &= ~fixed;
    }

    words = bits >> 5;
    bits &= 0x1F;
    if (words > 0) {
        if (words >= length) {
            memset(p_data, 0, length * 4);
            goto _exit;
        }
        for (src = length - 1 - words, dst = length - 1; src >= 0; src--, dst--)
            p_data[dst] = p_data[src];
        memset(p_data, 0, words * 4);
    }
    if (bits == 0)
        goto _exit;

    lbits = 32 - bits;
    mask = (1 << bits) - 1;
    prev = 0;
    for (i = 0; i < (int16_t)length; i++) {
        next = (p_data[i] & mask) << lbits;
        p_data[i] = (prev | (p_data[i] >> bits));
        prev = next;
    }

    _exit:
    (*p_data) |= fixed;
}

int str_to_int_util(const char *str, int base, bool *ok)
{
    char *endstr;
    int v;

    errno = 0;
    v = strtol(str, &endstr, base);
    if (ok)
        (*ok) = (*endstr == '\0' && errno == 0) ? true : false;
    return v;
}
