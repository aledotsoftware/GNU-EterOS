/*
 * Omni Drawing Engine - éterOS High Performance 2D Graphics
 *
 * Fase 5.1 Optimizations:
 * - Cached framebuffer pointer (eliminated per-call get_buffer overhead)
 * - Batch rendering with omni_begin_frame / omni_end_frame
 * - Optimized line drawing with direct pixel writes
 * - Row-based image blitting (vs per-pixel putpixel)
 * - 256-level alpha blending for smooth compositing
 * - Dirty region tracking for partial flushes
 */
#include <ui/omni.h>
#include <framebuffer.h>
#include <string.h>
#include "upng.h"
#include <fs/initrd.h>
#include <mm.h>
#include <serial.h>

/* ========================================================================= */
/* Global State - Cached for Performance                                     */
/* ========================================================================= */

static uint32_t* omni_fb = 0;
static uint32_t omni_pitch = 0; /* In BYTES */
static uint32_t omni_width = 0;
static uint32_t omni_height = 0;
static uint32_t omni_bpp = 0;
static uint32_t omni_pitch_div4 = 0; /* Pitch in uint32_t elements */

/* Dirty Region Tracking */
static int32_t dirty_x1 = 0;
static int32_t dirty_y1 = 0;
static int32_t dirty_x2 = 0;
static int32_t dirty_y2 = 0;
static bool    dirty_valid = false;

extern const uint8_t font8x16[];

/* ========================================================================= */
/* Core Init & Frame Management                                               */
/* ========================================================================= */

void omni_init(void) {
    omni_fb = framebuffer_get_buffer();
    omni_pitch = framebuffer_get_pitch();
    omni_width = framebuffer_get_width();
    omni_height = framebuffer_get_height();
    omni_bpp = framebuffer_get_bpp();

    if (omni_bpp == 32) {
        omni_pitch_div4 = omni_pitch / 4;
    }

    dirty_valid = false;

    serial_write_string("[OMNI] Engine v2.0 (Optimized). FB: ");
    char buf[32];
    utoa_hex_s((uint64_t)omni_fb, buf, 32);
    serial_write_string(buf);
    serial_write_string("\n");
}

uint32_t omni_get_width(void) { return omni_width; }
uint32_t omni_get_height(void) { return omni_height; }

/* Refresh cached framebuffer pointer - call once per frame */
void omni_begin_frame(void) {
    omni_fb = framebuffer_get_buffer();
    dirty_valid = false;
    dirty_x1 = (int32_t)omni_width;
    dirty_y1 = (int32_t)omni_height;
    dirty_x2 = 0;
    dirty_y2 = 0;
}

/* Get dirty region for partial flush */
void omni_get_dirty_rect(int32_t* x, int32_t* y, int32_t* w, int32_t* h) {
    if (!dirty_valid || dirty_x2 <= dirty_x1 || dirty_y2 <= dirty_y1) {
        *x = 0; *y = 0; *w = omni_width; *h = omni_height;
        return;
    }
    *x = dirty_x1;
    *y = dirty_y1;
    *w = dirty_x2 - dirty_x1;
    *h = dirty_y2 - dirty_y1;
}

/* ========================================================================= */
/* Dirty Region Tracking                                                      */
/* ========================================================================= */

static inline void dirty_mark(int32_t x, int32_t y, int32_t w, int32_t h) {
    if (x < dirty_x1) dirty_x1 = x;
    if (y < dirty_y1) dirty_y1 = y;
    int32_t x2 = x + w;
    int32_t y2 = y + h;
    if (x2 > dirty_x2) dirty_x2 = x2;
    if (y2 > dirty_y2) dirty_y2 = y2;
    dirty_valid = true;
}

/* ========================================================================= */
/* Optimized Pixel Operations                                                 */
/* ========================================================================= */

/* Ultra-fast inline pixel write - NO bounds check, NO buffer refresh */
static inline void _omni_put_pixel_fast(int x, int y, uint32_t color) {
    omni_fb[y * omni_pitch_div4 + x] = color;
}

