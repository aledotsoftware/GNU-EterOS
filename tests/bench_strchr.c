#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

/* Include the header under test which defines macros to rename strchr -> eteros_strchr */
#include "../include/string.h"

void benchmark_strchr() {
    const size_t size = 10 * 1024 * 1024; // 10 MB
    const size_t iterations = 100;

    char* buffer = malloc(size + 1);
    if (!buffer) {
        printf("Failed to allocate buffer\n");
        return;
    }

    // Initialize buffer with 'A's
    memset(buffer, 'A', size);
    buffer[size] = '\0'; // Null terminate

    // Test case 1: Character at the end
    buffer[size - 1] = 'B';
    printf("Benchmarking strchr (char at end of 10MB string)...\n");

    clock_t start = clock();

    volatile char* res;
    for (size_t i = 0; i < iterations; i++) {
        res = strchr(buffer, 'B');
        // Prevent compiler optimization
        __asm__ volatile("" : : "r"(res) : "memory");
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Time taken (Found at end): %f seconds\n", time_taken);
    printf("Bandwidth: %f GB/s\n", (double)(size * iterations) / time_taken / (1024.0*1024.0*1024.0));

    // Test case 2: Character not present
    buffer[size - 1] = 'A'; // Restore
    printf("Benchmarking strchr (char not present in 10MB string)...\n");

    start = clock();

    for (size_t i = 0; i < iterations; i++) {
        res = strchr(buffer, 'Z');
        __asm__ volatile("" : : "r"(res) : "memory");
    }

    end = clock();
    time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Time taken (Not found): %f seconds\n", time_taken);
    printf("Bandwidth: %f GB/s\n", (double)(size * iterations) / time_taken / (1024.0*1024.0*1024.0));

    free(buffer);
}

int main() {
    benchmark_strchr();
    return 0;
}
