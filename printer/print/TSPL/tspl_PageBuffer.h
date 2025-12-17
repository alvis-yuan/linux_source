#ifndef __TSPL_PAGEBUFFER_H__
#define __TSPL_PAGEBUFFER_H__

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <cairo.h>
#include <cairo-ft.h>
#include <ft2build.h>
#include "tspl_label_func.h"

typedef struct {
    uint32_t *Data;
    uint32_t DebugMode;
    uint32_t EndPos;
    uint32_t Height;
    uint32_t ValidHeight;
    uint32_t ReferenceHeight; // 参考坐标原点的高度
    uint32_t TextLineIndexHeight; // 用于渲染文字时，指示换行后的高度
    uint32_t TextLeftMargin; // TEXT指令，文字的x起始点
    uint32_t LineSpace;
    uint32_t HCharSize;
    uint32_t VCharSize;
    uint32_t DOTS_PER_LINE;
    uint32_t WORDS_PER_LINE;
    uint32_t BYTES_PER_LINE;
    uint32_t BYTES_PER_CHAR_ROW;
    cairo_surface_t *surface;
    cairo_t *cr;

    int       (*init)(uint32_t dots_per_line);
    void      (*clear)(void);
    void      (*kill)(void);
    void      (*print)(void);
    uint32_t  (*getHeight)(void);
    void      (*setHeight)(uint32_t);
    void      (*setValidHeight)(uint32_t);
    uint32_t *(*dotlineAt)(uint32_t);
    void      (*dump)(void);
    void      (*DrawRect)(int, int, int, int);   
    void      (*DrawEmptyRect)(int, int, int, int, int);
    void      (*DrawText)(int, int, int, int, int, int, char*);
    void      (*DrawBitmap)(int, int, int, int, const uint8_t *);
    void      (*EraseRegion)(int, int, int, int);
    void      (*ReverseRegion)(int, int, int, int);
    int       (*DrawDmatrix)(int, int, int, int, int, int, int, int, char *);
    int       (*DrawMaxicode)(int, int, char *);
    int       (*DrawBarcode)(int, int, const char *, uint32_t, bool, uint32_t, uint8_t, uint8_t, const char *);
    int       (*DrawPDF417)(int, int, int, int, uint32_t, const PDF417_options *, const char *);
} PageBuffer;

extern PageBuffer *pageBuffer;

/* 每一个命令对应的画笔操作都是单独的，因此当命令执行完毕后，画笔需要恢复最初的设置 */
#define CAIRO_ENTER_CRITICAL() cairo_save(pageBuffer->cr);
#define CAIRO_EXIT_CRITICAL()  cairo_restore(pageBuffer->cr);

#endif