/* Alpha blend: src over dst with 8-bit alpha */
static inline uint32_t _alpha_blend(uint32_t dst, uint32_t src, uint8_t alpha) {
    if (alpha == 0xFF) return src;
    if (alpha == 0x00) return dst;
    
    uint32_t inv_alpha = 255 - alpha;

    /* SWAR Optimization: Process R/B in parallel, then G */
    uint32_t s_rb = src & 0xFF00FF;
    uint32_t d_rb = dst & 0xFF00FF;
    uint32_t rb = (s_rb * alpha + d_rb * inv_alpha) >> 8;
    
    uint32_t s_g = src & 0x00FF00;
    uint32_t d_g = dst & 0x00FF00;
    uint32_t g = (s_g * alpha + d_g * inv_alpha) >> 8;
    
    return 0xFF000000 | (rb & 0xFF00FF) | (g & 0x00FF00);
}

void omni_draw_pixel(int32_t x, int32_t y, uint32_t color) {
    if (x < 0 || y < 0 || (uint32_t)x >= omni_width || (uint32_t)y >= omni_height) return;

    if (omni_bpp == 32) {
        _omni_put_pixel_fast(x, y, color);
    } else {
        framebuffer_putpixel(x, y, color);
    }
}

/* ========================================================================= */
/* Optimized Rectangle Fill                                                   */
/* ========================================================================= */

void omni_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    /* Clipping */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }

    if ((uint32_t)x >= omni_width || (uint32_t)y >= omni_height) return;
    if ((uint32_t)(x + w) > omni_width) w = omni_width - x;
    if ((uint32_t)(y + h) > omni_height) h = omni_height - y;

    if (w <= 0 || h <= 0) return;

    dirty_mark(x, y, w, h);

    if (omni_bpp == 32) {
        uint32_t* row = omni_fb + (y * omni_pitch_div4) + x;

        /* Optimization: Coalesce contiguous memory writes (e.g. Full Screen Clear) */
        if ((uint32_t)w == omni_pitch_div4) {
            memset32(row, color, (size_t)w * h);
            return;
        }

        /* Optimization: Avoid memset overhead for vertical lines (e.g. Grid/UI borders) */
        if (w == 1) {
            for (int i = 0; i < h; i++) {
                *row = color;
                row += omni_pitch_div4;
            }
            return;
        }

        for (int i = 0; i < h; i++) {
            memset32(row, color, w);
            row += omni_pitch_div4;
        }
    } else {
        framebuffer_rect(x, y, w, h, color);
    }
}

/* Alpha-blended rectangle fill */
void omni_fill_rect_alpha(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, uint8_t alpha) {
    if (alpha == 0xFF) { omni_fill_rect(x, y, w, h, color); return; }
    if (alpha == 0x00) return;
    
    /* Clipping */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if ((uint32_t)x >= omni_width || (uint32_t)y >= omni_height) return;
    if ((uint32_t)(x + w) > omni_width) w = omni_width - x;
    if ((uint32_t)(y + h) > omni_height) h = omni_height - y;
    if (w <= 0 || h <= 0) return;
    
    dirty_mark(x, y, w, h);
    
    if (omni_bpp == 32) {
        uint32_t* row = omni_fb + (y * omni_pitch_div4) + x;

        /* Pre-calculate invariant source components for SWAR blending */
        uint32_t inv_alpha = 255 - alpha;
        uint32_t s_rb_a = (color & 0xFF00FF) * alpha;
        uint32_t s_g_a  = (color & 0x00FF00) * alpha;

        for (int i = 0; i < h; i++) {
            /* Optimized inner loop: use local pointer and unroll or let compiler optimize */
            uint32_t* p = row;
            int count = w;
            while (count--) {
                uint32_t dst = *p;
                uint32_t d_rb = dst & 0xFF00FF;
                uint32_t d_g  = dst & 0x00FF00;

                uint32_t rb = (s_rb_a + d_rb * inv_alpha) >> 8;
                uint32_t g  = (s_g_a  + d_g  * inv_alpha) >> 8;

                *p++ = 0xFF000000 | (rb & 0xFF00FF) | (g & 0x00FF00);
            }
            row += omni_pitch_div4;
        }
    }
}

