#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

/* Rename userspace functions to avoid conflict with host libc */
#define strlen eteros_strlen
#define memcpy eteros_memcpy
#define memset eteros_memset
#define memmove eteros_memmove
#define memcmp eteros_memcmp
#define strcmp eteros_strcmp
#define strncmp eteros_strncmp
#define strcpy eteros_strcpy
#define strncpy eteros_strncpy
#define strcat eteros_strcat
#define strchr eteros_strchr
#define strrchr eteros_strrchr
#define strstr eteros_strstr
#define strtok eteros_strtok

/* Include the source file directly */
#include "../userspace/libc/src/string.c"

int main() {
    printf("Testing userspace strlen implementation...\n");

    /* Test 1: Empty string */
    assert(eteros_strlen("") == 0);

    /* Test 2: Short string */
    assert(eteros_strlen("Hello") == 5);

    /* Test 3: String with spaces */
    assert(eteros_strlen("Hello World") == 11);

    /* Test 4: Long string (trigger optimization) */
    char long_str[100];
    for (int i = 0; i < 99; i++) long_str[i] = 'A';
    long_str[99] = '\0';
    assert(eteros_strlen(long_str) == 99);

    /* Test 5: Alignment check using malloc (usually 16-byte aligned) */
    /* We want to test aligned and unaligned starts */
    char *buf = malloc(256);
    if (!buf) {
        printf("Malloc failed\n");
        return 1;
    }

    /* Fill with non-null */
    memset(buf, 'X', 256);

    /* Test various offsets (0-15) to check alignment handling */
    for (int offset = 0; offset < 16; offset++) {
        char *ptr = buf + offset;

        /* Test various lengths (0-64) */
        for (int len = 0; len < 64; len++) {
            ptr[len] = '\0'; /* Terminate */

            size_t res = eteros_strlen(ptr);
            if (res != (size_t)len) {
                printf("FAIL: offset=%d, len=%d, expected=%d, got=%lu\n", offset, len, len, res);
                assert(res == (size_t)len);
            }

            ptr[len] = 'X'; /* Restore */
        }
    }

    free(buf);

    printf("ALL TESTS PASSED.\n");
    return 0;
}
