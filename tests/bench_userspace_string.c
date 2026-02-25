#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h> // Host string.h

// Rename userspace functions to avoid conflict with host libc
#define memcpy eteros_memcpy
#define memset eteros_memset
#define memmove eteros_memmove
#define memcmp eteros_memcmp
#define strlen eteros_strlen
#define strcmp eteros_strcmp
#define strncmp eteros_strncmp
#define strncpy eteros_strncpy
#define strlcpy eteros_strlcpy
#define strlcat eteros_strlcat
#define strchr eteros_strchr
#define strrchr eteros_strrchr
#define strstr eteros_strstr
#define strtok eteros_strtok

// Include the userspace source file directly
// We rely on host's headers for types like size_t, uint8_t (via stdint.h)
#include "../userspace/libc/src/string.c"

void benchmark_strlen() {
    const size_t size = 10 * 1024 * 1024; // 10 MB
    const size_t iterations = 100;

    char* buffer = malloc(size + 1);
    if (!buffer) {
        printf("Failed to allocate buffer\n");
        return;
    }

    // Initialize buffer with 'A's
    for (size_t i = 0; i < size; i++) {
        buffer[i] = 'A';
    }
    buffer[size] = '\0';

    printf("Benchmarking userspace strlen...\n");
    printf("String size: %zu bytes\n", size);

    clock_t start = clock();

    volatile size_t total_len = 0;
    for (size_t i = 0; i < iterations; i++) {
        total_len += eteros_strlen(buffer);
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Time taken: %f seconds\n", time_taken);
    printf("Result: %zu (Expected: %zu)\n", total_len / iterations, size);
    printf("Bandwidth: %f GB/s\n", (double)(size * iterations) / time_taken / (1024.0*1024.0*1024.0));

    free(buffer);
}

void benchmark_strchr() {
    const size_t size = 10 * 1024 * 1024; // 10 MB
    const size_t iterations = 100;

    char* buffer = malloc(size + 1);
    if (!buffer) {
        printf("Failed to allocate buffer\n");
        return;
    }

    // Initialize buffer with 'A's
    for (size_t i = 0; i < size; i++) {
        buffer[i] = 'A';
    }
    buffer[size] = '\0'; // Null terminate

    // Test case 1: Character at the end
    buffer[size - 1] = 'B';
    printf("Benchmarking userspace strchr (char at end of 10MB string)...\n");

    clock_t start = clock();

    volatile char* res;
    for (size_t i = 0; i < iterations; i++) {
        res = eteros_strchr(buffer, 'B');
        // Prevent compiler optimization
        __asm__ volatile("" : : "r"(res) : "memory");
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Time taken (Found at end): %f seconds\n", time_taken);
    printf("Bandwidth: %f GB/s\n", (double)(size * iterations) / time_taken / (1024.0*1024.0*1024.0));

    // Test case 2: Character not present
    buffer[size - 1] = 'A'; // Restore
    printf("Benchmarking userspace strchr (char not present)...\n");

    start = clock();

    for (size_t i = 0; i < iterations; i++) {
        res = eteros_strchr(buffer, 'Z');
        __asm__ volatile("" : : "r"(res) : "memory");
    }

    end = clock();
    time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Time taken (Not found): %f seconds\n", time_taken);
    printf("Bandwidth: %f GB/s\n", (double)(size * iterations) / time_taken / (1024.0*1024.0*1024.0));

    free(buffer);
}

int main() {
    benchmark_strlen();
    printf("\n");
    benchmark_strchr();
    return 0;
}
