#define __ETEROS_HOST_TEST__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "../include/types.h"

void itoa_s_new(int64_t value, char* buffer, size_t buffer_size, int base) {
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
    int i = 64; // Fill backwards
    int is_negative = 0;
    uint64_t uvalue;

    if (value < 0 && base == 10) {
        is_negative = 1;
        uvalue = -(uint64_t)value;
    } else {
        uvalue = (uint64_t)value;
    }

    // Base 10 is the most common use-case, optimize it
    if (base == 10) {
        while (uvalue != 0) {
            temp[--i] = '0' + (uvalue % 10);
            uvalue /= 10;
        }
    } else if (base == 16) {
        const char hex_chars[] = "0123456789ABCDEF";
        while (uvalue != 0) {
            temp[--i] = hex_chars[uvalue & 0xF];
            uvalue >>= 4;
        }
    } else {
        while (uvalue != 0) {
            int remainder = (int)(uvalue % base);
            temp[--i] = (remainder > 9) ? (remainder - 10 + 'A') : (remainder + '0');
            uvalue /= base;
        }
    }

    if (is_negative) {
        temp[--i] = '-';
    }

    size_t len = 64 - i;
    if (len >= buffer_size) {
        len = buffer_size - 1;
    }

    memcpy(buffer, &temp[i], len);
    buffer[len] = '\0';
}

void itoa_s_old(int64_t value, char* buffer, size_t buffer_size, int base) {
    if (buffer_size == 0) return;

    char temp[65];
    int i = 0;
    int is_negative = 0;

    if (value == 0) {
        if (buffer_size > 1) {
            buffer[0] = '0';
            buffer[1] = '\0';
        } else {
            buffer[0] = '\0';
        }
        return;
    }

    uint64_t uvalue;

    if (value < 0 && base == 10) {
        is_negative = 1;
        uvalue = -(uint64_t)value;
    } else {
        uvalue = (uint64_t)value;
    }

    while (uvalue != 0) {
        int remainder = (int)(uvalue % base);
        temp[i++] = (remainder > 9) ? (remainder - 10 + 'A') : (remainder + '0');
        uvalue /= base;
    }

    if (is_negative) {
        temp[i++] = '-';
    }

    int j = 0;
    size_t chars_to_copy = (size_t)i;

    if (chars_to_copy >= buffer_size) {
        chars_to_copy = buffer_size - 1;
    }

    while (chars_to_copy > 0) {
        buffer[j++] = temp[--i];
        chars_to_copy--;
    }
    buffer[j] = '\0';
}

int main() {
    char buf1[65];
    char buf2[65];

    itoa_s_old(-1234567890LL, buf1, sizeof(buf1), 10);
    itoa_s_new(-1234567890LL, buf2, sizeof(buf2), 10);
    printf("Old: %s\n", buf1);
    printf("New: %s\n", buf2);
    if (strcmp(buf1, buf2) != 0) {
        printf("Mismatch!\n");
        return 1;
    }

    int iterations = 10000000;
    clock_t start, end;

    start = clock();
    for (int i = 0; i < iterations; i++) {
        itoa_s_old(1234567890LL + i, buf1, sizeof(buf1), 10);
    }
    end = clock();
    double time_old = (double)(end - start) / CLOCKS_PER_SEC;

    start = clock();
    for (int i = 0; i < iterations; i++) {
        itoa_s_new(1234567890LL + i, buf2, sizeof(buf2), 10);
    }
    end = clock();
    double time_new = (double)(end - start) / CLOCKS_PER_SEC;

    printf("itoa_s_old: %f s\n", time_old);
    printf("itoa_s_new: %f s\n", time_new);
    return 0;
}
