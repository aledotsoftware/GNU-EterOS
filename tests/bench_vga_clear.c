#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_SIZE (VGA_WIDTH * VGA_HEIGHT)

uint16_t buffer[VGA_SIZE];
uint16_t fill_val = 0x0720; // ' ' with attribute 0x07

// Original implementation
void clear_loop(void) {
    for (size_t i = 0; i < VGA_SIZE; i++) {
        buffer[i] = fill_val;
    }
    // Prevent compiler from optimizing away the loop
    __asm__ volatile("" : : "m"(buffer) : "memory");
}

// Optimized implementation using rep stosw
void clear_memset16(void) {
    size_t count = VGA_SIZE;
    uint16_t val = fill_val;
    void* dest = buffer;

    __asm__ volatile (
        "cld\n\t"
        "rep stosw"
        : "+D"(dest), "+c"(count)
        : "a"(val)
        : "memory", "cc"
    );
}

// Standard memset (filling with 0) for reference
void clear_memset_std(void) {
    memset(buffer, 0, VGA_SIZE * sizeof(uint16_t));
    __asm__ volatile("" : : "m"(buffer) : "memory");
}

int main() {
    uint64_t start, end;
    int iterations = 100000;

    printf("Benchmarking VGA Clear (Size: %d words)\n", VGA_SIZE);

    // Warmup
    clear_loop();
    clear_memset16();

    // Measure Loop
    start = __rdtsc();
    for (int i = 0; i < iterations; i++) {
        clear_loop();
    }
    end = __rdtsc();
    printf("Manual Loop cycles (avg): %lu\n", (end - start) / iterations);

    // Measure Memset16
    start = __rdtsc();
    for (int i = 0; i < iterations; i++) {
        clear_memset16();
    }
    end = __rdtsc();
    printf("Memset16 (rep stosw) cycles (avg): %lu\n", (end - start) / iterations);

    // Measure Standard Memset (fill 0)
    start = __rdtsc();
    for (int i = 0; i < iterations; i++) {
        clear_memset_std();
    }
    end = __rdtsc();
    printf("Standard Memset (fill 0) cycles (avg): %lu\n", (end - start) / iterations);

    return 0;
}
