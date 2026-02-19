#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include "../include/string.h"

void benchmark_memmove() {
    const size_t size = 10 * 1024 * 1024; // 10 MB
    const size_t iterations = 1000;

    char* buffer = malloc(size);
    if (!buffer) {
        printf("Failed to allocate buffer\n");
        return;
    }

    // Initialize buffer
    memset(buffer, 0xAA, size);

    // Test case: Backward copy (dest > src)
    // Overlapping by 1MB
    size_t overlap = 1024 * 1024;
    size_t copy_size = size - overlap;

    char* src = buffer;
    char* dest = buffer + overlap;

    printf("Benchmarking memmove (backward copy, dest > src)...\n");
    printf("Copy size: %zu bytes, Overlap: %zu bytes\n", copy_size, overlap);

    clock_t start = clock();

    for (size_t i = 0; i < iterations; i++) {
        memmove(dest, src, copy_size);
        // Prevent compiler optimization
         __asm__ volatile("" : : "r"(buffer) : "memory");
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Time taken: %f seconds\n", time_taken);
    printf("Bandwidth: %f GB/s\n", (double)(copy_size * iterations) / time_taken / (1024*1024*1024));

    free(buffer);
}

int main() {
    benchmark_memmove();
    return 0;
}
