#include <stdio.h>
#include <string.h>
#include <stdint.h>

static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

#define BUFFER_SIZE (1024 * 1024 * 10) // 10 MB

char buffer[BUFFER_SIZE];

int main() {
    printf("Benchmarking strlen...\n");

    // Initialize buffer
    memset(buffer, 'A', BUFFER_SIZE - 1);
    buffer[BUFFER_SIZE - 1] = '\0';

    printf("Buffer size: %d bytes\n", BUFFER_SIZE);

    uint64_t start = rdtsc();
    volatile size_t len = 0;

    // Run multiple times to get a measurable duration
    int iterations = 10;
    for (int i = 0; i < iterations; i++) {
        len = strlen(buffer);
    }

    uint64_t end = rdtsc();
    uint64_t cycles = end - start;

    if (len != BUFFER_SIZE - 1) {
        printf("Error: Incorrect length %lu\n", len);
    }

    printf("Total cycles for %d iterations: %lu\n", iterations, cycles);
    printf("Average cycles per call: %lu\n", cycles / iterations);
    printf("Cycles per byte: %lu.%lu\n",
           (cycles / iterations) / BUFFER_SIZE,
           ((cycles / iterations) * 100 / BUFFER_SIZE) % 100);

    return 0;
}
