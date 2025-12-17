#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libcommon.h"
#include "printer.h"
#include "Instruct_Proc.h"
#include "Runtime_Data.h"
#include "Util.h"

static const uint8_t bayer_matrix[16][16] = {
	{0x01,0xc0,0x30,0xf0,0x0c,0xcc,0x3c,0xfc,0x03,0xc3,0x33,0xf3,0x0f,0xcf,0x3f,0xff},
	{0x80,0x40,0xb0,0x70,0x8c,0x4c,0xbc,0x7c,0x83,0x43,0xb3,0x73,0x8f,0x4f,0xbf,0x7f},
	{0x20,0xe0,0x10,0xd0,0x2c,0xec,0x1c,0xdc,0x23,0xe3,0x13,0xd3,0x2f,0xef,0x1f,0xdf},
	{0xa0,0x60,0x90,0x50,0xac,0x6c,0x9c,0x5c,0xa3,0x63,0x93,0x53,0xaf,0x6f,0x9f,0x5f},
	{0x08,0xc8,0x38,0xf8,0x04,0xc4,0x34,0xf4,0x0b,0xcb,0x3b,0xfb,0x07,0xc7,0x37,0xf7},
	{0x88,0x48,0xb8,0x78,0x84,0x44,0xb4,0x74,0x8b,0x4b,0xbb,0x7b,0x87,0x47,0xb7,0x77},
	{0x28,0xe8,0x18,0xd8,0x24,0xe4,0x14,0xd4,0x2b,0xeb,0x1b,0xdb,0x27,0xe7,0x17,0xd7},
	{0xa8,0x68,0x98,0x58,0xa4,0x64,0x94,0x54,0xab,0x6b,0x9b,0x5b,0xa7,0x67,0x97,0x57},
	{0x02,0xc2,0x32,0xf2,0x0e,0xce,0x3e,0xfe,0x01,0xc1,0x31,0xf1,0x0d,0xcd,0x3d,0xfd},
	{0x82,0x42,0xb2,0x72,0x8e,0x4e,0xbe,0x7e,0x81,0x41,0xb1,0x71,0x8d,0x4d,0xbd,0x7d},
	{0x22,0xe2,0x12,0xd2,0x2e,0xee,0x1e,0xde,0x21,0xe1,0x11,0xd1,0x2d,0xed,0x1d,0xdd},
	{0xa2,0x62,0x92,0x52,0xae,0x6e,0x9e,0x5e,0xa1,0x61,0x91,0x51,0xad,0x6d,0x9d,0x5d},
	{0x0a,0xca,0x3a,0xfa,0x06,0xc6,0x36,0xf6,0x09,0xc9,0x39,0xf9,0x05,0xc5,0x35,0xf5},
	{0x8a,0x4a,0xba,0x7a,0x86,0x46,0xb6,0x76,0x89,0x49,0xb9,0x79,0x85,0x45,0xb5,0x75},
	{0x2a,0xea,0x1a,0xda,0x26,0xe6,0x16,0xd6,0x29,0xe9,0x19,0xd9,0x25,0xe5,0x15,0xd5},
	{0xaa,0x6a,0x9a,0x5a,0xa6,0x66,0x96,0x56,0xa9,0x69,0x99,0x59,0xa5,0x65,0x95,0x55}
};

static void BitToByte(uint8_t *dst, const uint32_t *src, uint32_t dots_per_line, bool bitwise_or)
{
	uint32_t i, k, p, mask;

	p = 0;
	for (i = 0; i < (dots_per_line >> 5); i++) {
		mask = 0x80000000;
		for (k = 0; k < 32; k++) {
			if (!bitwise_or)
				dst[p] = (src[i] & mask) ? 0xFF : 0x00;
			else
				dst[p] = (src[i] & mask) ? 0xFF : dst[p];
			mask >>= 1;
			p++;
		}
	}
}

void timespec_add_msec(struct timespec *ts, long msec)
{
	ts->tv_sec  += (msec / 1000);
	ts->tv_nsec += (msec % 1000) * 1000000L;
	if (ts->tv_nsec > 999999999L) {
		ts->tv_nsec -= 1000000000L;
		ts->tv_sec++;
	}
}

int timespec_compare(const struct timespec *ts1, const struct timespec *ts2)
{
	if (ts1->tv_sec < ts2->tv_sec)
		return -1;
	if (ts1->tv_sec > ts2->tv_sec)
		return 1;
	if (ts1->tv_nsec < ts2->tv_nsec)
		return -1;
	if (ts1->tv_nsec > ts2->tv_nsec)
		return 1;
	return 0;
}

void SysSleepStart(struct timespec *ts)
{
	clock_gettime(CLOCK_MONOTONIC, ts);
}

