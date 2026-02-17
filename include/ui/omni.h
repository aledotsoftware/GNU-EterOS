#ifndef OMNI_H
#define OMNI_H

#include <types.h>
#include <types.h>

/* Moved from primitives.h */
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
#define UI_COLOR_RED     0xFFFF0000
#define UI_COLOR_GREEN   0xFF00FF00

/* ========================================================================= */
/* Omni Drawing Engine v2.0 - éterOS High Performance 2D Graphics            */
/* ========================================================================= */
/*
 * Fase 5.1 Optimizations:
 * - Frame-batched buffer caching (omni_begin_frame)
 * - Dirty region tracking for partial flushes
 * - Alpha blending (256-level) for compositing
 * - Transparent text rendering (no background overdraw)
 * - Gradient fills, rounded rectangles, filled circles
 * - Optimized line drawing (horizontal/vertical fast paths)
 * - Row-based image blitting (memcpy per row vs per-pixel)
 */

/* Frame Management */
void omni_init(void);
void omni_begin_frame(void);
void omni_get_dirty_rect(int32_t* x, int32_t* y, int32_t* w, int32_t* h);

/* Basic Primitives */
void omni_draw_pixel(int32_t x, int32_t y, uint32_t color);
void omni_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void omni_fill_rect_alpha(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, uint8_t alpha);
void omni_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void omni_draw_rounded_rect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint32_t color);
void omni_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void omni_fill_circle(int32_t cx, int32_t cy, int32_t radius, uint32_t color);
void omni_fill_gradient_v(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color_top, uint32_t color_bottom);

/* Text Rendering */
void omni_draw_char(int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg);
void omni_draw_char_transparent(int32_t x, int32_t y, char c, uint32_t fg);
void omni_draw_char_scaled(int32_t x, int32_t y, char c, uint32_t fg, int scale);
void omni_draw_string(const rect_t* clip, int32_t x, int32_t y, const char* str, uint32_t fg, uint32_t bg);
void omni_draw_string_transparent(int32_t x, int32_t y, const char* str, uint32_t fg);

/* Bitmap & Image Support */
#define OMNI_BITMAP_ALPHA 1  /* Enable alpha blending */

void omni_draw_bitmap(int32_t x, int32_t y, int32_t w, int32_t h, const uint32_t* data, int flags);
void omni_draw_image_from_path(const char* path, int32_t x, int32_t y);

/* Utility */
uint32_t omni_get_width(void);
uint32_t omni_get_height(void);

#endif
