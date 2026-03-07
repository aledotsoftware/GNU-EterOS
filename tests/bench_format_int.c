#define __ETEROS_HOST_TEST__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "../include/types.h"

static void buffer_append(char* str, size_t size, size_t* pos, char c) {
    if (size > 0 && *pos < size - 1) {
        str[*pos] = c;
    }
    (*pos)++;
}

static void format_int_old(char* str, size_t size, size_t* pos, uint64_t val, int base, int width, int zeropad, int left_justify, int uppercase, int is_signed) {
    char temp[65];
    int i = 0;
    int is_neg = 0;

    if (is_signed && base == 10 && (int64_t)val < 0) {
        is_neg = 1;
        val = -val;
    }

    if (val == 0) {
        temp[i++] = '0';
    } else {
        while (val != 0) {
            int rem = val % base;
            if (rem < 10) temp[i++] = rem + '0';
            else temp[i++] = (uppercase ? 'A' : 'a') + rem - 10;
            val /= base;
        }
    }

    int len = i + (is_neg ? 1 : 0);
    int padding = width - len;

    if (!left_justify && !zeropad && padding > 0) {
        while (padding-- > 0) buffer_append(str, size, pos, ' ');
    }

    if (is_neg) buffer_append(str, size, pos, '-');

    if (zeropad && padding > 0) {
        while (padding-- > 0) buffer_append(str, size, pos, '0');
    }

    while (i > 0) buffer_append(str, size, pos, temp[--i]);

    if (left_justify && padding > 0) {
        while (padding-- > 0) buffer_append(str, size, pos, ' ');
    }
}

static const char hex_chars_upper[] = "0123456789ABCDEF";
static const char hex_chars_lower[] = "0123456789abcdef";

static void format_int_new(char* str, size_t size, size_t* pos, uint64_t val, int base, int width, int zeropad, int left_justify, int uppercase, int is_signed) {
    char temp[65];
    int i = 64; // Fill backwards
    int is_neg = 0;

    if (is_signed && base == 10 && (int64_t)val < 0) {
        is_neg = 1;
        val = -val;
    }

    if (val == 0) {
        temp[--i] = '0';
    } else {
        if (base == 10) {
            while (val != 0) {
                temp[--i] = '0' + (val % 10);
                val /= 10;
            }
        } else if (base == 16) {
            const char* hex_chars = uppercase ? hex_chars_upper : hex_chars_lower;
            while (val != 0) {
                temp[--i] = hex_chars[val & 0xF];
                val >>= 4;
            }
        } else {
            while (val != 0) {
                int rem = val % base;
                if (rem < 10) temp[--i] = rem + '0';
                else temp[--i] = (uppercase ? 'A' : 'a') + rem - 10;
                val /= base;
            }
        }
    }

    int len = 64 - i + (is_neg ? 1 : 0);
    int padding = width - len;

    if (!left_justify && !zeropad && padding > 0) {
        while (padding-- > 0) buffer_append(str, size, pos, ' ');
    }

    if (is_neg) buffer_append(str, size, pos, '-');

    if (zeropad && padding > 0) {
        while (padding-- > 0) buffer_append(str, size, pos, '0');
    }

    // Direct buffer write if space permits
    size_t copy_len = 64 - i;
    if (size > 0 && *pos + copy_len < size) {
        memcpy(&str[*pos], &temp[i], copy_len);
        *pos += copy_len;
    } else {
        while (i < 64) buffer_append(str, size, pos, temp[i++]);
    }

    if (left_justify && padding > 0) {
        while (padding-- > 0) buffer_append(str, size, pos, ' ');
    }
}

int main() {
    char buf1[1024];
    char buf2[1024];

    size_t pos1 = 0, pos2 = 0;
    format_int_old(buf1, sizeof(buf1), &pos1, -1234567890LL, 10, 20, 0, 0, 0, 1);
    format_int_new(buf2, sizeof(buf2), &pos2, -1234567890LL, 10, 20, 0, 0, 0, 1);
    buf1[pos1] = '\0';
    buf2[pos2] = '\0';
    printf("Old: '%s'\n", buf1);
    printf("New: '%s'\n", buf2);
    if (strcmp(buf1, buf2) != 0) {
        printf("Mismatch!\n");
        return 1;
    }

    int iterations = 10000000;
    clock_t start, end;

    start = clock();
    for (int i = 0; i < iterations; i++) {
        size_t pos = 0;
        format_int_old(buf1, sizeof(buf1), &pos, 1234567890LL + i, 16, 0, 0, 0, 0, 0);
    }
    end = clock();
    double time_old = (double)(end - start) / CLOCKS_PER_SEC;

    start = clock();
    for (int i = 0; i < iterations; i++) {
        size_t pos = 0;
        format_int_new(buf2, sizeof(buf2), &pos, 1234567890LL + i, 16, 0, 0, 0, 0, 0);
    }
    end = clock();
    double time_new = (double)(end - start) / CLOCKS_PER_SEC;

    printf("format_int_old: %f s\n", time_old);
    printf("format_int_new: %f s\n", time_new);
    return 0;
}
