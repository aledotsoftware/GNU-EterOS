#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

/* Capture standard library functions before they are renamed */
static size_t (*std_strnlen)(const char*, size_t) = strnlen;

#undef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#include "../include/string.h"

int main() {
    printf("Running optimized strnlen tests...\n");

    /* Test 1: Empty string */
    assert(strnlen("", 10) == 0);
    assert(strnlen("", 0) == 0);

    /* Test 2: Basic strings */
    assert(strnlen("Hello", 10) == 5);
    assert(strnlen("Hello", 5) == 5);
    assert(strnlen("Hello", 3) == 3);

    /* Test 3: Large strings (aligned) */
    char long_str[100];
    memset(long_str, 'A', 99);
    long_str[99] = '\0';
    assert(strnlen(long_str, 100) == 99);
    assert(strnlen(long_str, 50) == 50);

    /* Test 4: Unaligned strings */
    char buffer[256];
    memset(buffer, 'B', 255);
    buffer[255] = '\0';

    /* Test all offsets from 0 to 15 */
    for (int offset = 0; offset < 16; offset++) {
        char* ptr = buffer + offset;
        size_t len = 255 - offset;

        /* Null terminator reached */
        ptr[len] = '\0'; /* Ensure null terminator exists */
        assert(strnlen(ptr, len + 10) == len);

        /* Maxlen reached */
        assert(strnlen(ptr, 10) == 10);

        /* Maxlen exactly at null terminator */
        assert(strnlen(ptr, len) == len);

        /* Restore buffer */
        ptr[len] = 'B';
    }

    /* Test 5: Exact 8-byte boundaries */
    char aligned_buf[16];
    memset(aligned_buf, 'C', 15);
    aligned_buf[15] = '\0';

    assert(strnlen(aligned_buf, 16) == 15);
    assert(strnlen(aligned_buf, 8) == 8);
    assert(strnlen(aligned_buf, 7) == 7);

    /* Test 6: Maxlen larger than string length but smaller than buffer size */
    char buffer2[20] = "Short string";
    assert(strnlen(buffer2, 20) == 12);

    /* Test 7: Maxlen 0 */
    assert(strnlen("Test", 0) == 0);

    /* Test 8: Maxlen huge */
    assert(strnlen("Test", SIZE_MAX) == 4);

    /* Test 9: Null terminator exactly at the end of maxlen */
    char buffer3[10] = "123456789";
    assert(strnlen(buffer3, 9) == 9);

    /* Test 10: Null terminator beyond maxlen */
    assert(strnlen(buffer3, 5) == 5);

    /* Test 11: Benchmark (Basic) */
    const int ITERATIONS = 1000000;
    char bench_str[128];
    memset(bench_str, 'X', 127);
    bench_str[127] = '\0';

    clock_t start = clock();
    volatile size_t total_len = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        total_len += strnlen(bench_str, 128);
    }
    clock_t end = clock();

    /* CLOCKS_PER_SEC compatibility fallback */
    #ifndef CLOCKS_PER_SEC
    #define CLOCKS_PER_SEC 1000000
    #endif

    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Benchmark: %d iterations of 128-byte strnlen took %f seconds\n", ITERATIONS, time_taken);

    printf("All optimized strnlen tests passed!\n");
    return 0;
}
