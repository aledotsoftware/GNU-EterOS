#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

/* Capture standard library strcmp before it is renamed by our header */
static int (*std_strcmp)(const char*, const char*) = strcmp;

/* Define host test mode */
#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

/* Include local string header which renames strcmp to eteros_strcmp */
#include "../include/string.h"

/*
 * Macro to ensure we are calling the function we intend to benchmark.
 * If we just call strcmp, it might be the macro or the function depending on include order.
 * But here we included string.h last, so strcmp is a macro to eteros_strcmp.
 */

void benchmark_strcmp() {
    const size_t size = 1024 * 1024; /* 1 MB string */
    const int iterations = 1000;

    printf("Benchmarking eteros_strcmp implementation...\n");

    /* Allocate buffers */
    char* str1 = malloc(size);
    char* str2 = malloc(size);

    if (!str1 || !str2) {
        printf("Benchmark failed: malloc error\n");
        return;
    }

    /* Fill strings with non-null data */
    memset(str1, 'A', size - 1);
    memset(str2, 'A', size - 1);
    str1[size - 1] = '\0';
    str2[size - 1] = '\0';

    printf("1. Aligned strings (length %zu bytes)...\n", size - 1);

    clock_t start = clock();
    volatile int res = 0;
    for (int i = 0; i < iterations; i++) {
        res += strcmp(str1, str2); /* This calls eteros_strcmp due to macro */
    }
    clock_t end = clock();

    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("   -> Time: %f seconds (%d iterations)\n", time_taken, iterations);

    /* Test unaligned */
    printf("2. Unaligned strings (start offset +1)...\n");
    char* ustr1 = str1 + 1;
    char* ustr2 = str2 + 1;
    ustr1[size - 2] = '\0';
    ustr2[size - 2] = '\0';

    start = clock();
    res = 0;
    for (int i = 0; i < iterations; i++) {
        res += strcmp(ustr1, ustr2);
    }
    end = clock();

    time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("   -> Time: %f seconds (%d iterations)\n", time_taken, iterations);

    free(str1);
    free(str2);
}

int main() {
    benchmark_strcmp();
    return 0;
}
