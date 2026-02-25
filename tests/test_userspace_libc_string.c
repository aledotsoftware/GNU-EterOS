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
#define strncpy my_strncpy
#define strlcpy my_strlcpy
#define strlcat my_strlcat
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

void test_strlcpy_correctness() {
    printf("Testing userspace strlcpy correctness...\n");
    char buffer[16];
    size_t len;

    // Test 1: Normal copy
    len = my_strlcpy(buffer, "Hello", sizeof(buffer));
    assert(len == 5);
    assert(my_strcmp(buffer, "Hello") == 0);

    // Test 2: Truncation
    len = my_strlcpy(buffer, "Hello World", 5);
    assert(len == 11);
    assert(my_strcmp(buffer, "Hell") == 0);
    assert(buffer[4] == '\0');

    // Test 3: Zero size
    len = my_strlcpy(buffer, "Hello", 0);
    assert(len == 5);

    printf("Userspace strlcpy correctness passed.\n");
}

void test_strlcat_correctness() {
    printf("Testing userspace strlcat correctness...\n");
    char buffer[16];
    size_t len;

    my_strlcpy(buffer, "Hello", sizeof(buffer));

    // Test 1: Normal cat
    len = my_strlcat(buffer, " World", sizeof(buffer));
    assert(len == 11);
    assert(my_strcmp(buffer, "Hello World") == 0);

    // Test 2: Truncation
    my_strlcpy(buffer, "Hello", sizeof(buffer));
    len = my_strlcat(buffer, " World", 8);
    assert(len == 11);
    assert(my_strcmp(buffer, "Hello W") == 0);

    printf("Userspace strlcat correctness passed.\n");
}

void test_strchr_correctness() {
    printf("Testing userspace strchr correctness...\n");

    const char *str = "Hello World";

    // Found
    assert(my_strchr(str, 'H') == str);
    assert(my_strchr(str, 'e') == str + 1);
    assert(my_strchr(str, 'o') == str + 4);
    assert(my_strchr(str, 'd') == str + 10);

    // Not found
    assert(my_strchr(str, 'z') == NULL);
    assert(my_strchr(str, 'X') == NULL);

    // Terminator
    assert(my_strchr(str, '\0') == str + 11);

    // Empty string
    assert(my_strchr("", 'a') == NULL);
    assert(my_strchr("", '\0') != NULL);
    assert(*my_strchr("", '\0') == '\0');

    // Alignment and SWAR test
    char *buf = malloc(64);
    memset(buf, 'x', 63);
    buf[63] = '\0';

    // Test finding character at various offsets and alignments
    for (int i = 0; i < 16; i++) {
        for (int pos = 0; pos < 30; pos++) {
            buf[i + pos] = 'Y';
            char *res = my_strchr(buf + i, 'Y');
            assert(res == buf + i + pos);
            buf[i + pos] = 'x'; // Restore
        }
    }

    // Test finding terminator at various offsets
     for (int i = 0; i < 16; i++) {
        for (int len = 0; len < 30; len++) {
            char saved = buf[i + len];
            buf[i + len] = '\0';
            assert(my_strchr(buf + i, '\0') == buf + i + len);
            buf[i + len] = saved;
        }
    }

    free(buf);
    printf("Userspace strchr correctness passed.\n");
}

int main() {
    test_strlen_correctness();
    test_strlcpy_correctness();
    test_strlcat_correctness();
    test_strchr_correctness();
    return 0;
}
