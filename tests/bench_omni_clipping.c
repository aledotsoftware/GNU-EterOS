#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>

#define OMNI_WIDTH 1920
#define OMNI_HEIGHT 1080
#define IMG_WIDTH 1024
#define IMG_HEIGHT 768

// Baseline: Per-pixel bounds checking (as in original code)
// Returns number of pixels drawn to verify correctness
long long draw_baseline(int x, int y, int width, int height) {
    long long drawn_pixels = 0;

    // Original loop structure
    for (int py = 0; py < height; py++) {
        int dest_y = y + py;
        if (dest_y < 0 || dest_y >= OMNI_HEIGHT) continue;

        for (int px = 0; px < width; px++) {
            int dest_x = x + px;
            if (dest_x < 0 || dest_x >= OMNI_WIDTH) continue;

            // Simulate pixel access and blend
            drawn_pixels++;
            // Prevent optimization
            __asm__ volatile("" : : "r"(drawn_pixels) : "memory");
        }
    }
    return drawn_pixels;
}

// Optimized: Pre-calculated clipping bounds
long long draw_optimized(int x, int y, int width, int height) {
    long long drawn_pixels = 0;

    int start_y = 0;
    int end_y = height;

    // Vertical clipping logic
    if (y < 0) {
        start_y = -y;
    }
    if (y + height > OMNI_HEIGHT) {
        end_y = OMNI_HEIGHT - y;
    }

    if (start_y >= end_y) return 0; // Fully off-screen vertically

    int start_x = 0;
    int end_x = width;

    // Horizontal clipping logic
    if (x < 0) {
        start_x = -x;
    }
    if (x + width > OMNI_WIDTH) {
        end_x = OMNI_WIDTH - x;
    }

    if (start_x >= end_x) return 0; // Fully off-screen horizontally

    // Optimized loop structure
    for (int py = start_y; py < end_y; py++) {
        // No per-row check needed here

        // Simulating pointer arithmetic (src + py*width + start_x)
        // const uint32_t* src_row = ...

        for (int px = start_x; px < end_x; px++) {
            // No per-pixel check needed here

            drawn_pixels++;
             // Prevent optimization
            __asm__ volatile("" : : "r"(drawn_pixels) : "memory");
        }
    }
    return drawn_pixels;
}

void run_benchmark(const char* name, int x, int y, int iterations) {
    clock_t start, end;
    double cpu_time_used;
    long long pixels_base = 0;
    long long pixels_opt = 0;

    printf("Benchmarking: %s (x=%d, y=%d, %d iters)\n", name, x, y, iterations);

    start = clock();
    for (int i = 0; i < iterations; i++) {
        pixels_base += draw_baseline(x, y, IMG_WIDTH, IMG_HEIGHT);
    }
    end = clock();
    double time_base = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("  Baseline:  %.4f seconds (Pixels: %lld)\n", time_base, pixels_base);

    start = clock();
    for (int i = 0; i < iterations; i++) {
        pixels_opt += draw_optimized(x, y, IMG_WIDTH, IMG_HEIGHT);
    }
    end = clock();
    double time_opt = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("  Optimized: %.4f seconds (Pixels: %lld)\n", time_opt, pixels_opt);

    if (pixels_base != pixels_opt) {
        printf("  ERROR: Pixel count mismatch! (%lld vs %lld)\n", pixels_base, pixels_opt);
    } else {
        printf("  Speedup:   %.2fx\n", time_base / time_opt);
    }
    printf("\n");
}

int main() {
    // Case 1: Fully on-screen
    run_benchmark("Fully On-Screen", 100, 100, 1000);

    // Case 2: Partially off-screen (Top-Left) - heavy clipping
    run_benchmark("Clipped Top-Left", -500, -300, 1000);

    // Case 3: Partially off-screen (Bottom-Right)
    run_benchmark("Clipped Bottom-Right", OMNI_WIDTH - 500, OMNI_HEIGHT - 300, 1000);

    // Case 4: Fully off-screen
    run_benchmark("Fully Off-Screen", -2000, -2000, 1000000); // More iterations since it's fast

    return 0;
}
