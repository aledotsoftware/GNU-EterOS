#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define fb_width 1920
#define fb_height 1080
#define fb_pitch (1920 * 4)
#define fb_bpp 32

uint8_t* fb_buffer;
uint8_t* back_buffer;

void framebuffer_flush_rect_unoptimized(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    size_t bytes_per_pixel = fb_bpp / 8;
    size_t row_len = w * bytes_per_pixel;
    uint8_t* dest = (uint8_t*)fb_buffer + (y * fb_pitch) + (x * bytes_per_pixel);
    uint8_t* src  = (uint8_t*)back_buffer + (y * fb_pitch) + (x * bytes_per_pixel);

    for (uint32_t i = 0; i < h; i++) {
        memcpy(dest, src, row_len);
        dest += fb_pitch;
        src += fb_pitch;
    }
}

void framebuffer_flush_rect_optimized(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    size_t bytes_per_pixel = fb_bpp / 8;
    size_t row_len = w * bytes_per_pixel;
    uint8_t* dest = (uint8_t*)fb_buffer + (y * fb_pitch) + (x * bytes_per_pixel);
    uint8_t* src  = (uint8_t*)back_buffer + (y * fb_pitch) + (x * bytes_per_pixel);

    if (row_len == fb_pitch) {
        memcpy(dest, src, row_len * h);
    } else {
        for (uint32_t i = 0; i < h; i++) {
            memcpy(dest, src, row_len);
            dest += fb_pitch;
            src += fb_pitch;
        }
    }
}

int main() {
    fb_buffer = malloc(fb_pitch * fb_height);
    back_buffer = malloc(fb_pitch * fb_height);

    clock_t start, end;

    printf("--- Benchmarking Partial Screen Flush (1900x1060) ---\n");
    start = clock();
    for (int i = 0; i < 5000; i++) {
        framebuffer_flush_rect_unoptimized(10, 10, 1900, 1060);
    }
    end = clock();
    printf("Unoptimized took: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    start = clock();
    for (int i = 0; i < 5000; i++) {
        framebuffer_flush_rect_optimized(10, 10, 1900, 1060);
    }
    end = clock();
    printf("Optimized took: %f seconds\n\n", (double)(end - start) / CLOCKS_PER_SEC);

    printf("--- Benchmarking Full Screen Flush (1920x1080) ---\n");
    start = clock();
    for (int i = 0; i < 5000; i++) {
        framebuffer_flush_rect_unoptimized(0, 0, 1920, 1080);
    }
    end = clock();
    printf("Unoptimized took: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    start = clock();
    for (int i = 0; i < 5000; i++) {
        framebuffer_flush_rect_optimized(0, 0, 1920, 1080);
    }
    end = clock();
    printf("Optimized took: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    free(fb_buffer);
    free(back_buffer);
    return 0;
}
