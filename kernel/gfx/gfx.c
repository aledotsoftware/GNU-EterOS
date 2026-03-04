#include <gfx/gfx.h>
#include <framebuffer.h>
#include <string.h>

/* Dirty Rectangle global (Union of all dirty areas) */
static rect_t dirty_rect = {0, 0, 0, 0};
static int dirty_rect_valid = 0; /* 0 = Empty, 1 = Valid */

/* Helper: Math functions */
static int32_t abs(int32_t x) { return x < 0 ? -x : x; }
static int32_t min(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t max(int32_t a, int32_t b) { return a > b ? a : b; }

void gfx_init(boot_info_t* boot_info) {
    /* Initialize low-level framebuffer */
    framebuffer_init(boot_info);

    /* Enable double buffering immediately */
    framebuffer_enable_double_buffer();

    /* Invalidate full screen initially */
    gfx_add_dirty_rect(0, 0, framebuffer_get_width(), framebuffer_get_height());
}

void gfx_add_dirty_rect(int32_t x, int32_t y, int32_t w, int32_t h) {
    if (w <= 0 || h <= 0) return;

    /* Clip input rect to screen bounds */
    int32_t screen_w = (int32_t)framebuffer_get_width();
    int32_t screen_h = (int32_t)framebuffer_get_height();

    if (x >= screen_w || y >= screen_h) return;
    if (x + w < 0 || y + h < 0) return;

    /* Clip logic */
    int32_t x2 = x + w;
    int32_t y2 = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > screen_w) x2 = screen_w;
    if (y2 > screen_h) y2 = screen_h;

    w = x2 - x;
    h = y2 - y;

    if (!dirty_rect_valid) {
        dirty_rect.x = x;
        dirty_rect.y = y;
        dirty_rect.w = w;
        dirty_rect.h = h;
        dirty_rect_valid = 1;
    } else {
        /* Union logic: Expand bounding box */
        int32_t old_x2 = dirty_rect.x + dirty_rect.w;
        int32_t old_y2 = dirty_rect.y + dirty_rect.h;

        int32_t new_x = min(dirty_rect.x, x);
        int32_t new_y = min(dirty_rect.y, y);
        int32_t new_x2 = max(old_x2, x2);
        int32_t new_y2 = max(old_y2, y2);

        dirty_rect.x = new_x;
        dirty_rect.y = new_y;
        dirty_rect.w = new_x2 - new_x;
        dirty_rect.h = new_y2 - new_y;
    }
}

void gfx_present(void) {
    if (!dirty_rect_valid) return;

    framebuffer_flush_rect(dirty_rect.x, dirty_rect.y, dirty_rect.w, dirty_rect.h);

    /* Reset dirty rect */
    dirty_rect_valid = 0;
    dirty_rect.x = 0;
    dirty_rect.y = 0;
    dirty_rect.w = 0;
    dirty_rect.h = 0;
}

void gfx_put_pixel(int32_t x, int32_t y, uint32_t color) {
    framebuffer_putpixel((uint32_t)x, (uint32_t)y, color);
    /* Auto-dirty single pixel? Usually inefficient.
       Let the caller dirty regions or batch it.
       BUT the prompt implies automatic tracking?
       "Interceptar los eventos de la UI para calcular..."
       Since this is a primitive, we should probably not auto-dirty every pixel for performance unless asked.
       Wait, "Intercept events...".
       For now, let's assume primitives DO dirty the area, because otherwise existing code calling primitives won't update.
    */
    gfx_add_dirty_rect(x, y, 1, 1);
}

