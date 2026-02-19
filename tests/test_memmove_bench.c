#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>

/* Capture standard library functions before they are renamed */
static void* (*std_memset)(void*, int, size_t) = memset;
static int (*std_memcmp)(const void*, const void*, size_t) = memcmp;
static void* (*std_memmove)(void*, const void*, size_t) = memmove;

/* Define host test mode */
#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

/* Include kernel string functions (will be renamed to eteros_*) */
#include "../include/string.h"

/* Wrapper for kernel function (mapped in include/string.h to eteros_memmove) */
extern void* eteros_memmove(void* dest, const void* src, size_t n);
extern int eteros_memcmp(const void* s1, const void* s2, size_t n);

void benchmark_memcmp() {
    const size_t size = 10 * 1024 * 1024; /* 10 MB */
    const int iterations = 100;

    char* buf1 = malloc(size);
    char* buf2 = malloc(size);
    if (!buf1 || !buf2) return;

    /* Fill buffers identically */
    std_memset(buf1, 0xAA, size);
    std_memset(buf2, 0xAA, size);

    /* Measure standard memcmp */
    clock_t start = clock();
    volatile int res = 0;
    for (int i = 0; i < iterations; i++) {
        res += std_memcmp(buf1, buf2, size);
    }
    clock_t end = clock();
    double time_std = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Standard memcmp (10MB x %d):  %f seconds\n", iterations, time_std);

    /* Measure kernel memcmp */
    start = clock();
    res = 0;
    for (int i = 0; i < iterations; i++) {
        res += eteros_memcmp(buf1, buf2, size);
    }
    end = clock();
    double time_kernel = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Kernel memcmp (10MB x %d):    %f seconds\n", iterations, time_kernel);

    if (time_kernel > 0) {
        printf("Speedup factor: %.2fx (Baseline/Kernel)\n", time_std / time_kernel);
    }

    free(buf1);
    free(buf2);
}

/* Benchmark function */
void benchmark_memmove_backward() {
    const size_t size = 10 * 1024 * 1024; /* 10 MB */
    const int iterations = 100;
    const size_t offset = 1; /* dest > src by 1 byte, forcing backward copy */

    /* Allocate buffer */
    char* buf = malloc(size + offset);
    if (!buf) {
        printf("Benchmark failed: malloc error\n");
        return;
    }

    /* Initialize with pattern */
    std_memset(buf, 0xAA, size + offset);

    /* Measure standard libc (optimized assembly usually) */
    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        /* Reset buffer slightly to prevent pure cache hits being too easy? No, memmove is write-heavy. */
        /* Copy from buf to buf+offset (backward copy needed) */
        std_memmove(buf + offset, buf, size);
    }
    clock_t end = clock();
    double time_std = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Standard memmove (10MB x %d): %f seconds\n", iterations, time_std);

    /* Measure kernel implementation */
    start = clock();
    for (int i = 0; i < iterations; i++) {
        eteros_memmove(buf + offset, buf, size);
    }
    end = clock();
    double time_kernel = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Kernel memmove (10MB x %d):   %f seconds\n", iterations, time_kernel);

    if (time_kernel > 0) {
        printf("Speedup factor: %.2fx (Baseline/Kernel)\n", time_std / time_kernel);
    }

    free(buf);
}

int main() {
    printf("Benchmarking memmove backward copy (dest > src)...\n");
    benchmark_memmove_backward();
    printf("\nBenchmarking memcmp...\n");
    benchmark_memcmp();
    return 0;
}
