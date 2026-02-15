#include <ui/omni.h>
#include <framebuffer.h>
#include <string.h>
#include "upng.h"
#include <fs/initrd.h>
#include <mm.h>
#include <serial.h>

/* Global cached values for performance */
static uint32_t* omni_fb = 0;
static uint32_t omni_pitch = 0; /* In BYTES */
static uint32_t omni_width = 0;
static uint32_t omni_height = 0;
static uint32_t omni_bpp = 0;
static uint32_t omni_pitch_div4 = 0; /* Pitch in uint32_t elements */

extern const uint8_t font8x16[];

void omni_init(void) {
    omni_fb = framebuffer_get_buffer();
    omni_pitch = framebuffer_get_pitch();
    omni_width = framebuffer_get_width();
    omni_height = framebuffer_get_height();
    omni_bpp = framebuffer_get_bpp();

    if (omni_bpp == 32) {
        omni_pitch_div4 = omni_pitch / 4;
    }

    serial_write_string("[OMNI] Engine Initialized. FB: ");
    char buf[32];
    utoa_hex_s((uint64_t)omni_fb, buf, 32);
    serial_write_string(buf);
    serial_write_string("\n");
}

uint32_t omni_get_width(void) { return omni_width; }
uint32_t omni_get_height(void) { return omni_height; }

/* Inlined pixel put for internal use */
static inline void _omni_put_pixel_fast(int x, int y, uint32_t color) {
    omni_fb[y * omni_pitch_div4 + x] = color;
}

void omni_draw_pixel(int32_t x, int32_t y, uint32_t color) {
    /* Update pointers if double buffering switched buffers */
    omni_fb = framebuffer_get_buffer();

    if (x < 0 || y < 0 || (uint32_t)x >= omni_width || (uint32_t)y >= omni_height) return;

    if (omni_bpp == 32) {
        _omni_put_pixel_fast(x, y, color);
    } else {
        framebuffer_putpixel(x, y, color);
    }
}

void omni_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    omni_fb = framebuffer_get_buffer();

    /* Clipping */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }

    if ((uint32_t)x >= omni_width || (uint32_t)y >= omni_height) return;
    if ((uint32_t)(x + w) > omni_width) w = omni_width - x;
    if ((uint32_t)(y + h) > omni_height) h = omni_height - y;

    if (w <= 0 || h <= 0) return;

    if (omni_bpp == 32) {
        uint32_t* row = omni_fb + (y * omni_pitch_div4) + x;
        for (int i = 0; i < h; i++) {
            memset32(row, color, w);
            row += omni_pitch_div4;
        }
    } else {
        /* Fallback */
        framebuffer_rect(x, y, w, h, color);
    }
}

void omni_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    /* Top */
    omni_fill_rect(x, y, w, 1, color);
    /* Bottom */
    omni_fill_rect(x, y + h - 1, w, 1, color);
    /* Left */
    omni_fill_rect(x, y, 1, h, color);
    /* Right */
    omni_fill_rect(x + w - 1, y, 1, h, color);
}

void omni_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2;
    int e2;

    while (1) {
        omni_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 < dy) { err += dx; y0 += sy; }
    }
}

void omni_draw_char(int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg) {
    omni_fb = framebuffer_get_buffer();

    if (x < 0 || y < 0 || (uint32_t)(x + 8) > omni_width || (uint32_t)(y + 16) > omni_height) return;

    if (omni_bpp == 32) {
        const uint8_t* glyph = &font8x16[(unsigned char)c * 16];
        uint32_t* row_ptr = omni_fb + (y * omni_pitch_div4) + x;

        for (int i = 0; i < 16; i++) {
            uint8_t bits = glyph[i];
            /* Unrolled for speed */
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

void omni_draw_string(const rect_t* clip, int32_t x, int32_t y, const char* str, uint32_t fg, uint32_t bg) {
    if (!str) return;
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

void omni_draw_bitmap(int32_t x, int32_t y, int32_t w, int32_t h, const uint32_t* data, int flags) {
    if (!data) return;
    omni_fb = framebuffer_get_buffer();

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

    if (omni_bpp == 32) {
        uint32_t* dest_row = omni_fb + (y * omni_pitch_div4) + x;
        const uint32_t* src_row = data + (src_y * stride) + src_x;

         for (int i = 0; i < h; i++) {
             for (int j = 0; j < w; j++) {
                 uint32_t color = src_row[j];

                 if (flags & OMNI_BITMAP_ALPHA) {
                     /* Check alpha (highest byte) */
                     uint8_t alpha = (color >> 24) & 0xFF;
                     if (alpha > 128) { /* Simple threshold */
                         dest_row[j] = color;
                     }
                 } else {
                     dest_row[j] = color;
                 }
             }
             dest_row += omni_pitch_div4;
             src_row += stride;
         }
    }
}

/* Helper for images that handles decoding and clipping properly */
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

                omni_fb = framebuffer_get_buffer();

                /* Simple implementation: Loop and putpixel (still faster than syscall overhead)
                   or optimized row blit. */

                for (unsigned py = 0; py < height; py++) {
                    int dest_y = y + py;
                    if (dest_y < 0 || (uint32_t)dest_y >= omni_height) continue;

                    uint32_t* dest_row = omni_fb + (dest_y * omni_pitch_div4);

                    for (unsigned px = 0; px < width; px++) {
                        int dest_x = x + px;
                        if (dest_x < 0 || (uint32_t)dest_x >= omni_width) continue;

                        uint32_t color = 0;
                        if (format == UPNG_RGBA8) {
                            unsigned idx = (py * width + px) * 4;
                            uint8_t r = buffer[idx];
                            uint8_t g = buffer[idx+1];
                            uint8_t b = buffer[idx+2];
                            uint8_t a = buffer[idx+3];

                            if (a < 10) continue; /* Transparent */

                            color = (0xFF << 24) | (r << 16) | (g << 8) | b;
                        } else if (format == UPNG_RGB8) {
                            unsigned idx = (py * width + px) * 3;
                            uint8_t r = buffer[idx];
                            uint8_t g = buffer[idx+1];
                            uint8_t b = buffer[idx+2];
                            color = (0xFF << 24) | (r << 16) | (g << 8) | b;
                        }

                        dest_row[dest_x] = color;
                    }
                }
            }
            upng_free(upng);
        }
    } else {
        /* Raw format logic if needed */
        /* width(4) + height(4) + pixels... */
        if (fileSize < 8) return;
        uint32_t w = *(uint32_t*)fileData;
        uint32_t h = *(uint32_t*)(fileData + 4);
        const uint32_t* pixels = (const uint32_t*)(fileData + 8);

        omni_draw_bitmap(x, y, w, h, pixels, 0);
    }
}
