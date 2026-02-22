#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

// Mock kernel headers
#include "types.h"
#include "boot.h"
#include "framebuffer.h"

// Mock dependencies
void serial_write_string(const char* s) {
    // printf("%s", s); // Suppress output for benchmark
}

void hal_mem_map(uint64_t phys, uint64_t virt, uint32_t flags) {
    // No-op
}

void* kmalloc(size_t size) {
    return malloc(size);
}

void kfree(void* ptr) {
    free(ptr);
}

// Needed by framebuffer.c -> include/string.h -> kernel/string.c
// But if we compile with kernel/string.c, we are good.

int main() {
    printf("Starting Framebuffer Benchmark...\n");

    // Setup Mock Framebuffer
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t bpp = 32;
    uint32_t pitch = width * 4;
    size_t fb_size = height * pitch;

    void* fake_fb = malloc(fb_size);
    if (!fake_fb) {
        fprintf(stderr, "Failed to allocate fake framebuffer\n");
        return 1;
    }
    memset(fake_fb, 0, fb_size);

    boot_info_t info;
    memset(&info, 0, sizeof(info));
    info.fb_addr = (uint64_t)fake_fb;
    info.fb_width = width;
    info.fb_height = height;
    info.fb_pitch = pitch;
    info.fb_bpp = bpp;

    // Initialize Framebuffer Driver
    framebuffer_init(&info);
    framebuffer_enable_double_buffer();

    // Benchmark Loop
    clock_t start = clock();
    int iterations = 2000;

    // Simulate moving a window or cursor
    for (int i = 0; i < iterations; i++) {
        uint32_t x = (i * 5) % (width - 50);
        uint32_t y = (i * 3) % (height - 50);

        // Draw something (updates back buffer)
        framebuffer_rect(x, y, 50, 50, 0xFFFFFFFF);

        // Flush to front buffer (The critical part)
        framebuffer_flush();
    }

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Benchmark Results:\n");
    printf("  Iterations: %d\n", iterations);
    printf("  Time: %.6f s\n", time_spent);
    printf("  FPS: %.2f\n", iterations / time_spent);
    printf("  Avg Time per Flush: %.6f ms\n", (time_spent * 1000.0) / iterations);

    free(fake_fb);
    // Note: back_buffer allocated by kmalloc/malloc is not freed here explicitly
    // but OS will clean up process memory.

    return 0;
}
