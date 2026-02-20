#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

/* Define host test mode */
#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

/* Include local string header which renames strchr to eteros_strchr */
#include "../include/string.h"

void benchmark_strchr() {
    const size_t size = 1024 * 1024 * 10; /* 10 MB string */
    const int iterations = 100;

    printf("Benchmarking eteros_strchr implementation...\n");

    /* Allocate buffer */
    char* str = malloc(size);

    if (!str) {
        printf("Benchmark failed: malloc error\n");
        return;
    }

    /* Fill string with 'A' */
    memset(str, 'A', size - 1);
    str[size - 1] = '\0';

    /* Place target character at the very end to force full scan */
    char target = 'B';
    str[size - 2] = target;

    printf("1. Long string scan (length %zu bytes)...\n", size - 1);

    clock_t start = clock();
    volatile char* res = NULL;
    for (int i = 0; i < iterations; i++) {
        res = strchr(str, target); /* This calls eteros_strchr */
    }
    clock_t end = clock();

    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("   -> Time: %f seconds (%d iterations)\n", time_taken, iterations);

    if (res != &str[size-2]) {
        printf("ERROR: strchr found wrong location or NULL!\n");
    }

    free(str);
}

int main() {
    benchmark_strchr();
    return 0;
}