/* Scaled character drawing (Transparency only) */
void omni_draw_char_scaled(int32_t x, int32_t y, char c, uint32_t fg, int scale) {
    if (scale < 1) scale = 1;
    if (x < 0 || y < 0 || (uint32_t)(x + 8 * scale) > omni_width || (uint32_t)(y + 16 * scale) > omni_height) return;

    dirty_mark(x, y, 8 * scale, 16 * scale);

    const uint8_t* glyph = &font8x16[(unsigned char)c * 16];

    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                omni_fill_rect(x + col * scale, y + row * scale, scale, scale, fg);
            }
        }
    }
}

void omni_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    omni_fill_rect(x, y, w, 1, color);         /* Top */
    omni_fill_rect(x, y + h - 1, w, 1, color); /* Bottom */
    omni_fill_rect(x, y, 1, h, color);         /* Left */
    omni_fill_rect(x + w - 1, y, 1, h, color); /* Right */
}

/* Rounded rectangle (border radius via corner overwrites) */
void omni_draw_rounded_rect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint32_t color) {
    if (radius <= 0) { omni_fill_rect(x, y, w, h, color); return; }
    if (radius > w/2) radius = w/2;
    if (radius > h/2) radius = h/2;
    
    /* Fill center body */
    omni_fill_rect(x + radius, y, w - 2 * radius, h, color);
    /* Fill left strip */
    omni_fill_rect(x, y + radius, radius, h - 2 * radius, color);
    /* Fill right strip */
    omni_fill_rect(x + w - radius, y + radius, radius, h - 2 * radius, color);
    
    /* Fill corners with circle quadrants (Bresenham) */
    int cx, cy, d;
    d = 3 - 2 * radius;
    cx = 0;
    cy = radius;
    
    while (cx <= cy) {
        /* Top-left corner */
        omni_fill_rect(x + radius - cy, y + radius - cx, cy, 1, color);
        omni_fill_rect(x + radius - cx, y + radius - cy, cx, 1, color);
        /* Top-right corner */
        omni_fill_rect(x + w - radius, y + radius - cx, cy, 1, color);
        omni_fill_rect(x + w - radius + radius - cx, y + radius - cy, cx, 1, color);
        /* Bottom-left corner */
        omni_fill_rect(x + radius - cy, y + h - radius + cx - 1, cy, 1, color);
        omni_fill_rect(x + radius - cx, y + h - radius + cy - 1, cx, 1, color);
        /* Bottom-right corner */
        omni_fill_rect(x + w - radius, y + h - radius + cx - 1, cy, 1, color);
        omni_fill_rect(x + w - radius + radius - cx, y + h - radius + cy - 1, cx, 1, color);
        
        if (d < 0) {
            d += 4 * cx + 6;
        } else {
            d += 4 * (cx - cy) + 10;
            cy--;
        }
        cx++;
    }
}

/* ========================================================================= */
/* Optimized Line Drawing                                                     */
/* ========================================================================= */

void omni_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    /* Fast path: Horizontal line */
    if (y0 == y1) {
        int32_t start = (x0 < x1) ? x0 : x1;
        int32_t len = (x0 < x1) ? (x1 - x0 + 1) : (x0 - x1 + 1);
        omni_fill_rect(start, y0, len, 1, color);
        return;
    }
    
    /* Fast path: Vertical line */
    if (x0 == x1) {
        int32_t start = (y0 < y1) ? y0 : y1;
        int32_t len = (y0 < y1) ? (y1 - y0 + 1) : (y0 - y1 + 1);
        omni_fill_rect(x0, start, 1, len, color);
        return;
    }

    /* Bresenham with direct pixel writes (no function call overhead) */
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2;
    int e2;

    /* Mark dirty region for entire line bounding box */
    int32_t min_x = (x0 < x1) ? x0 : x1;
    int32_t min_y = (y0 < y1) ? y0 : y1;
    dirty_mark(min_x, min_y, dx + 1, dy + 1);

    while (1) {
        /* Direct pixel write with inline bounds check */
        if (x0 >= 0 && y0 >= 0 && (uint32_t)x0 < omni_width && (uint32_t)y0 < omni_height) {
            if (omni_bpp == 32) {
                _omni_put_pixel_fast(x0, y0, color);
            } else {
                framebuffer_putpixel(x0, y0, color);
            }
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 < dy) { err += dx; y0 += sy; }
    }
}

