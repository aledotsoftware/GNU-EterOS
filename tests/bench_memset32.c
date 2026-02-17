#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include "../include/string.h"

/*
 * Benchmark for memset32.
 */

void benchmark_memset32() {
    /*
     * Framebuffer size simulation: 1920x1080 * 4 bytes/pixel = ~8MB
     * But memset32 takes count of 32-bit elements, so 1920x1080 = 2,073,600 elements.
     */
    const size_t elements = 1920 * 1080;
    const size_t buffer_size = elements * sizeof(uint32_t);
    const int iterations = 1000;

    printf("Benchmarking memset32 implementation...\n");
    printf("Buffer size: %zu bytes (%zu elements)\n", buffer_size, elements);

    /* Allocate buffer */
    /* Use aligned_alloc if available, or just malloc (which is usually aligned enough) */
    uint32_t* buffer = (uint32_t*)malloc(buffer_size);

    if (!buffer) {
        printf("Benchmark failed: malloc error\n");
        return;
    }

    printf("1. Aligned buffer...\n");

    clock_t start = clock();

    /* We use volatile to prevent loop optimization, but memset32 itself is what we test */
    for (int i = 0; i < iterations; i++) {
        memset32(buffer, 0xFF00FF00, elements);
        /*
         * Barrier to prevent compiler from optimizing away the call if it thinks result is unused
         * specific_bzero does this, but we can just do a volatile read.
         */
        __asm__ volatile("" : : "r"(buffer) : "memory");
    }
    clock_t end = clock();

    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("   -> Time: %f seconds (%d iterations)\n", time_taken, iterations);
    printf("   -> Bandwidth: %f GB/s\n", (double)(buffer_size * iterations) / time_taken / (1024*1024*1024));

    free(buffer);
}

int main() {
    benchmark_memset32();
    return 0;
}