void SysSleep(struct timespec *ts, long msec)
{
	timespec_add_msec(ts, msec);
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, ts, NULL);
}

void Dotline_SetDots(void *dotline, uint32_t dot_pos, uint32_t data, uint32_t mask, bool bitwise_or)
{
	if (Settings.BitsPerDot == 0) {
		Dotline_CopyU32((uint32_t *)dotline, dot_pos, data, mask, bitwise_or);
	}
	else {
		uint8_t *buf = (uint8_t *)dotline + dot_pos, v;
		uint32_t i, m;

		v = (Settings.BlackWhiteReverseMode == 0 && Settings.Color != 0) ? 0x7F : 0xFF;
		for (i = 0, m = 0x80000000; i < 32; i++, m >>= 1) {
			if (mask & m) {
				if (!bitwise_or)
					(*buf) = (data & m) ? v : 0x00;
				else
					(*buf) = (data & m) ? v : (*buf);
			}
			buf++;
		}
	}
}

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
void Dotline_CopyU32(uint32_t *buf, uint32_t bit_pos, uint32_t data, uint32_t mask, bool bitwise_or)
{
	uint32_t word_pos;
	uint32_t d0, d1, m0, m1;

	if (mask == 0)
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
	buf[word_pos++] |= d0;

	if (m1 != 0) {
		if (!bitwise_or)
			buf[word_pos] &= ~m1;
		buf[word_pos] |= d1;
	}
}

void Dotline_Or(uint32_t *dst, const uint32_t *src, uint32_t words)
{
	uint32_t i;

	for (i = 0; i < words; i++)
		dst[i] |= src[i];
}

void Dotline_Lshift(uint32_t *p_data, uint16_t length, uint16_t bits)
{
	uint16_t words;
	int16_t i, src, dst;
	uint32_t rbits, mask, prev, next;

	if (length == 0 || bits == 0)
		return;

	words = bits >> 5;
	bits &= 0x1F;
	if (words > 0) {
		if (words >= length) {
			memset(p_data, 0, length * 4);
			return;
		}
		for (src = words, dst = 0; src < (int16_t)length; src++, dst++)
			p_data[dst] = p_data[src];
		memset(p_data + length - words, 0, words * 4);
	}
	if (bits == 0)
		return;

	rbits = 32 - bits;
	mask = ~((1 << rbits) - 1);
	prev = 0;
	for (i = (int16_t)length - 1; i >= 0; i--) {
		next = (p_data[i] & mask) >> rbits;
		p_data[i] = ((p_data[i] << bits) | prev);
		prev = next;
	}
}

void Dotline_Rshift(uint32_t *p_data, uint16_t length, uint16_t start_bit, uint16_t bits)
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

void DotsExtension(uint32_t data, uint16_t bits, uint16_t times, uint32_t *out)
{
	uint32_t i, n, pos;
	uint32_t in_mask, out_mask;

	if (bits == 0 || bits > 32)
		return;
	if (times == 0 || times > 8)
		return;

	if (times == 1) {
		out[0] = data;
		return;
	}

	i = (bits * times - 1) / 32 + 1;
	memset(out, 0, sizeof(uint32_t) * i);

	out_mask = 0x80000000;
	for (i = 1, n = 0x40000000; i < times; i++, n >>= 1)
		out_mask |= n;
	pos = 0;
	for (i = 0, in_mask = 0x80000000; i < bits; i++, in_mask >>= 1) {
		if (data & in_mask) {
			Dotline_CopyU32(out, pos, out_mask, out_mask, false);
		}
		pos += times;
	}
}

void CopyMonoToLineBuffer(uint32_t *dst, const uint32_t *src, uint32_t dots_per_line, bool bitwise_or, uint32_t lines)
{
	uint32_t i;

	if (Settings.BitsPerDot == 0) {
		if (!bitwise_or) {
			memcpy(dst, src, (dots_per_line >> 3) * lines);
		}
		else {
			while (lines-- > 0) {
				for (i = 0; i < (dots_per_line >> 5); i++) {
					(*dst++) |= (*src++);
				}
			}
		}
	}
	else {
		while (lines-- > 0) {
			BitToByte((uint8_t *)dst, src, dots_per_line, bitwise_or);
			dst += dots_per_line >> 2;
			src += dots_per_line >> 5;
		}
	}
}