/* ========================================================================= */
/* Optimized Text Rendering                                                   */
/* ========================================================================= */

void omni_draw_char(int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg) {
    if (x < 0 || y < 0 || (uint32_t)(x + 8) > omni_width || (uint32_t)(y + 16) > omni_height) return;

    dirty_mark(x, y, 8, 16);

    if (omni_bpp == 32) {
        const uint8_t* glyph = &font8x16[(unsigned char)c * 16];
        uint32_t* row_ptr = omni_fb + (y * omni_pitch_div4) + x;

        for (int i = 0; i < 16; i++) {
            uint8_t bits = glyph[i];
            /* Unrolled 8-pixel write */
            row_ptr[0] = (bits & 0x80) ? fg : bg;
            row_ptr[1] = (bits & 0x40) ? fg : bg;
            row_ptr[2] = (bits & 0x20) ? fg : bg;
            row_ptr[3] = (bits & 0x10) ? fg : bg;
            row_ptr[4] = (bits & 0x08) ? fg : bg;
            row_ptr[5] = (bits & 0x04) ? fg : bg;
            row_ptr[6] = (bits & 0x02) ? fg : bg;
            row_ptr[7] = (bits & 0x01) ? fg : bg;

            row_ptr += omni_pitch_div4;
        }
    } else {
        framebuffer_putchar(c, x, y, fg, bg);
    }
}

/* Optimized: Transparent text (only draw foreground pixels, skip background) */
void omni_draw_char_transparent(int32_t x, int32_t y, char c, uint32_t fg) {
    if (x < 0 || y < 0 || (uint32_t)(x + 8) > omni_width || (uint32_t)(y + 16) > omni_height) return;
    
    dirty_mark(x, y, 8, 16);
    
    if (omni_bpp == 32) {
        const uint8_t* glyph = &font8x16[(unsigned char)c * 16];
        uint32_t* row_ptr = omni_fb + (y * omni_pitch_div4) + x;
        
        for (int i = 0; i < 16; i++) {
            uint8_t bits = glyph[i];
            /* Only write foreground pixels - skip background (transparency) */
            if (bits & 0x80) row_ptr[0] = fg;
            if (bits & 0x40) row_ptr[1] = fg;
            if (bits & 0x20) row_ptr[2] = fg;
            if (bits & 0x10) row_ptr[3] = fg;
            if (bits & 0x08) row_ptr[4] = fg;
            if (bits & 0x04) row_ptr[5] = fg;
            if (bits & 0x02) row_ptr[6] = fg;
            if (bits & 0x01) row_ptr[7] = fg;
            
            row_ptr += omni_pitch_div4;
        }
    }
}

