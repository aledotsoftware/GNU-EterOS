#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <hal.h>
#include <types.h>

/* Helper to output a character to buffer with bounds check */
static void buffer_append(char* str, size_t size, size_t* pos, char c) {
    if (size > 0 && *pos < size - 1) {
        str[*pos] = c;
    }
    (*pos)++;
}

/* Helper for numbers */
/* ⚡ BOLT Optimization: Use backward filling to avoid string reversal, fast paths for base 10/16,
   and memcpy for faster buffer writing when space permits. */
static const char hex_chars_upper[] = "0123456789ABCDEF";
static const char hex_chars_lower[] = "0123456789abcdef";

static void format_int(char* str, size_t size, size_t* pos, uint64_t val, int base, int width, int zeropad, int left_justify, int uppercase, int is_signed) {
    char temp[65];
    int i = 64; /* Fill backwards */
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

    /* Direct copy to str if it fits to avoid character-by-character append overhead */
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

int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    size_t pos = 0;
    const char* p = format;

    if (size > 0 && str) str[0] = '\0';

    while (*p) {
        if (*p != '%') {
            buffer_append(str, size, &pos, *p++);
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
                format_int(str, size, &pos, (uint64_t)val, 10, width, zeropad, left_justify, 0, 1);
                break;
            }
            case 'u': {
                uint64_t val;
                if (is_longlong) val = va_arg(ap, unsigned long long);
                else if (is_long) val = va_arg(ap, unsigned long);
                else val = va_arg(ap, unsigned int);
                format_int(str, size, &pos, val, 10, width, zeropad, left_justify, 0, 0);
                break;
            }
            case 'x':
            case 'X': {
                uint64_t val;
                if (is_longlong) val = va_arg(ap, unsigned long long);
                else if (is_long) val = va_arg(ap, unsigned long);
                else val = va_arg(ap, unsigned int);
                format_int(str, size, &pos, val, 16, width, zeropad, left_justify, (*p == 'X'), 0);
                break;
            }
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                size_t len = strlen(s);
                if (precision >= 0 && (size_t)precision < len) len = (size_t)precision;
                int padding = width - (int)len;

                if (!left_justify) {
                    while (padding-- > 0) buffer_append(str, size, &pos, ' ');
                }
                for (size_t i = 0; i < len; i++) buffer_append(str, size, &pos, s[i]);
                if (left_justify) {
                    while (padding-- > 0) buffer_append(str, size, &pos, ' ');
                }
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                buffer_append(str, size, &pos, c);
                break;
            }
            case 'p': {
                void* ptr = va_arg(ap, void*);
                buffer_append(str, size, &pos, '0');
                buffer_append(str, size, &pos, 'x');
                format_int(str, size, &pos, (uintptr_t)ptr, 16, 0, 0, 0, 0, 0);
                break;
            }
            case '%': {
                buffer_append(str, size, &pos, '%');
                break;
            }
            default: {
                buffer_append(str, size, &pos, '%');
                if (*p) buffer_append(str, size, &pos, *p);
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

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int kprintf(const char* format, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    hal_console_write(buf);
    return ret;
}