void gfx_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    /* Dirty the bounding box of the line */
    int32_t min_x = min(x0, x1);
    int32_t min_y = min(y0, y1);
    int32_t max_x = max(x0, x1);
    int32_t max_y = max(y0, y1);

    /* ⚡ BOLT Optimization: Fast path for horizontal and vertical lines */
    if (y0 == y1) {
        gfx_fill_rect(min_x, y0, (max_x - min_x) + 1, 1, color);
        return;
    }
    if (x0 == x1) {
        gfx_fill_rect(x0, min_y, 1, (max_y - min_y) + 1, color);
        return;
    }

    gfx_add_dirty_rect(min_x, min_y, (max_x - min_x) + 1, (max_y - min_y) + 1);

    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    for (;;) {
        framebuffer_putpixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (w <= 0 || h <= 0) return;

    /* ⚡ BOLT Optimization: Use gfx_fill_rect for horizontal/vertical lines
       instead of pixel-by-pixel gfx_draw_line. This leverages memset32 fast paths. */

    /* 1px thick edge cases */
    if (w == 1 || h == 1) {
        gfx_fill_rect(x, y, w, h, color);
        return;
    }

    gfx_fill_rect(x, y, w, 1, color);                  /* Top */
    gfx_fill_rect(x, y + h - 1, w, 1, color);          /* Bottom */

    if (h > 2) {
        gfx_fill_rect(x, y + 1, 1, h - 2, color);          /* Left */
        gfx_fill_rect(x + w - 1, y + 1, 1, h - 2, color);  /* Right */
    /* ⚡ BOLT Optimization: Use gfx_fill_rect directly for the 4 edges.
       This bypasses the function overhead of gfx_draw_line and takes
       advantage of optimized contiguous memory block operations. */

    /* Top edge */
    gfx_fill_rect(x, y, w, 1, color);

    /* Bottom edge */
    if (h > 1) {
        gfx_fill_rect(x, y + h - 1, w, 1, color);
    }

    /* Left and right edges (excluding corners already drawn by top/bottom) */
    if (h > 2) {
        gfx_fill_rect(x, y + 1, 1, h - 2, color);
        if (w > 1) {
            gfx_fill_rect(x + w - 1, y + 1, 1, h - 2, color);
    /* ⚡ BOLT Optimization: Use gfx_fill_rect for the 4 edges instead of gfx_draw_line.
       This avoids Bresenham's overhead entirely and utilizes fast-path memory operations. */
    gfx_fill_rect(x, y, w, 1, color); /* Top */
    if (h > 1) {
        gfx_fill_rect(x, y + h - 1, w, 1, color); /* Bottom */
    }
    if (h > 2) {
        gfx_fill_rect(x, y + 1, 1, h - 2, color); /* Left */
        if (w > 1) {
            gfx_fill_rect(x + w - 1, y + 1, 1, h - 2, color); /* Right */
        }
    }
}

void gfx_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    /* Use optimized framebuffer function */
    framebuffer_rect((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, color);

    /* Mark dirty */
    gfx_add_dirty_rect(x, y, w, h);
}

void gfx_draw_pixels(const gfx_point_t* pixels, size_t count) {
    if (!pixels || count == 0) return;

    int32_t min_x = 0x7FFFFFFF;
    int32_t min_y = 0x7FFFFFFF;
    int32_t max_x = -0x80000000;
    int32_t max_y = -0x80000000;

    uint32_t screen_w = framebuffer_get_width();
    uint32_t screen_h = framebuffer_get_height();
    uint32_t fb_bpp = framebuffer_get_bpp();
    uint32_t* fb_buf = framebuffer_get_buffer();
    uint32_t fb_pitch = framebuffer_get_pitch();

    /* ⚡ BOLT Optimization: Bypass framebuffer_putpixel for 32bpp fast path */
    if (fb_buf && fb_bpp == 32) {
        for (size_t i = 0; i < count; i++) {
            int32_t px = pixels[i].x;
            int32_t py = pixels[i].y;

            /* Safety check: Clip to screen bounds */
            if (px < 0 || px >= (int32_t)screen_w || py < 0 || py >= (int32_t)screen_h) {
                continue;
            }

            /* Direct memory access */
            uint32_t* pixel_addr = (uint32_t*)((uint8_t*)fb_buf + (py * fb_pitch) + (px * 4));
            *pixel_addr = pixels[i].color;

            /* Update bounding box */
            if (px < min_x) min_x = px;
            if (px > max_x) max_x = px;
            if (py < min_y) min_y = py;
            if (py > max_y) max_y = py;
        }
    } else {
        /* Fallback for other depths */
        for (size_t i = 0; i < count; i++) {
            int32_t px = pixels[i].x;
            int32_t py = pixels[i].y;

            /* Safety check: Clip to screen bounds */
            if (px < 0 || px >= (int32_t)screen_w || py < 0 || py >= (int32_t)screen_h) {
                continue;
            }

            /* Draw directly to framebuffer (avoids repeated dirty tracking per pixel) */
            framebuffer_putpixel((uint32_t)px, (uint32_t)py, pixels[i].color);

            /* Update bounding box */
            if (px < min_x) min_x = px;
            if (px > max_x) max_x = px;
            if (py < min_y) min_y = py;
            if (py > max_y) max_y = py;
        }
    }

    /* Add single dirty rect for all pixels */
    if (max_x >= min_x && max_y >= min_y) {
        gfx_add_dirty_rect(min_x, min_y, (max_x - min_x) + 1, (max_y - min_y) + 1);
    }
}
