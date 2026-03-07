#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#define OMNI_WIDTH 1920
#define OMNI_HEIGHT 1080
#define FB_PITCH (OMNI_WIDTH * 4)

uint32_t active_buffer[OMNI_WIDTH * OMNI_HEIGHT];

void fb_rect_base(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < h; i++) {
        uint32_t* row_ptr = (uint32_t*)((uint8_t*)active_buffer + ((y + i) * FB_PITCH));
        row_ptr += x;
        // memset32 replacement
        for (uint32_t j = 0; j < w; j++) {
            row_ptr[j] = color;
        }
        __asm__ volatile("" : : "r"(row_ptr) : "memory");
    }
}

void fb_rect_opt(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    // Optimization: Calculate base pointer once and increment
    uint8_t* row_addr = (uint8_t*)active_buffer + (y * FB_PITCH) + (x * 4);

    for (uint32_t i = 0; i < h; i++) {
        uint32_t* row_ptr = (uint32_t*)row_addr;

        // memset32
        for (uint32_t j = 0; j < w; j++) {
            row_ptr[j] = color;
        }
        row_addr += FB_PITCH;
        __asm__ volatile("" : : "r"(row_ptr) : "memory");
    }
}

int main() {
    int iterations = 10000;
    clock_t start, end;

    start = clock();
    for(int i=0; i<iterations; i++) {
        fb_rect_base(100, 100, 1024, 768, 0xFFFFFFFF);
    }
    end = clock();
    printf("Base: %.4f s\n", (double)(end-start)/CLOCKS_PER_SEC);

    start = clock();
    for(int i=0; i<iterations; i++) {
        fb_rect_opt(100, 100, 1024, 768, 0xFFFFFFFF);
    }
    end = clock();
    printf("Opt: %.4f s\n", (double)(end-start)/CLOCKS_PER_SEC);

    return 0;
}
