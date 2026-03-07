#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#define OMNI_WIDTH 1920
#define OMNI_HEIGHT 1080
#define FB_PITCH (OMNI_WIDTH * 4)

uint32_t fb_buf[OMNI_WIDTH * OMNI_HEIGHT];
uint32_t win_buffer[1024 * 768];

void draw_window_base(int draw_x_start, int draw_y_start, int width, int height, int win_x_start, int win_y_start, int win_width) {
    for (int32_t i = 0; i < height; i++) {
        uint32_t* dest = (uint32_t*)((uint8_t*)fb_buf + ((draw_y_start + i) * FB_PITCH) + (draw_x_start * 4));
        uint32_t* src = win_buffer + ((win_y_start + i) * win_width) + win_x_start;

        for (int32_t j = 0; j < width; j++) {
            uint32_t color = src[j];
            if (color != 0) {
                dest[j] = color;
            }
        }
        // Prevent opt
        __asm__ volatile("" : : "r"(dest) : "memory");
    }
}

void draw_window_opt(int draw_x_start, int draw_y_start, int width, int height, int win_x_start, int win_y_start, int win_width) {
    uint32_t* dest_row = (uint32_t*)((uint8_t*)fb_buf + (draw_y_start * FB_PITCH) + (draw_x_start * 4));
    uint32_t* src_row = win_buffer + (win_y_start * win_width) + win_x_start;

    int dest_stride = FB_PITCH / 4;
    int src_stride = win_width;

    for (int32_t i = 0; i < height; i++) {
        uint32_t* dest = dest_row;
        uint32_t* src = src_row;
        int32_t j = 0;

        // Unroll by 4
        for (; j <= width - 4; j += 4) {
            uint32_t c0 = src[j];
            uint32_t c1 = src[j+1];
            uint32_t c2 = src[j+2];
            uint32_t c3 = src[j+3];

            if (c0) dest[j] = c0;
            if (c1) dest[j+1] = c1;
            if (c2) dest[j+2] = c2;
            if (c3) dest[j+3] = c3;
        }

        // Remainder
        for (; j < width; j++) {
            uint32_t color = src[j];
            if (color != 0) {
                dest[j] = color;
            }
        }

        dest_row += dest_stride;
        src_row += src_stride;

        // Prevent opt
        __asm__ volatile("" : : "r"(dest) : "memory");
    }
}

int main() {
    int iterations = 1000;
    clock_t start, end;

    // Fill with non-zero
    memset(win_buffer, 0x11, sizeof(win_buffer));

    start = clock();
    for(int i=0; i<iterations; i++) {
        draw_window_base(100, 100, 1024, 768, 0, 0, 1024);
    }
    end = clock();
    printf("Base: %.4f s\n", (double)(end-start)/CLOCKS_PER_SEC);

    start = clock();
    for(int i=0; i<iterations; i++) {
        draw_window_opt(100, 100, 1024, 768, 0, 0, 1024);
    }
    end = clock();
    printf("Opt: %.4f s\n", (double)(end-start)/CLOCKS_PER_SEC);

    return 0;
}
