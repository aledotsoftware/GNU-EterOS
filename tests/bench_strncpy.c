#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

char* strncpy_old(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char* strncpy_new(char* dest, const char* src, size_t n) {
    size_t len = strnlen(src, n);
    memcpy(dest, src, len);
    if (len < n) {
        memset(dest + len, 0, n - len);
    }
    return dest;
}


int main() {
    char dest[256];
    char src[256];
    memset(src, 'A', 255);
    src[255] = '\0';

    clock_t start = clock();
    for (int i = 0; i < 1000000; i++) {
        strncpy_old(dest, src, 256);
    }
    clock_t end = clock();
    double old_time = (double)(end - start) / CLOCKS_PER_SEC;

    start = clock();
    for (int i = 0; i < 1000000; i++) {
        strncpy_new(dest, src, 256);
    }
    end = clock();
    double new_time = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Old Time: %f seconds\n", old_time);
    printf("New Time: %f seconds\n", new_time);
    printf("Speedup: %fx\n", old_time / new_time);

    return 0;
}
