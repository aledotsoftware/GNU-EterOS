#define __ETEROS_HOST_TEST__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "../include/types.h"

#define memcpy eteros_memcpy
#define memset eteros_memset
#define memmove eteros_memmove
#define memcmp eteros_memcmp
#define strlen eteros_strlen
#define strncpy eteros_strncpy
#define strlcpy eteros_strlcpy
#define strcmp eteros_strcmp
#define itoa_s eteros_itoa_s
#define utoa_hex_s eteros_utoa_hex_s

#include "../kernel/string.c"

static char hex_chars_lower[] = "0123456789abcdef";
static char hex_chars_upper[] = "0123456789ABCDEF";

void format_int_old(char** buf, int64_t val, int base, int is_signed, int width, char pad, int upper) {
    char temp[64];
    int i = 0;
    uint64_t uval;
    int is_neg = 0;

    if (is_signed && val < 0) {
        is_neg = 1;
        uval = (uint64_t)(-val);
    } else {
        uval = (uint64_t)val;
    }

    if (uval == 0) {
        temp[i++] = '0';
    } else {
        while (uval > 0) {
            int rem = uval % base;
            if (rem < 10) {
                temp[i++] = '0' + rem;
            } else {
                temp[i++] = (upper ? 'A' : 'a') + (rem - 10);
            }
            uval /= base;
        }
    }

    if (is_neg) {
        temp[i++] = '-';
    }

    while (i < width) {
        temp[i++] = pad;
    }

    while (i > 0) {
        *(*buf)++ = temp[--i];
    }
}

void format_int_new(char** buf, int64_t val, int base, int is_signed, int width, char pad, int upper) {
    char temp[64];
    char* p = &temp[63];
    uint64_t uval;
    int is_neg = 0;

    if (is_signed && val < 0) {
        is_neg = 1;
        uval = (uint64_t)(-val);
    } else {
        uval = (uint64_t)val;
    }

    if (uval == 0) {
        *p-- = '0';
    } else {
        if (base == 10) {
            while (uval > 0) {
                *p-- = '0' + (uval % 10);
                uval /= 10;
            }
        } else if (base == 16) {
            char* hex_chars = upper ? hex_chars_upper : hex_chars_lower;
            while (uval > 0) {
                *p-- = hex_chars[uval % 16];
                uval /= 16;
            }
        } else {
            char* hex_chars = upper ? hex_chars_upper : hex_chars_lower;
            while (uval > 0) {
                *p-- = hex_chars[uval % base];
                uval /= base;
            }
        }
    }

    if (is_neg) {
        *p-- = '-';
    }

    int len = &temp[63] - p;
    while (len < width) {
        *p-- = pad;
        len++;
    }

    char* dest = *buf;
    p++;

    // Copy forward
    while (p <= &temp[63]) {
        *dest++ = *p++;
    }
    *buf = dest;
}

int main() {
    char buf1[1024];
    char* ptr;
    clock_t start, end;
    double time_taken;

    start = clock();
    for (int i = 0; i < 10000000; i++) {
        ptr = buf1;
        format_int_old(&ptr, 123456789, 10, 1, 0, ' ', 0);
        format_int_old(&ptr, -987654321, 10, 1, 12, ' ', 0);
        format_int_old(&ptr, 0xDEADBEEF, 16, 0, 8, '0', 1);
    }
    end = clock();
    time_taken = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Old format_int time: %f s\n", time_taken);

    start = clock();
    for (int i = 0; i < 10000000; i++) {
        ptr = buf1;
        format_int_new(&ptr, 123456789, 10, 1, 0, ' ', 0);
        format_int_new(&ptr, -987654321, 10, 1, 12, ' ', 0);
        format_int_new(&ptr, 0xDEADBEEF, 16, 0, 8, '0', 1);
    }
    end = clock();
    time_taken = (double)(end - start) / CLOCKS_PER_SEC;
    printf("New format_int time: %f s\n", time_taken);

    return 0;
}