void omni_draw_string(const rect_t* clip, int32_t x, int32_t y, const char* str, uint32_t fg, uint32_t bg) {
    if (!str) return;

    /* ⚡ BOLT Optimization: Fast path for unclipped/fully visible text using Row-Major rendering */
    /* This changes memory access pattern from vertical strips to horizontal contiguous writes */
    if (omni_bpp == 32) {
        size_t len = strlen(str);
        int32_t w = (int32_t)len * 8;
        int32_t h = 16;

        bool fits = false;
        if (clip) {
            fits = (x >= clip->x && x + w <= clip->x + clip->w &&
                    y >= clip->y && y + h <= clip->y + clip->h);
        } else {
            fits = (x >= 0 && y >= 0 && (uint32_t)(x + w) <= omni_width && (uint32_t)(y + h) <= omni_height);
        }

        if (fits) {
            dirty_mark(x, y, w, h);

            uint32_t* fb_row = omni_fb + (y * omni_pitch_div4) + x;
            const uint8_t* font_base = font8x16;

            for (int row = 0; row < 16; row++) {
                uint32_t* p = fb_row;
                const char* s = str;
                while (*s) {
                    unsigned char c = (unsigned char)*s++;
                    uint8_t bits = font_base[c * 16 + row];

                    /* Unroll 8 pixels */
                    p[0] = (bits & 0x80) ? fg : bg;
                    p[1] = (bits & 0x40) ? fg : bg;
                    p[2] = (bits & 0x20) ? fg : bg;
                    p[3] = (bits & 0x10) ? fg : bg;
                    p[4] = (bits & 0x08) ? fg : bg;
                    p[5] = (bits & 0x04) ? fg : bg;
                    p[6] = (bits & 0x02) ? fg : bg;
                    p[7] = (bits & 0x01) ? fg : bg;

                    p += 8;
                }
                fb_row += omni_pitch_div4;
            }
            return;
        }
    }

    /* Slow path: Per-char clipping / bounds checking */
    int32_t cx = x;
    while (*str) {
        if (clip) {
            if (cx + 8 >= clip->x && cx < clip->x + clip->w &&
                y + 16 >= clip->y && y < clip->y + clip->h) {
                omni_draw_char(cx, y, *str, fg, bg);
            }
        } else {
            omni_draw_char(cx, y, *str, fg, bg);
        }
        cx += 8;
        str++;
    }
}

/* Optimized string with transparent background */
void omni_draw_string_transparent(int32_t x, int32_t y, const char* str, uint32_t fg) {
    if (!str) return;

    /* ⚡ BOLT Optimization: Fast path for unclipped/fully visible text using Row-Major rendering */
    if (omni_bpp == 32) {
        size_t len = strlen(str);
        int32_t w = (int32_t)len * 8;
        int32_t h = 16;

        /* Check screen bounds */
        if (x >= 0 && y >= 0 && (uint32_t)(x + w) <= omni_width && (uint32_t)(y + h) <= omni_height) {
            dirty_mark(x, y, w, h);

            uint32_t* fb_row = omni_fb + (y * omni_pitch_div4) + x;
            const uint8_t* font_base = font8x16;

            for (int row = 0; row < 16; row++) {
                uint32_t* p = fb_row;
                const char* s = str;
                while (*s) {
                    unsigned char c = (unsigned char)*s++;
                    uint8_t bits = font_base[c * 16 + row];

                    if (bits) { /* Optimization: skip entire byte if 0 */
                        if (bits & 0x80) p[0] = fg;
                        if (bits & 0x40) p[1] = fg;
                        if (bits & 0x20) p[2] = fg;
                        if (bits & 0x10) p[3] = fg;
                        if (bits & 0x08) p[4] = fg;
                        if (bits & 0x04) p[5] = fg;
                        if (bits & 0x02) p[6] = fg;
                        if (bits & 0x01) p[7] = fg;
                    }
                    p += 8;
                }
                fb_row += omni_pitch_div4;
            }
            return;
        }
    }

    int32_t cx = x;
    while (*str) {
        omni_draw_char_transparent(cx, y, *str, fg);
        cx += 8;
        str++;
    }
}

/* ========================================================================= */
/* Optimized Bitmap & Image Rendering                                         */
/* ========================================================================= */

