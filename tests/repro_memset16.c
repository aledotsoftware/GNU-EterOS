#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

/* Include kernel string functions directly */
/* We need to rename standard functions that might conflict if not static/inline in header */
/* But here we just include the source. */
#include "../include/string.h"

/* We need to make sure we are calling the KERNEL version of memset16, not a potential libc one (though libc usually doesn't have memset16) */
/* kernel/string.c implements memset16. */

extern void* memset16(void* dest, uint16_t c, size_t n);

void benchmark_memset16() {
    const size_t size_bytes = 128 * 1024 * 1024; /* 128 MB */
    const size_t num_words = size_bytes / 2;
    const size_t iterations = 50;

    printf("Allocating %zu MB buffer...\n", size_bytes / (1024*1024));
    uint16_t* buffer = (uint16_t*)malloc(size_bytes);
    if (!buffer) {
        printf("Failed to allocate buffer\n");
        return;
    }

    /* Touch memory to fault it in */
    memset(buffer, 0, size_bytes);

    printf("Benchmarking memset16 (%zu iterations)...\n", iterations);

    clock_t start = clock();

    for (size_t i = 0; i < iterations; i++) {
        memset16(buffer, 0xA5A5, num_words);
        /* Prevent optimization */
        __asm__ volatile("" : : "r"(buffer) : "memory");
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    double total_bytes = (double)size_bytes * iterations;
    double gb_per_sec = (total_bytes / time_taken) / (1024*1024*1024);

    printf("Time taken: %f seconds\n", time_taken);
    printf("Throughput: %f GB/s\n", gb_per_sec);

    free(buffer);
}

int main() {
    benchmark_memset16();
    return 0;
}
