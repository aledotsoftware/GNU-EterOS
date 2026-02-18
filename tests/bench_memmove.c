#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

/* Capture standard libc memmove */
static void* (*libc_memmove)(void*, const void*, size_t) = memmove;

#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

/* Rename atoi to avoid conflict with stdlib.h (declared in include/stdlib.h via kernel/string.c) */
#define atoi eteros_atoi
#define atoi_s eteros_atoi_s
#define itoa_s eteros_itoa_s
#define utoa_hex_s eteros_utoa_hex_s

/* Include local string header and implementation */
#include "../include/string.h"

/* We include the kernel implementation directly to benchmark it */
#include "../kernel/string.c"

void benchmark_memmove() {
    const size_t size = 10 * 1024 * 1024; /* 10 MB */
    const int iterations = 100;

    printf("Benchmarking memmove (backward copy, dest > src)...\n");

    uint8_t* buffer = malloc(size + 1024);
    if (!buffer) {
        printf("Failed to allocate buffer\n");
        return;
    }

    /* Initialize with pattern */
    memset(buffer, 0xAA, size + 1024);

    /* Case: dest = src + 1 (massive overlap requiring backward copy) */
    /* src = buffer, dest = buffer + 1 */
    /* This forces the 'else' branch in memmove */

    /* 1. Measure eteros_memmove */
    clock_t start = clock();
    volatile int dummy = 0;
    for (int i = 0; i < iterations; i++) {
        eteros_memmove(buffer + 1, buffer, size);
        dummy += buffer[0]; /* prevent optimization */
    }
    clock_t end = clock();
    double time_eteros = ((double)(end - start)) / CLOCKS_PER_SEC;
    double mb_s_eteros = (double)size * iterations / (1024*1024) / time_eteros;
    printf("eteros_memmove: %.4f seconds (%.2f MB/s)\n", time_eteros, mb_s_eteros);

    /* Reset buffer just in case (though not strictly needed for performance test) */
    memset(buffer, 0xAA, size + 1024);

    /* 2. Measure libc_memmove */
    start = clock();
    for (int i = 0; i < iterations; i++) {
        libc_memmove(buffer + 1, buffer, size);
        dummy += buffer[0];
    }
    end = clock();
    double time_libc = ((double)(end - start)) / CLOCKS_PER_SEC;
    double mb_s_libc = (double)size * iterations / (1024*1024) / time_libc;
    printf("libc_memmove:   %.4f seconds (%.2f MB/s)\n", time_libc, mb_s_libc);

    free(buffer);
}

int main() {
    benchmark_memmove();
    return 0;
}
