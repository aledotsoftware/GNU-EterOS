#include <gfx/window.h>
#include <gfx/gfx.h>
#include <mm.h>
#include <string.h>
#include <framebuffer.h>

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

static void draw_windows_recursive(window_t* w) {
    if (!w) return;
    draw_windows_recursive(w->next);
    draw_window(w);
}

void compositor_render(void) {
    /* Clear screen to dark gray */
    framebuffer_rect(0, 0, framebuffer_get_width(), framebuffer_get_height(), 0xFF202020);

    /* Draw windows back to front */
    draw_windows_recursive(window_list);

    /* Mark whole screen dirty for present */
    gfx_add_dirty_rect(0, 0, framebuffer_get_width(), framebuffer_get_height());
    gfx_present();
}
