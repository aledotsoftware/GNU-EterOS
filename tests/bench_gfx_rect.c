#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include <boot.h>


#define abs my_abs

int hal_mem_map(uint64_t phys_addr, uint64_t virt_addr, uint32_t flags) { return 0; }
void serial_write_string(const char* str) {}
void* kmalloc(size_t s) { return malloc(s); }

#include "../kernel/gfx/gfx.c"
#include "../kernel/drivers/video/framebuffer.c"

const uint8_t font8x16[4096] = {0};

int main() {
    printf("Starting benchmark for gfx_draw_rect...\n");

    fb_width = 800;
    fb_height = 600;
    fb_bpp = 32;
    fb_pitch = 800 * 4;
    active_buffer = malloc(800 * 600 * 4);

    struct timeval start, end;
    gettimeofday(&start, NULL);

    for (int i = 0; i < 10000; i++) {
        gfx_draw_rect(10, 10, 780, 580, 0xFFFFFFFF);
    }

    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    printf("Time for 10000 gfx_draw_rect calls: %.6f seconds\n", elapsed);

    if (elapsed < 1.0) {
        printf("Benchmark passed! Fast-path optimization active.\n");
        return 0;
    } else {
        printf("Benchmark failed! It was too slow.\n");
        return 1;
    }
}