void DiffuseDither(uint32_t *dst_data, uint8_t *src_data, uint32_t width, uint32_t height, int shift_bits)
{
	int linebuf[DOTS_PER_LINE_MAX * 2], err;
	int *line1, *line2, *b1, *b2, *tmp;
	uint8_t *p;
	uint32_t x, y, bmwidth, mask;
	uint32_t *d, *q;
	bool not_last_line;

	line1 = linebuf;
	line2 = linebuf + DOTS_PER_LINE;
	bmwidth = (width + 7) / 8;

	p = src_data;
	b2 = line2;
	for (x = 0; x < width; x++)
		*b2++ = *p++;

	for (y = 0; y < height; y++) {
		tmp = line1;
		line1 = line2;
		line2 = tmp;
		not_last_line = y < height - 1;
		if (not_last_line) {
			p = src_data + (y + 1) * width;
			b2 = line2;
			for (x = 0; x < width; x++)
				*b2++ = *p++;
		}

		d = dst_data + y * WORDS_PER_LINE;
		q = d;
		memset(q, 0, bmwidth);
		b1 = line1;
		b2 = line2;
		mask = 0x80000000;
		for (x = 1; x <= width; x++) {
			if (*b1 < 128) { // black pixel
				err = *b1++;
				*q |= mask;
			}
			else { // white pixel
				err = *b1++ - 255;
			}
			if (mask == 1) {
				q++;
				mask = 0x80000000;
			}
			else {
				mask >>= 1;
			}
			const int e7 = ((err * 7) + 8) >> 4;
			const int e5 = ((err * 5) + 8) >> 4;
			const int e3 = ((err * 3) + 8) >> 4;
			const int e1 = err - (e7 + e5 + e3);
			if (x < width)
				*b1 += e7; // spread error to right pixel
			if (not_last_line) {
				b2[0] += e5; // pixel below
				if (x > 1)
					b2[-1] += e3; // pixel below left
				if (x < width)
					b2[1] += e1; // pixel below right
			}
			b2++;
		}

		if (shift_bits != 0) {
			if (shift_bits < 0)
				Dotline_Lshift(d, WORDS_PER_LINE,    -shift_bits);
			else
				Dotline_Rshift(d, WORDS_PER_LINE, 0,  shift_bits);
		}
	}
}

void OrderedDither(uint32_t *dst_data, uint8_t *src_data, uint32_t width, uint32_t height, int shift_bits)
{
	uint32_t i, j, k, s, mask;

	for (i = 0; i < height; i++) {
		mask = 0x80000000;
		s = 0;
		for (j = 0, k = 0; j < width; j++) {
			if (src_data[j] < bayer_matrix[s++ & 15][i & 15])
				dst_data[k] |= mask;
			if (mask == 1) {
				k++;
				mask = 0x80000000;
			}
			else
				mask >>= 1;
		}
		if (shift_bits != 0) {
			if (shift_bits < 0)
				Dotline_Lshift(dst_data, WORDS_PER_LINE,    -shift_bits);
			else
				Dotline_Rshift(dst_data, WORDS_PER_LINE, 0,  shift_bits);
		}
		src_data += width;
		dst_data += WORDS_PER_LINE;
	}
}

void ThresholdDither(uint32_t *dst_data, uint8_t *src_data, uint32_t width, uint32_t height, int shift_bits)
{
	uint32_t i, j, k, mask;

	for (i = 0; i < height; i++) {
		mask = 0x80000000;
		for (j = 0, k = 0; j < width; j++) {
			if (src_data[j] < 0x80)
				dst_data[k] |= mask;
			if (mask == 1) {
				k++;
				mask = 0x80000000;
			}
			else
				mask >>= 1;
		}
		if (shift_bits != 0) {
			if (shift_bits < 0)
				Dotline_Lshift(dst_data, WORDS_PER_LINE,    -shift_bits);
			else
				Dotline_Rshift(dst_data, WORDS_PER_LINE, 0,  shift_bits);
		}
		src_data += width;
		dst_data += WORDS_PER_LINE;
	}
}

void AutoContrast(uint8_t *data, uint32_t width, uint32_t height)
{
	uint32_t x, y, hist[256];
	uint32_t sum, pixels, min = 0, max = 255, range;
	int i;

	memset(hist, 0, sizeof(hist));

	i = 0;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++)
			hist[data[i++]]++;
	}

	pixels = (width * height) >> 5;

	sum = 0;
	for (i = 0; i < 256; i++) {
		sum += hist[i];
		if (sum >= pixels) {
			min = i;
			break;
		}
	}

	sum = 0;
	for (i = 255; i >= 0; i--) {
		sum += hist[i];
		if (sum >= pixels) {
			max = i;
			break;
		}
	}

	LogInfo("min=%u, max=%u", min, max);

	if ((min >= max) || (min < 5 && max > 250))
		return;
	range = max - min;

	i = 0;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			if (data[i] <= min)
				data[i] = 0;
			else if (data[i] >= max)
				data[i] = 255;
			else
				data[i] = (data[i] - min) * 255 / range;
			i++;
		}
	}
}

