#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

// Macros to rename functions to avoid conflict with host libc
#define strlen my_strlen
#define memcpy my_memcpy
#define memset my_memset
#define memmove my_memmove
#define memcmp my_memcmp
#define strcmp my_strcmp
#define strncmp my_strncmp
#define strcpy my_strcpy
#define strncpy my_strncpy
#define strcat my_strcat
#define strchr my_strchr
#define strrchr my_strrchr
#define strstr my_strstr

// Include the source file directly
#include "../userspace/libc/src/string.c"

#undef strlen

void test_strlen_correctness() {
    printf("Testing userspace strlen correctness...\n");

    // Test 1: Empty string
    assert(my_strlen("") == 0);

    // Test 2: Single char
    assert(my_strlen("a") == 1);

    // Test 3: Standard string
    assert(my_strlen("hello") == 5);

    // Test 4: Long string
    char *long_str = malloc(1000);
    memset(long_str, 'a', 999);
    long_str[999] = '\0';
    assert(my_strlen(long_str) == 999);
    free(long_str);

    // Test 5: Alignment test
    char *buf = malloc(64);
    memset(buf, 'x', 63);
    buf[63] = '\0';

    // Test all start offsets
    for (int i = 0; i < 16; i++) {
        // Place a null terminator at various distances
        for (int len = 0; len < 30; len++) {
            char saved = buf[i + len];
            buf[i + len] = '\0';
            assert(my_strlen(buf + i) == (size_t)len);
            buf[i + len] = saved;
        }
    }
    free(buf);

    printf("Userspace strlen correctness passed.\n");
}

int main() {
    test_strlen_correctness();
    return 0;
}