void omni_draw_bitmap(int32_t x, int32_t y, int32_t w, int32_t h, const uint32_t* data, int flags) {
    if (!data) return;

    int32_t stride = w; /* Original width is the stride */

    /* Clipping */
    int32_t src_x = 0;
    int32_t src_y = 0;

    if (x < 0) { src_x = -x; w += x; x = 0; }
    if (y < 0) { src_y = -y; h += y; y = 0; }

    if ((uint32_t)x >= omni_width || (uint32_t)y >= omni_height) return;
    if ((uint32_t)(x + w) > omni_width) w = omni_width - x;
    if ((uint32_t)(y + h) > omni_height) h = omni_height - y;

    if (w <= 0 || h <= 0) return;

    dirty_mark(x, y, w, h);

    if (omni_bpp == 32) {
        uint32_t* dest_row = omni_fb + (y * omni_pitch_div4) + x;
        const uint32_t* src_row = data + (src_y * stride) + src_x;

        if (flags & OMNI_BITMAP_ALPHA) {
            /* Alpha blending path */
            for (int i = 0; i < h; i++) {
                for (int j = 0; j < w; j++) {
                    uint32_t color = src_row[j];
                    uint8_t alpha = (color >> 24) & 0xFF;
                    if (alpha > 240) {
                        dest_row[j] = color; /* Nearly opaque - fast path */
                    } else if (alpha > 10) {
                        dest_row[j] = _alpha_blend(dest_row[j], color, alpha);
                    }
                    /* alpha <= 10: fully transparent, skip */
                }
                dest_row += omni_pitch_div4;
                src_row += stride;
            }
        } else {
            /* Opaque path: use memcpy per row for maximum throughput */
            for (int i = 0; i < h; i++) {
                memcpy(dest_row, src_row, w * 4);
                dest_row += omni_pitch_div4;
                src_row += stride;
            }
        }
    }
}

/* Fill with gradient (vertical) */
void omni_fill_gradient_v(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color_top, uint32_t color_bottom) {
    if (w <= 0 || h <= 0) return;
    
    /* Clipping */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if ((uint32_t)x >= omni_width || (uint32_t)y >= omni_height) return;
    if ((uint32_t)(x + w) > omni_width) w = omni_width - x;
    if ((uint32_t)(y + h) > omni_height) h = omni_height - y;
    if (w <= 0 || h <= 0) return;
    
    dirty_mark(x, y, w, h);
    
    uint32_t tr = (color_top >> 16) & 0xFF;
    uint32_t tg = (color_top >> 8) & 0xFF;
    uint32_t tb = color_top & 0xFF;
    uint32_t br = (color_bottom >> 16) & 0xFF;
    uint32_t bg = (color_bottom >> 8) & 0xFF;
    uint32_t bb = color_bottom & 0xFF;
    
    if (omni_bpp == 32) {
        if (h <= 0) return; /* Defense-in-depth: Ensure no division by zero */

        /* ⚡ BOLT Optimization: Use fixed-point math to avoid division in loop
           and fix unsigned underflow bug for decreasing gradients. */
        int32_t r_val = (int32_t)tr << 16;
        int32_t g_val = (int32_t)tg << 16;
        int32_t b_val = (int32_t)tb << 16;

        int32_t r_step = (((int32_t)br - (int32_t)tr) << 16) / h;
        int32_t g_step = (((int32_t)bg - (int32_t)tg) << 16) / h;
        int32_t b_step = (((int32_t)bb - (int32_t)tb) << 16) / h;

        /* ⚡ BOLT Optimization: Calculate row start once and increment pointer */
        uint32_t* row = omni_fb + (y * omni_pitch_div4) + x;

        for (int i = 0; i < h; i++) {
            uint32_t r = (uint32_t)(r_val >> 16);
            uint32_t g = (uint32_t)(g_val >> 16);
            uint32_t b = (uint32_t)(b_val >> 16);

            uint32_t color = 0xFF000000 | (r << 16) | (g << 8) | b;
            
            memset32(row, color, w);
            row += omni_pitch_div4;

            row += omni_pitch_div4;

            r_val += r_step;
            g_val += g_step;
            b_val += b_step;
        }
    }
}

/* ========================================================================= */
/* Image Loading from Initrd                                                  */
/* ========================================================================= */

