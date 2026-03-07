#include <gfx/window.h>
#include <gfx/gfx.h>
#include <mm.h>
#include <string.h>
#include <framebuffer.h>
#include <timer.h>

static window_t* window_list = NULL;
static int32_t next_window_id = 1;

void compositor_init(void) {
    window_list = NULL;
}

window_t* window_create(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t flags) {
    window_t* win = (window_t*)kmalloc(sizeof(window_t));
    if (!win) return NULL;

    win->buffer = (uint32_t*)kmalloc(width * height * 4);
    if (!win->buffer) {
        kfree(win);
        return NULL;
    }

    memset(win->buffer, 0, width * height * 4);
    win->id = next_window_id++;
    win->x = x;
    win->y = y;
    win->width = width;
    win->height = height;
    win->flags = flags;
    win->z_order = 0;
    win->next = NULL;

    return win;
}

void window_destroy(window_t* win) {
    if (!win) return;
    if (win->buffer) kfree(win->buffer);
    kfree(win);
}

void compositor_add_window(window_t* win) {
    if (!win) return;

    /* Simple append to front (top) for now */
    win->next = window_list;
    window_list = win;
}

void compositor_remove_window(window_t* win) {
    if (!window_list || !win) return;

    if (window_list == win) {
        window_list = win->next;
        return;
    }

    window_t* curr = window_list;
    while (curr->next) {
        if (curr->next == win) {
            curr->next = win->next;
            return;
        }
        curr = curr->next;
    }
}

void window_invalidate(window_t* win) {
    if (!win) return;
    gfx_add_dirty_rect(win->x, win->y, win->width, win->height);
}

static void draw_window(window_t* win) {
    if (!(win->flags & WIN_VISIBLE)) return;

    uint32_t fb_w = framebuffer_get_width();
    uint32_t fb_h = framebuffer_get_height();

    /* Clip window to screen */
    int32_t x_start = win->x;
    int32_t y_start = win->y;
    int32_t x_end = win->x + win->width;
    int32_t y_end = win->y + win->height;

    if (x_start >= (int32_t)fb_w || y_start >= (int32_t)fb_h || x_end <= 0 || y_end <= 0) return;

    int32_t draw_x_start = x_start < 0 ? 0 : x_start;
    int32_t draw_y_start = y_start < 0 ? 0 : y_start;
    int32_t draw_x_end = x_end > (int32_t)fb_w ? (int32_t)fb_w : x_end;
    int32_t draw_y_end = y_end > (int32_t)fb_h ? (int32_t)fb_h : y_end;

    /* Optimized 32-bit path */
    if (framebuffer_get_bpp() == 32) {
        uint32_t* fb_buf = framebuffer_get_buffer();
        uint32_t fb_pitch = framebuffer_get_pitch();
        int32_t width = draw_x_end - draw_x_start;
        int32_t height = draw_y_end - draw_y_start;

        int32_t win_y_start = draw_y_start - win->y;
        int32_t win_x_start = draw_x_start - win->x;

        /* ⚡ BOLT Optimization: Hoist pointer arithmetic out of the inner loop
           and unroll the alpha-blend pixel copy loop by 4x for speed. */
        uint8_t* dest_row = (uint8_t*)fb_buf + (draw_y_start * fb_pitch) + (draw_x_start * 4);
        uint32_t* src_row = win->buffer + (win_y_start * win->width) + win_x_start;

        if (win->flags & WIN_OPAQUE) {
            /* Fast path for completely opaque windows: skip per-pixel transparency check */
            if (width * 4 == (int32_t)fb_pitch && width == win->width) {
                /* If window perfectly spans the framebuffer, do one massive copy */
                memcpy(dest_row, src_row, width * height * 4);
            } else {
                /* Copy row by row */
                for (int32_t i = 0; i < height; i++) {
                    memcpy(dest_row, src_row, width * 4);
                    dest_row += fb_pitch;
                    src_row += win->width;
                }
            }
        } else if (win->flags & WIN_GLASS) {
            /* Glassmorphism mode */
            for (int32_t i = 0; i < height; i++) {
                uint32_t* dest = (uint32_t*)dest_row;
                uint32_t* src = src_row;

                for (int32_t j = 0; j < width; j++) {
                    uint32_t sc = src[j];
                    if (sc != 0) {
                        uint32_t a = (sc >> 24) & 0xFF;
                        if (a == 255) {
                            dest[j] = sc;
                        } else if (a > 0) {
                            uint32_t dc = dest[j];

                            /* ⚡ BOLT Optimization: SWAR (SIMD Within A Register) Alpha Blending.
                               Process Red and Blue channels together in a single 32-bit operation,
                               reducing multiplications from 6 to 4 per pixel and eliminating
                               costly bitwise shifts for individual component extraction. */
                            uint32_t inv_a = 255 - a;
                            uint32_t rb = (((sc & 0xFF00FF) * a + (dc & 0xFF00FF) * inv_a) >> 8) & 0xFF00FF;
                            uint32_t g = (((sc & 0x00FF00) * a + (dc & 0x00FF00) * inv_a) >> 8) & 0x00FF00;

                            dest[j] = 0xFF000000 | rb | g;
                        }
                    }
                }

                /* Advance pointers by one row */
                dest_row += fb_pitch;
                src_row += win->width;
            }
        } else {
            for (int32_t i = 0; i < height; i++) {
                uint32_t* dest = (uint32_t*)dest_row;
                uint32_t* src = src_row;

                int32_t j = 0;
                /* Unrolled loop (4x) */
                for (; j <= width - 4; j += 4) {
                    if (src[j] != 0) dest[j] = src[j];
                    if (src[j+1] != 0) dest[j+1] = src[j+1];
                    if (src[j+2] != 0) dest[j+2] = src[j+2];
                    if (src[j+3] != 0) dest[j+3] = src[j+3];
                }

                /* Remainder */
                for (; j < width; j++) {
                    if (src[j] != 0) {
                        dest[j] = src[j];
                    }
                    dest++;
                }

                /* Advance pointers by one row */
                dest_row += fb_pitch;
                src_row += win->width;
            }
        }
    } else {
        /* Fallback for other depths */
        if (win->flags & WIN_GLASS) {
            for (int32_t y = draw_y_start; y < draw_y_end; y++) {
                for (int32_t x = draw_x_start; x < draw_x_end; x++) {
                    int32_t win_x = x - win->x;
                    int32_t win_y = y - win->y;

                    uint32_t sc = win->buffer[win_y * win->width + win_x];
                    if (sc != 0) {
                        uint32_t a = (sc >> 24) & 0xFF;
                        if (a == 255) {
                            framebuffer_putpixel(x, y, sc);
                        } else if (a > 0) {
                            /* Fallback alpha blending */
                            uint32_t dc = 0; /* Fallback: we cannot read screen pixels easily, but the prompt says use >> 8 */
                            /* Since we don't have framebuffer_getpixel, we approximate or assume black background
                               for the 16bpp fallback, but wait, the prompt specifically mandates WIN_GLASS checking
                               and >> 8 shift alpha blending. I will provide the formula assuming dc is 0. */
                            uint32_t inv_a = 255 - a;
                            uint32_t rb = (((sc & 0xFF00FF) * a + (dc & 0xFF00FF) * inv_a) >> 8) & 0xFF00FF;
                            uint32_t g = (((sc & 0x00FF00) * a + (dc & 0x00FF00) * inv_a) >> 8) & 0x00FF00;
                            framebuffer_putpixel(x, y, 0xFF000000 | rb | g);
                        }
                    }
                }
            }
        } else {
            for (int32_t y = draw_y_start; y < draw_y_end; y++) {
                for (int32_t x = draw_x_start; x < draw_x_end; x++) {
                    /* Map screen (x,y) to window (win_x, win_y) */
                    int32_t win_x = x - win->x;
                    int32_t win_y = y - win->y;

                    uint32_t color = win->buffer[win_y * win->width + win_x];

                    /* Simple Alpha: If 0, transparent */
                    if (color != 0) {
                        framebuffer_putpixel(x, y, color);
                    }
                }
            }
        }
    }
}

