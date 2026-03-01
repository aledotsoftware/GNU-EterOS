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
#define itoa_s eteros_itoa_s_old
#define utoa_hex_s eteros_utoa_hex_s

#include "../kernel/string.c"

#undef itoa_s

void eteros_itoa_s_new(int64_t value, char* buffer, size_t buffer_size, int base) {
    if (buffer_size == 0) return;

    if (value == 0) {
        if (buffer_size > 1) {
            buffer[0] = '0';
            buffer[1] = '\0';
        } else {
            buffer[0] = '\0';
        }
        return;
    }

    char temp[65];
    char *p = &temp[64];
    *p = '\0';

    int is_negative = 0;
    uint64_t uvalue;

    if (value < 0 && base == 10) {
        is_negative = 1;
        uvalue = -(uint64_t)value;
    } else {
        uvalue = (uint64_t)value;
    }

    if (base == 10) {
        while (uvalue != 0) {
            *--p = (uvalue % 10) + '0';
            uvalue /= 10;
        }
    } else if (base == 16) {
        while (uvalue != 0) {
            int remainder = (int)(uvalue % 16);
            *--p = (remainder > 9) ? (remainder - 10 + 'A') : (remainder + '0');
            uvalue /= 16;
        }
    } else {
        while (uvalue != 0) {
            int remainder = (int)(uvalue % base);
            *--p = (remainder > 9) ? (remainder - 10 + 'A') : (remainder + '0');
            uvalue /= base;
        }
    }

    if (is_negative) {
        *--p = '-';
    }

    size_t len = &temp[64] - p;
    size_t chars_to_copy = len;

    if (chars_to_copy >= buffer_size) {
        chars_to_copy = buffer_size - 1;
    }

    /* Use simple loop or memcpy */
    for (size_t i = 0; i < chars_to_copy; i++) {
        buffer[i] = p[i];
    }
    buffer[chars_to_copy] = '\0';
}

int main() {
    char buf[64];
    clock_t start, end;
    double time_taken;

    start = clock();
    for (int i = 0; i < 10000000; i++) {
        eteros_itoa_s_old(123456789, buf, sizeof(buf), 10);
        eteros_itoa_s_old(-987654321, buf, sizeof(buf), 10);
        eteros_itoa_s_old(0xDEADBEEF, buf, sizeof(buf), 16);
    }
    end = clock();
    time_taken = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Old itoa_s time: %f s\n", time_taken);

    start = clock();
    for (int i = 0; i < 10000000; i++) {
        eteros_itoa_s_new(123456789, buf, sizeof(buf), 10);
        eteros_itoa_s_new(-987654321, buf, sizeof(buf), 10);
        eteros_itoa_s_new(0xDEADBEEF, buf, sizeof(buf), 16);
    }
    end = clock();
    time_taken = (double)(end - start) / CLOCKS_PER_SEC;
    printf("New itoa_s time: %f s\n", time_taken);

    return 0;
}
