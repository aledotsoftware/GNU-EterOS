#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> // Host string.h
#include <assert.h>

// Macros
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

#include "../userspace/libc/src/string.c"

void test_strlen() {
    assert(strlen("") == 0);
    assert(strlen("a") == 1);
    assert(strlen("ab") == 2);
    assert(strlen("abc") == 3);
    assert(strlen("1234567") == 7);
    assert(strlen("12345678") == 8);
    assert(strlen("123456789") == 9);

    // Alignment tests
    char *buf = malloc(256);
    if (!buf) return;

    // Fill with 'A'
    // Use host memset for reliability
    for (int i=0; i<256; i++) buf[i] = 'A';

    // Test various start offsets (0 to 15 to cover alignment)
    for (int offset = 0; offset < 16; offset++) {
        // Test various lengths
        for (int len = 0; len < 128; len++) {
            buf[offset + len] = '\0';
            assert(strlen(buf + offset) == (size_t)len);
            buf[offset + len] = 'A'; // Restore
        }
    }
    free(buf);

    printf("strlen correctness tests passed!\n");
}

int main() {
    test_strlen();
    return 0;
}