void omni_draw_image_from_path(const char* path, int32_t x, int32_t y) {
    uint32_t fileSize = 0;
    uint8_t* fileData = (uint8_t*)initrd_read_file(path, &fileSize);

    if (!fileData) return;

    /* Check PNG Magic */
    if (fileSize > 8 && fileData[0] == 0x89 && fileData[1] == 'P') {
        upng_t* upng = upng_new_from_bytes(fileData, fileSize);
        if (upng) {
            upng_decode(upng);
            if (upng_get_error(upng) == UPNG_EOK) {
                unsigned width = upng_get_width(upng);
                unsigned height = upng_get_height(upng);
                const uint8_t* buffer = upng_get_buffer(upng);
                unsigned format = upng_get_format(upng);

                dirty_mark(x, y, width, height);

                if (omni_bpp == 32) {
                    /* ⚡ BOLT Optimization: Pre-calculate clipping bounds to remove per-pixel checks */
                    int32_t img_w = (int32_t)width;
                    int32_t img_h = (int32_t)height;

                    /* Vertical clipping (using int64_t to prevent overflow) */
                    int32_t start_y = 0;
                    int32_t end_y = img_h;

                    if (y < 0) start_y = -y;

                    if ((int64_t)y + img_h > omni_height) {
                        end_y = (int32_t)((int64_t)omni_height - y);
                    }

                    /* Horizontal clipping */
                    int32_t start_x = 0;
                    int32_t end_x = img_w;

                    if (x < 0) start_x = -x;

                    if ((int64_t)x + img_w > omni_width) {
                        end_x = (int32_t)((int64_t)omni_width - x);
                    }

                    if (start_y < end_y && start_x < end_x) {
                        for (int py = start_y; py < end_y; py++) {
                            int dest_y = y + py;
                            /* No check needed: dest_y is guaranteed valid */

                            uint32_t* dest_row = omni_fb + (dest_y * omni_pitch_div4);

                            /* Process clipped row */
                            if (format == UPNG_RGBA8) {
                                const uint8_t* src_row_start = buffer + (py * width * 4);
                                for (int px = start_x; px < end_x; px++) {
                                    int dest_x = x + px;
                                    /* No check needed: dest_x is guaranteed valid */

                                    const uint8_t* src = src_row_start + (px * 4);
                                    uint8_t a = src[3];

                                    if (a < 10) continue;

                                    uint8_t r = src[0];
                                    uint8_t g = src[1];
                                    uint8_t b = src[2];

                                    uint32_t color = (0xFF << 24) | (r << 16) | (g << 8) | b;
                                    if (a > 240) {
                                        dest_row[dest_x] = color;
                                    } else {
                                        dest_row[dest_x] = _alpha_blend(dest_row[dest_x], color, a);
                                    }
                                }
                            } else if (format == UPNG_RGB8) {
                                const uint8_t* src_row_start = buffer + (py * width * 3);
                                for (int px = start_x; px < end_x; px++) {
                                    int dest_x = x + px;
                                    /* No check needed */

                                    const uint8_t* src = src_row_start + (px * 3);
                                    uint32_t color = (0xFF << 24) |
                                                     (src[0] << 16) |
                                                     (src[1] << 8) |
                                                     src[2];
                                    dest_row[dest_x] = color;
                                }
                            }
                        }
                    }
                }
            }
            upng_free(upng);
        }
    } else {
        /* Raw format */
        if (fileSize < 8) return;
        uint32_t w = *(uint32_t*)fileData;
        uint32_t h = *(uint32_t*)(fileData + 4);
        const uint32_t* pixels = (const uint32_t*)(fileData + 8);

        omni_draw_bitmap(x, y, w, h, pixels, 0);
    }
}

/* ========================================================================= */
/* Utility: Circle Drawing (Midpoint Algorithm)                               */
/* ========================================================================= */

void omni_fill_circle(int32_t cx, int32_t cy, int32_t radius, uint32_t color) {
    if (radius <= 0) return;
    
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;
    
    while (x <= y) {
        /* Draw horizontal spans for filled circle */
        omni_fill_rect(cx - y, cy + x, 2 * y + 1, 1, color);
        omni_fill_rect(cx - y, cy - x, 2 * y + 1, 1, color);
        omni_fill_rect(cx - x, cy + y, 2 * x + 1, 1, color);
        omni_fill_rect(cx - x, cy - y, 2 * x + 1, 1, color);
        
        if (d < 0) {
            d += 4 * x + 6;
        } else {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}