void AutoLevel(uint8_t *data, uint32_t width, uint32_t height)
{
	uint32_t i, sum, size, v;
	double ratio;
	uint8_t *p;

	size = width * height;
	for (i = 0, sum = 0, p = data; i < size; i++, p++)
		sum += (*p);
	LogInfo("sum=%u, pixels=%u, avg=%u", sum, i, sum / i);

	sum /= size;
	if (sum >= 170 && sum <= 180) /* 75~85 */
		return;

	ratio = (double)sum / 175.0;
	if (ratio > 1.4)
		ratio = 1.4;
	else if (ratio < 0.7)
		ratio = 0.7;
	for (i = 0, p = data; i < size; i++, p++) {
		v = (uint32_t)((double)(255 - (*p)) * ratio);
		if (v > 255)
			v = 255;
		(*p) = 255 - (uint8_t)v;
	}
}

/*
 * Coding of 'utf8':
 *
 *         +--------+--------+--------+--------+
 * 1-byte: |0xxxxxxx|        |        |        |
 *         +--------+--------+--------+--------+
 *
 *         +--------+--------+--------+--------+
 * 2-byte: |110xxxxx|10xxxxxx|        |        |
 *         +--------+--------+--------+--------+
 *
 *         +--------+--------+--------+--------+
 * 3-byte: |1110xxxx|10xxxxxx|10xxxxxx|        |
 *         +--------+--------+--------+--------+
 *
 *         +--------+--------+--------+--------+
 * 4-byte: |11110xxx|10xxxxxx|10xxxxxx|10xxxxxx|
 *         +--------+--------+--------+--------+
 *
 * Returns the unicode, or 0xFFFFFFFF if 'utf8' is not a valid UTF-8 bytes.
 */
uint32_t utf8_to_unicode(uint32_t utf8)
{
	uint32_t uni;

	if ((utf8 & 0x80000000) == 0) { /* 1-byte */
		uni  = ((utf8 >> 24) & 0x7F);
	}
	else if ((utf8 & 0xE0C00000) == 0xC0800000) { /* 2-byte */
		uni  = ((utf8 >> 16) & 0x3F);
		uni |= ((utf8 >> 18) & 0x7C0);
	}
	else if ((utf8 & 0xF0C0C000) == 0xE0808000) { /* 3-byte */
		uni  = ((utf8 >> 8 ) & 0x3F);
		uni |= ((utf8 >> 10) & 0xFC0);
		uni |= ((utf8 >> 12) & 0xF000);
	}
	else if ((utf8 & 0xF8C0C0C0) == 0xF0808080) { /* 4-byte */
		uni  = ((utf8      ) & 0x3F);
		uni |= ((utf8 >> 2 ) & 0xFC0);
		uni |= ((utf8 >> 4 ) & 0x3F000);
		uni |= ((utf8 >> 6 ) & 0x1C0000);
	}
	else /* Invalid */
		uni = 0xFFFFFFFF;

	return uni;
}

uint32_t utf8_bytes(uint8_t first)
{
	if ((first & 0x80) == 0x00) /* 1-byte */
		return 1;
	if ((first & 0xE0) == 0xC0) /* 2-byte */
		return 2;
	if ((first & 0xF0) == 0xE0) /* 3-byte */
		return 3;
	if ((first & 0xF8) == 0xF0) /* 4-byte */
		return 4;
	return 0;
}

int str_to_int(const char *str, int base, bool *ok)
{
	char *endstr;
	int v;

	errno = 0;
	v = strtol(str, &endstr, base);
	if (ok)
		(*ok) = (*endstr == '\0' && errno == 0) ? true : false;
	return v;
}

int PaperLayoutUnitToDots(int v)
{
	int s = 1, q, r;

	if (v < 0) {
		s = -1;
		v = -v;
	}

	v <<= 3;
	q = v / 10;
	r = v % 10;
	if (r > 4)
		q++;

	return (s * q);
}

void MakeCutCmdId(uint64_t *pv)
{
	const print_shm_t *shm = print_shm();
	const print_task_t *task;

	(*pv) = 0ULL;

	if (!shm)
		return;
	task = task_at(shm->task_queue.head);
	if (!task)
		return;
	(*pv)  = ((uint64_t)(task->id    )   << 48) & 0xFFFF000000000000ULL;
	(*pv)  = ((uint64_t)(task->copies)   << 32) & 0x0000FFFF00000000ULL;
	(*pv) |= ((uint64_t)(Cmd_BufferId)   << 16) & 0x00000000FFFF0000ULL;
	(*pv) |= ((uint64_t)(Cmd_BufferOffset + 1)) & 0x000000000000FFFFULL;
}

void SendPrinterCommand1(uint8_t code, uint8_t data1)
{
	uint8_t buf[4];

	buf[0] = code;
	buf[1] = data1;
	printer_send_command(buf, 2);
}
