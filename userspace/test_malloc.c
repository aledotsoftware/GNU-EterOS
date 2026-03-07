#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("=== test_malloc ===\n");
    void* ptr1 = malloc(128);
    if (ptr1) {
        printf("malloc(128) returned: %p\n", ptr1);
        memset(ptr1, 0xAA, 128);
    } else {
        printf("malloc(128) failed\n");
        return 1;
    }

    void* ptr2 = malloc(256);
    if (ptr2) {
        printf("malloc(256) returned: %p\n", ptr2);
    } else {
        printf("malloc(256) failed\n");
        return 1;
    }

    free(ptr1);
    printf("Freed ptr1\n");

    free(ptr2);
    printf("Freed ptr2\n");

    return 0;
}
