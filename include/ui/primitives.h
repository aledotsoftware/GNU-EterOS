#ifndef UI_PRIMITIVES_H
#define UI_PRIMITIVES_H

#include <types.h>

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} rect_t;

typedef struct {
    int32_t x;
    int32_t y;
} point_t;

/* Colores predefinidos */
#define UI_COLOR_BLACK   0xFF000000
#define UI_COLOR_WHITE   0xFFFFFFFF
#define UI_COLOR_GREY    0xFF808080
#define UI_COLOR_DARK    0xFF202020
#define UI_COLOR_BLUE    0xFF0000FF
#define UI_COLOR_CYAN    0xFF00FFFF

/* Funciones de dibujo base con clipping */
void ui_draw_pixel(int32_t x, int32_t y, uint32_t color);
void ui_draw_pixel_clipped(const rect_t* clip, int32_t x, int32_t y, uint32_t color);
void ui_fill_rect(const rect_t* rect, uint32_t color);
void ui_draw_rect(const rect_t* rect, uint32_t color);
void ui_draw_char(const rect_t* clip, int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg);
void ui_draw_string(const rect_t* clip, int32_t x, int32_t y, const char* text, uint32_t fg, uint32_t bg);

#endif
