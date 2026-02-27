#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> // Host string.h
#include <assert.h> // Host assert.h

// Rename userspace functions to avoid conflicts
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

// NOTE: eteros_strcpy is NOT defined in userspace/libc/src/string.c
// because the file removed it for security reasons (as per comment in file).
// So we should use strlcpy or manually implement strcpy using strlcpy for test purposes if needed,
// OR just use host strcpy for test setup.
// Since we #defined strcpy to eteros_strcpy, calling strcpy() in test calls eteros_strcpy which doesn't exist.
// We should NOT redefine strcpy if we want to use host strcpy for setup.

#undef strcpy
// But we want to ensure we don't accidentally link against host implementation if we were testing it.
// Here we are testing `eteros_memmove`. We use `strcpy` to setup buffers.
// So we want HOST strcpy.

#define strdup eteros_strdup
#define strndup eteros_strndup
#define strspn eteros_strspn
#define strcspn eteros_strcspn
#define strpbrk eteros_strpbrk
#define strerror eteros_strerror
#define strcoll eteros_strcoll
#define strxfrm eteros_strxfrm

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

    for (int i=0; i<256; i++) buf[i] = 'A';

    for (int offset = 0; offset < 16; offset++) {
        for (int len = 0; len < 128; len++) {
            buf[offset + len] = '\0';
            assert(strlen(buf + offset) == (size_t)len);
            buf[offset + len] = 'A'; // Restore
        }
    }
    free(buf);

    printf("strlen correctness tests passed!\n");
}

void test_memmove() {
    char buf[100];

    // Test 1: Simple copy
    // Use host strcpy (unmasked)
    strcpy(buf, "Hello World");
    memmove(buf + 20, buf, 12);
    assert(strcmp(buf + 20, "Hello World") == 0);

    // Test 2: Overlap (dest > src) - Backward copy needed
    strcpy(buf, "0123456789");
    memmove(buf + 1, buf, 5);
    // Result: "0012346789"
    assert(strncmp(buf, "0012346789", 10) == 0);

    // Test 3: Overlap (dest < src) - Forward copy needed
    strcpy(buf, "0123456789");
    // Move "12345" to index 0.
    memmove(buf, buf + 1, 5);
    // Expected: "1234556789"
    assert(strncmp(buf, "1234556789", 10) == 0);

    printf("memmove correctness tests passed!\n");
}

int main() {
    test_strlen();
    test_memmove();
    return 0;
}
