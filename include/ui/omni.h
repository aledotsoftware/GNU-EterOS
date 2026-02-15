#ifndef OMNI_H
#define OMNI_H

#include <types.h>
#include <ui/primitives.h>

/* Omni Drawing Engine - High Performance 2D Graphics */

/* Initializes the Omni engine (caches framebuffer pointers) */
void omni_init(void);

/* Basic Primitives */
void omni_draw_pixel(int32_t x, int32_t y, uint32_t color);
void omni_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void omni_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void omni_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);

/* Text Rendering */
void omni_draw_char(int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg);
void omni_draw_string(const rect_t* clip, int32_t x, int32_t y, const char* str, uint32_t fg, uint32_t bg);

/* Bitmap & Image Support */
#define OMNI_BITMAP_ALPHA 1  /* Enable alpha blending */

void omni_draw_bitmap(int32_t x, int32_t y, int32_t w, int32_t h, const uint32_t* data, int flags);
void omni_draw_image_from_path(const char* path, int32_t x, int32_t y);

/* Utility */
uint32_t omni_get_width(void);
uint32_t omni_get_height(void);

#endif
