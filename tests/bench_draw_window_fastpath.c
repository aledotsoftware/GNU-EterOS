#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define fb_pitch (1920 * 4)

uint8_t* fb_buf;
uint32_t* win_buffer;

void draw_window_unoptimized(int32_t draw_y_start, int32_t draw_x_start, int32_t win_y_start, int32_t win_x_start, int32_t width, int32_t height, int32_t win_width, uint32_t flags) {
    uint8_t* dest_row = (uint8_t*)fb_buf + (draw_y_start * fb_pitch) + (draw_x_start * 4);
    uint32_t* src_row = win_buffer + (win_y_start * win_width) + win_x_start;

    for (int32_t i = 0; i < height; i++) {
        uint32_t* dest = (uint32_t*)dest_row;
        uint32_t* src = src_row;

        int32_t j = 0;
        for (; j <= width - 4; j += 4) {
            if (src[j] != 0) dest[j] = src[j];
            if (src[j+1] != 0) dest[j+1] = src[j+1];
            if (src[j+2] != 0) dest[j+2] = src[j+2];
            if (src[j+3] != 0) dest[j+3] = src[j+3];
        }

        for (; j < width; j++) {
            if (src[j] != 0) {
                dest[j] = src[j];
            }
            dest++;
        }

        dest_row += fb_pitch;
        src_row += win_width;
    }
}

void draw_window_optimized(int32_t draw_y_start, int32_t draw_x_start, int32_t win_y_start, int32_t win_x_start, int32_t width, int32_t height, int32_t win_width, uint32_t flags) {
    uint8_t* dest_row = (uint8_t*)fb_buf + (draw_y_start * fb_pitch) + (draw_x_start * 4);
    uint32_t* src_row = win_buffer + (win_y_start * win_width) + win_x_start;

    if (flags & 4) { // WIN_OPAQUE = (1 << 2) = 4
        if (width * 4 == fb_pitch && width == win_width) {
            memcpy(dest_row, src_row, width * height * 4);
        } else {
            for (int32_t i = 0; i < height; i++) {
                memcpy(dest_row, src_row, width * 4);
                dest_row += fb_pitch;
                src_row += win_width;
            }
        }
    } else {
        for (int32_t i = 0; i < height; i++) {
            uint32_t* dest = (uint32_t*)dest_row;
            uint32_t* src = src_row;

            int32_t j = 0;
            for (; j <= width - 4; j += 4) {
                if (src[j] != 0) dest[j] = src[j];
                if (src[j+1] != 0) dest[j+1] = src[j+1];
                if (src[j+2] != 0) dest[j+2] = src[j+2];
                if (src[j+3] != 0) dest[j+3] = src[j+3];
            }

            for (; j < width; j++) {
                if (src[j] != 0) {
                    dest[j] = src[j];
                }
                dest++;
            }

            dest_row += fb_pitch;
            src_row += win_width;
        }
    }
}

int main() {
    fb_buf = malloc(fb_pitch * 1080);
    win_buffer = malloc(800 * 600 * 4);
    for (int i = 0; i < 800 * 600; i++) {
        win_buffer[i] = 0xFFFFFFFF; // Solid opaque window
    }

    clock_t start, end;

    start = clock();
    for (int i = 0; i < 5000; i++) {
        draw_window_unoptimized(10, 10, 0, 0, 800, 600, 800, 4);
    }
    end = clock();
    printf("Unoptimized (Alpha Blend) took: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    start = clock();
    for (int i = 0; i < 5000; i++) {
        draw_window_optimized(10, 10, 0, 0, 800, 600, 800, 4);
    }
    end = clock();
    printf("Optimized (Memcpy fastpath) took: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    free(fb_buf);
    free(win_buffer);

    return 0;
}
