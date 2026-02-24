#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#define memcpy eteros_memcpy
#define memset eteros_memset
#define memmove eteros_memmove
#define memcmp eteros_memcmp
#define strlen eteros_strlen
#define strnlen eteros_strnlen
#define strcmp eteros_strcmp
#define strncmp eteros_strncmp
#define strncpy eteros_strncpy
#define strlcpy eteros_strlcpy
#define strlcat eteros_strlcat
#define strchr eteros_strchr
#define strrchr eteros_strrchr
#define strstr eteros_strstr

// Include the implementation directly
#include "../userspace/libc/src/string.c"

void benchmark_strcmp() {
    const size_t size = 10 * 1024 * 1024; // 10 MB
    const size_t iterations = 50;

    char* s1 = malloc(size + 1);
    char* s2 = malloc(size + 1);
    if (!s1 || !s2) {
        printf("Failed to allocate buffer\n");
        return;
    }

    // Initialize buffers to be identical
    // We can use standard memset here since we renamed the custom one
    // But we need to use the system one. Since we defined memset macro, we can't call system memset directly by name 'memset'.
    // Actually, 'memset' token is replaced by 'eteros_memset'.
    // So we should use a loop or call eteros_memset.
    for (size_t i = 0; i < size; i++) {
        s1[i] = 'A';
        s2[i] = 'A';
    }
    s1[size] = '\0';
    s2[size] = '\0';

    // Correctness check
    printf("Verifying correctness...\n");
    if (eteros_strcmp(s1, s2) != 0) {
        printf("FAIL: Identical strings not equal\n");
        return;
    }
    s2[size-1] = 'B';
    if (eteros_strcmp(s1, s2) == 0) {
        printf("FAIL: Different strings equal\n");
        return;
    }
    s2[size-1] = 'A'; // Restore

    // Check short string
    char short1[] = "hello";
    char short2[] = "hello";
    if (eteros_strcmp(short1, short2) != 0) {
        printf("FAIL: Short identical strings\n");
        return;
    }
    short2[4] = 'p';
    if (eteros_strcmp(short1, short2) == 0) {
        printf("FAIL: Short different strings\n");
        return;
    }

    printf("Benchmarking userspace strcmp...\n");
    printf("String size: %zu bytes\n", size);
    printf("Iterations: %zu\n", iterations);

    // 1. Identical strings (Worst case for byte-wise)
    printf("\nTest 1: Identical strings (Full scan)...\n");
    clock_t start = clock();
    volatile int res = 0;
    for (size_t i = 0; i < iterations; i++) {
        res += eteros_strcmp(s1, s2);
    }
    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", time_taken);
    printf("Throughput: %f MB/s\n", (double)(size * iterations) / (1024*1024) / time_taken);

    // 2. Difference at end
    s2[size-1] = 'B';
    printf("\nTest 2: Difference at end (Full scan)...\n");
    start = clock();
    for (size_t i = 0; i < iterations; i++) {
        res += eteros_strcmp(s1, s2);
    }
    end = clock();
    time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", time_taken);

    // 3. Difference at beginning
    s2[0] = 'B';
    printf("\nTest 3: Difference at beginning (Fast fail)...\n");
    start = clock();
    for (size_t i = 0; i < iterations * 10000; i++) { // More iterations for fast fail
        res += eteros_strcmp(s1, s2);
    }
    end = clock();
    time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds (for %zu iterations)\n", time_taken, iterations * 10000);

    free(s1);
    free(s2);
}

int main() {
    benchmark_strcmp();
    return 0;
}
