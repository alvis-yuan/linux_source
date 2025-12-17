#ifndef __TSPL_UTIL_H__
#define __TSPL_UTIL_H__

#include <stdbool.h>
#include <stdint.h>

extern const uint32_t PointStretchWidth_util[32];
extern const uint32_t PointStretchWidth_littleEdian_util[32];

extern void Dotline_CopyU32_util(uint32_t *buf, uint32_t bit_pos, uint32_t data, uint32_t mask, bool bitwise_or);
extern void Dotline_CopyU32_CairoFormat(uint32_t *buf, uint32_t pos, uint32_t data, uint32_t mask, bool bitwise_or);
extern void Dotline_Or_util(uint32_t *dst, const uint32_t *src);
extern void Dotline_Rshift_util(uint32_t *p_data, uint16_t length, uint16_t start_bit, uint16_t bits);
extern int str_to_int_util(const char *str, int base, bool *ok);

#endif
