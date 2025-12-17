#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define PRINTER_SECTION "printer"

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern void timespec_add_msec(struct timespec *ts, long msec);
extern int timespec_compare(const struct timespec *ts1, const struct timespec *ts2);

extern void SysSleepStart(struct timespec *ts);
extern void SysSleep(struct timespec *ts, long msec);

extern void Dotline_SetDots(void *dotline, uint32_t dot_pos, uint32_t data, uint32_t mask, bool bitwise_or);
extern void Dotline_CopyU32(uint32_t *buf, uint32_t bit_pos, uint32_t data, uint32_t mask, bool bitwise_or);
extern void Dotline_Or(uint32_t *dst, const uint32_t *src, uint32_t words);
extern void Dotline_Lshift(uint32_t *p_data, uint16_t length, uint16_t bits);
extern void Dotline_Rshift(uint32_t *p_data, uint16_t length, uint16_t start_bit, uint16_t bits);

extern void DotsExtension(uint32_t data, uint16_t bits, uint16_t times, uint32_t *out);
extern void CopyMonoToLineBuffer(uint32_t *dst, const uint32_t *src, uint32_t dots_per_line, bool bitwise_or, uint32_t lines);

extern void DiffuseDither(uint32_t *dst_data, uint8_t *src_data, uint32_t width, uint32_t height, int shift_bits);
extern void OrderedDither(uint32_t *dst_data, uint8_t *src_data, uint32_t width, uint32_t height, int shift_bits);
extern void ThresholdDither(uint32_t *dst_data, uint8_t *src_data, uint32_t width, uint32_t height, int shift_bits);
extern void AutoContrast(uint8_t *data, uint32_t width, uint32_t height);
extern void AutoLevel(uint8_t *data, uint32_t width, uint32_t height);

extern uint32_t utf8_to_unicode(uint32_t utf8);
extern uint32_t utf8_bytes(uint8_t first);

extern int str_to_int(const char *str, int base, bool *ok);
extern int PaperLayoutUnitToDots(int v);

extern void MakeCutCmdId(uint64_t *pv);
extern void SendPrinterCommand1(uint8_t code, uint8_t data1);

#endif // __UTIL_H__
