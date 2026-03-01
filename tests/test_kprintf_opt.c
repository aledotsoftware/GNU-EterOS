#define __ETEROS_HOST_TEST__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

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

void hal_console_write(const char* s) {
    (void)s;
}

#define format_int format_int_old
#define buffer_append buffer_append_old
#define vsnprintf vsnprintf_old
#define snprintf snprintf_old
#define kprintf kprintf_old

#include "../kernel/stdio.c"

#undef format_int
#undef buffer_append
#undef vsnprintf
#undef snprintf
#undef kprintf

static inline void buffer_append_new(char* str, size_t size, size_t* pos, char c) {
    if (size > 0 && *pos < size - 1) {
        str[*pos] = c;
    }
    (*pos)++;
}

static char hex_chars_lower[] = "0123456789abcdef";
static char hex_chars_upper[] = "0123456789ABCDEF";

static void format_int_new(char* str, size_t size, size_t* pos, uint64_t val, int base, int width, int zeropad, int left_justify, int uppercase, int is_signed) {
    char temp[65];
    char* p = &temp[64];
    int is_neg = 0;

    if (is_signed && base == 10 && (int64_t)val < 0) {
        is_neg = 1;
        val = (uint64_t)(-(int64_t)val);
    }

    if (val == 0) {
        *--p = '0';
    } else {
        if (base == 10) {
            while (val > 0) {
                *--p = '0' + (val % 10);
                val /= 10;
            }
        } else if (base == 16) {
            char* hex_chars = uppercase ? hex_chars_upper : hex_chars_lower;
            while (val > 0) {
                *--p = hex_chars[val % 16];
                val /= 16;
            }
        } else {
            char* hex_chars = uppercase ? hex_chars_upper : hex_chars_lower;
            while (val > 0) {
                *--p = hex_chars[val % base];
                val /= base;
            }
        }
    }

    int len = &temp[64] - p + (is_neg ? 1 : 0);
    int padding = width - len;

    if (!left_justify && !zeropad && padding > 0) {
        while (padding-- > 0) buffer_append_new(str, size, pos, ' ');
    }

    if (is_neg) buffer_append_new(str, size, pos, '-');

    if (zeropad && padding > 0) {
        while (padding-- > 0) buffer_append_new(str, size, pos, '0');
    }

    while (p < &temp[64]) {
        buffer_append_new(str, size, pos, *p++);
    }

    if (left_justify && padding > 0) {
        while (padding-- > 0) buffer_append_new(str, size, pos, ' ');
    }
}

int vsnprintf_new(char* str, size_t size, const char* format, va_list ap) {
    size_t pos = 0;
    const char* p = format;

    if (size > 0 && str) str[0] = '\0';

    while (*p) {
        if (*p != '%') {
            buffer_append_new(str, size, &pos, *p++);
            continue;
        }

        p++; /* Skip '%' */

        /* Flags */
        int zeropad = 0;
        int left_justify = 0;
        while (*p == '0' || *p == '-') {
            if (*p == '0') zeropad = 1;
            else if (*p == '-') left_justify = 1;
            p++;
        }

        /* Width */
        int width = 0;
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        if (left_justify) zeropad = 0;

        /* Precision */
        int precision = -1;
        if (*p == '.') {
            p++;
            precision = 0;
            while (*p >= '0' && *p <= '9') {
                precision = precision * 10 + (*p - '0');
                p++;
            }
        }

        /* Length modifier */
        int is_long = 0;
        int is_longlong = 0;
        if (*p == 'l') {
            is_long = 1;
            p++;
            if (*p == 'l') {
                is_longlong = 1;
                p++;
            }
        }

        /* Specifier */
        switch (*p) {
            case 'd':
            case 'i': {
                int64_t val;
                if (is_longlong) val = va_arg(ap, long long);
                else if (is_long) val = va_arg(ap, long);
                else val = va_arg(ap, int);
                format_int_new(str, size, &pos, (uint64_t)val, 10, width, zeropad, left_justify, 0, 1);
                break;
            }
            case 'u': {
                uint64_t val;
                if (is_longlong) val = va_arg(ap, unsigned long long);
                else if (is_long) val = va_arg(ap, unsigned long);
                else val = va_arg(ap, unsigned int);
                format_int_new(str, size, &pos, val, 10, width, zeropad, left_justify, 0, 0);
                break;
            }
            case 'x':
            case 'X': {
                uint64_t val;
                if (is_longlong) val = va_arg(ap, unsigned long long);
                else if (is_long) val = va_arg(ap, unsigned long);
                else val = va_arg(ap, unsigned int);
                format_int_new(str, size, &pos, val, 16, width, zeropad, left_justify, (*p == 'X'), 0);
                break;
            }
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                size_t len = eteros_strlen(s);
                if (precision >= 0 && (size_t)precision < len) len = (size_t)precision;
                int padding = width - (int)len;

                if (!left_justify) {
                    while (padding-- > 0) buffer_append_new(str, size, &pos, ' ');
                }
                for (size_t i = 0; i < len; i++) buffer_append_new(str, size, &pos, s[i]);
                if (left_justify) {
                    while (padding-- > 0) buffer_append_new(str, size, &pos, ' ');
                }
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                buffer_append_new(str, size, &pos, c);
                break;
            }
            case 'p': {
                void* ptr = va_arg(ap, void*);
                buffer_append_new(str, size, &pos, '0');
                buffer_append_new(str, size, &pos, 'x');
                format_int_new(str, size, &pos, (uintptr_t)ptr, 16, 0, 0, 0, 0, 0);
                break;
            }
            case '%': {
                buffer_append_new(str, size, &pos, '%');
                break;
            }
            default: {
                buffer_append_new(str, size, &pos, '%');
                if (*p) buffer_append_new(str, size, &pos, *p);
                break;
            }
        }
        if (*p) p++;
    }

    /* Null terminate */
    if (size > 0 && str) {
        if (pos < size) str[pos] = '\0';
        else str[size - 1] = '\0';
    }

    return (int)pos;
}

int snprintf_new(char* str, size_t size, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf_new(str, size, format, ap);
    va_end(ap);
    return ret;
}


int main() {
    char buf1[1024];
    char buf2[1024];

    snprintf_old(buf1, sizeof(buf1), "Test %d %x %s %p", 123456789, 0xDEADBEEF, "Hello World", (void*)0x12345678);
    snprintf_new(buf2, sizeof(buf2), "Test %d %x %s %p", 123456789, 0xDEADBEEF, "Hello World", (void*)0x12345678);

    printf("Old: %s\n", buf1);
    printf("New: %s\n", buf2);

    if (eteros_strcmp(buf1, buf2) != 0) {
        printf("FAILED 1\n");
        return 1;
    }

    snprintf_old(buf1, sizeof(buf1), "Test %-10d %08x", -123, 0xABC);
    snprintf_new(buf2, sizeof(buf2), "Test %-10d %08x", -123, 0xABC);
    printf("Old: %s\n", buf1);
    printf("New: %s\n", buf2);
    if (eteros_strcmp(buf1, buf2) != 0) {
        printf("FAILED 2\n");
        return 1;
    }

    printf("ALL TESTS PASSED\n");

    return 0;
}
