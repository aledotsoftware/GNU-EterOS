#define __ETEROS_HOST_TEST__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Use Linux time.h explicitly instead of kernel time.h, or avoid it */
#include <sys/time.h>

/* Mock Headers */
#include "../include/types.h"
#include "../include/io.h"
#include "../include/vga.h"
#include "../include/framebuffer.h"
#include "../include/boot.h"

/* Mock System Functions */
void outb(uint16_t port, uint8_t value) { (void)port; (void)value; }
void outw(uint16_t port, uint16_t value) { (void)port; (void)value; }
void outl(uint16_t port, uint32_t value) { (void)port; (void)value; }
uint8_t inb(uint16_t port) { (void)port; return 0; }
uint16_t inw(uint16_t port) { (void)port; return 0; }
uint32_t inl(uint16_t port) { (void)port; return 0; }
void io_wait(void) {}

void* kmalloc(size_t size) { return malloc(size); }
void kfree(void* ptr) { free(ptr); }
void* kcalloc(size_t n, size_t s) { return calloc(n, s); }
void serial_write_string(const char* s) { (void)s; }

int hal_mem_map(uint64_t phys, uint64_t virt, uint32_t flags) { return 0; }

/* Rename string functions */
#define memcpy eteros_memcpy
#define memset eteros_memset
#define memset32 eteros_memset32
#define memset16 eteros_memset16
#define memmove eteros_memmove
#define memcmp eteros_memcmp
#define strlen eteros_strlen
#define strncpy eteros_strncpy
#define strlcpy eteros_strlcpy
#define strcmp eteros_strcmp
#define itoa_s eteros_itoa_s
#define utoa_hex_s eteros_utoa_hex_s
#include "../kernel/string.c"

/* Rename to avoid conflict if any */
#define framebuffer_init original_framebuffer_init
#define framebuffer_flush_rect original_framebuffer_flush_rect
#include "../kernel/drivers/video/framebuffer.c"
#undef framebuffer_init
#undef framebuffer_flush_rect

/* Mock FB global variables */
void custom_framebuffer_init(void) {
    fb_width = 1024;
    fb_height = 768;
    fb_pitch = 1024 * 4;
    fb_bpp = 32;
    fb_size_bytes = fb_height * fb_pitch;

    fb_buffer = (uint32_t*)malloc(fb_size_bytes);
    back_buffer = (uint32_t*)malloc(fb_size_bytes);
    active_buffer = back_buffer;
    memset(fb_buffer, 0, fb_size_bytes);
    memset(back_buffer, 1, fb_size_bytes);
}

/* Original Implementation */
void framebuffer_flush_rect_unoptimized(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!back_buffer || !fb_buffer) return;
    if (x >= fb_width || y >= fb_height) return;
    if (x + w > fb_width) w = fb_width - x;
    if (y + h > fb_height) h = fb_height - y;
    if (w == 0 || h == 0) return;

    for (uint32_t i = 0; i < h; i++) {
        uint8_t* dest = (uint8_t*)fb_buffer + ((y + i) * fb_pitch) + (x * (fb_bpp / 8));
        uint8_t* src  = (uint8_t*)back_buffer + ((y + i) * fb_pitch) + (x * (fb_bpp / 8));
        size_t row_len = w * (fb_bpp / 8);

        eteros_memcpy(dest, src, row_len);
    }
}

/* Optimized Implementation */
void framebuffer_flush_rect_optimized(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!back_buffer || !fb_buffer) return;
    if (x >= fb_width || y >= fb_height) return;
    if (x + w > fb_width) w = fb_width - x;
    if (y + h > fb_height) h = fb_height - y;
    if (w == 0 || h == 0) return;

    size_t bytes_per_pixel = fb_bpp / 8;
    size_t row_len = w * bytes_per_pixel;
    uint8_t* dest = (uint8_t*)fb_buffer + (y * fb_pitch) + (x * bytes_per_pixel);
    uint8_t* src  = (uint8_t*)back_buffer + (y * fb_pitch) + (x * bytes_per_pixel);

    for (uint32_t i = 0; i < h; i++) {
        eteros_memcpy(dest, src, row_len);
        dest += fb_pitch;
        src += fb_pitch;
    }
}

double get_time() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec + t.tv_usec * 1e-6;
}

int main() {
    custom_framebuffer_init();

    int iterations = 10000;

    double start = get_time();
    for (int i = 0; i < iterations; i++) {
        framebuffer_flush_rect_unoptimized(100, 100, 400, 300);
    }
    double end = get_time();
    double time_unopt = end - start;

    start = get_time();
    for (int i = 0; i < iterations; i++) {
        framebuffer_flush_rect_optimized(100, 100, 400, 300);
    }
    end = get_time();
    double time_opt = end - start;

    printf("Unoptimized: %.6f s\n", time_unopt);
    printf("Optimized:   %.6f s\n", time_opt);

    return 0;
}