static void draw_windows_recursive(window_t* w) {
    if (!w) return;
    draw_windows_recursive(w->next);
    draw_window(w);
}

int display_sleep_mode = 0;
uint64_t last_input_ticks = 0;
int dark_mode_enabled = 1;

void flux_set_theme(int is_dark) {
    dark_mode_enabled = is_dark;
    gfx_add_dirty_rect(0, 0, framebuffer_get_width(), framebuffer_get_height());
    gfx_present();
}

void compositor_wake(void) {
    last_input_ticks = timer_get_ticks();
    if (display_sleep_mode) {
        display_sleep_mode = 0;
        gfx_add_dirty_rect(0, 0, framebuffer_get_width(), framebuffer_get_height());
        gfx_present();
    }
}

void compositor_render(void) {
    uint64_t current_ticks = timer_get_ticks();

    if (!display_sleep_mode && (current_ticks - last_input_ticks > 30 * TIMER_HZ)) {
        display_sleep_mode = 1;
        framebuffer_rect(0, 0, framebuffer_get_width(), framebuffer_get_height(), 0xFF000000);
        gfx_add_dirty_rect(0, 0, framebuffer_get_width(), framebuffer_get_height());
        gfx_present();
        return;
    }

    if (display_sleep_mode) {
        return;
    }

    /* Clear screen to dark gray or light gray depending on dark mode */
    uint32_t bg_color = dark_mode_enabled ? 0xFF202020 : 0xFFE0E0E0;
    framebuffer_rect(0, 0, framebuffer_get_width(), framebuffer_get_height(), bg_color);

    /* Draw windows back to front */
    draw_windows_recursive(window_list);

    /* Mark whole screen dirty for present */
    gfx_add_dirty_rect(0, 0, framebuffer_get_width(), framebuffer_get_height());
    gfx_present();
}
