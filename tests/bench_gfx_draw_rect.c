#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

void gfx_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (h == 1) {
        // Fast horizontal
        volatile uint32_t c;
        for (int i=0; i<w; i++) {
           c = color;
        }
    } else {
        // Vertical or block
        volatile uint32_t c;
        for (int i=0; i<h; i++) {
            for (int j=0; j<w; j++) {
                c = color;
            }
        }
    }
}

void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    volatile uint32_t c = color;
}

static int32_t abs_val(int32_t x) { return x < 0 ? -x : x; }

void gfx_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    int dx = abs_val(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs_val(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    for (;;) {
        framebuffer_putpixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_draw_rect_unoptimized(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    gfx_draw_line(x, y, x + w - 1, y, color);
    gfx_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
    gfx_draw_line(x + w - 1, y + h - 1, x, y + h - 1, color);
    gfx_draw_line(x, y + h - 1, x, y, color);
}

void gfx_draw_rect_optimized(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    gfx_fill_rect(x, y, w, 1, color);
    gfx_fill_rect(x, y + h - 1, w, 1, color);
    gfx_fill_rect(x, y + 1, 1, h - 2, color);
    gfx_fill_rect(x + w - 1, y + 1, 1, h - 2, color);
}

int main() {
    clock_t start, end;

    start = clock();
    for (int i = 0; i < 50000; i++) {
        gfx_draw_rect_unoptimized(10, 10, 800, 600, 0xFFFFFFFF);
    }
    end = clock();
    printf("Unoptimized took: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    start = clock();
    for (int i = 0; i < 50000; i++) {
        gfx_draw_rect_optimized(10, 10, 800, 600, 0xFFFFFFFF);
    }
    end = clock();
    printf("Optimized took: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    return 0;
}
