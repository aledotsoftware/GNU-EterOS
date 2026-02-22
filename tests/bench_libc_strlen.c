#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

/* Rename functions to prevent conflicts with host libc */
#define memcpy my_memcpy
#define memset my_memset
#define memmove my_memmove
#define memcmp my_memcmp
#define strlen my_strlen
#define strcmp my_strcmp
#define strncmp my_strncmp
#define strcpy my_strcpy
#define strncpy my_strncpy
#define strcat my_strcat
#define strchr my_strchr
#define strrchr my_strrchr
#define strstr my_strstr

/* Include the source file directly */
#include "../userspace/libc/src/string.c"

void verify_strlen() {
    assert(my_strlen("") == 0);
    assert(my_strlen("a") == 1);
    assert(my_strlen("ab") == 2);
    assert(my_strlen("abc") == 3);
    assert(my_strlen("abcd") == 4);
    assert(my_strlen("abcde") == 5);
    assert(my_strlen("abcdef") == 6);
    assert(my_strlen("abcdefg") == 7);
    assert(my_strlen("abcdefgh") == 8);
    assert(my_strlen("abcdefghi") == 9);
    assert(my_strlen("0123456789") == 10);

    /* Test alignment */
    char *buf = (char*)malloc(100);
    memset(buf, 'a', 100);

    /* Test various lengths and offsets */
    for (int offset = 0; offset < 16; offset++) {
        for (int len = 0; len < 64; len++) {
            char *ptr = buf + offset;
            ptr[len] = '\0';
            assert(my_strlen(ptr) == len);
            ptr[len] = 'a'; /* restore */
        }
    }

    free(buf);
    printf("Verification passed.\n");
}

void benchmark_strlen() {
    const int iterations = 1000000;
    const int len = 1024; // 1KB string
    char *str = (char*)malloc(len + 1);

    // Fill with 'a'
    for (int i = 0; i < len; i++) {
        str[i] = 'a';
    }
    str[len] = '\0';

    // Warmup
    volatile size_t res = 0;
    res = my_strlen(str);

    // Benchmark My Implementation
    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        res += my_strlen(str);
    }
    clock_t end = clock();
    double time_my = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Custom strlen: %f seconds (res=%zu)\n", time_my, res);

    free(str);
}

int main() {
    printf("Benchmarking userspace/libc/src/string.c functions...\n");
    verify_strlen();
    benchmark_strlen();
    return 0;
}
