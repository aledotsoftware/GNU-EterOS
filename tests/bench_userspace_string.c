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
#define strcpy eteros_strcpy
#define strncpy eteros_strncpy
#define strcat eteros_strcat
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

    // Initialize buffer with 'A's and null terminator
    // Use host memset for setup to be safe/fast
    // But we renamed memset... so call eteros_memset or direct use
    // Since we included string.c, eteros_memset is available.
    // But for setup reliability, let's use a loop or the function we just compiled.
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

    free(buffer);
}

int main() {
    benchmark_strlen();
    return 0;
}
