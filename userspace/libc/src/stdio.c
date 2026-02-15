#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

static void putc(int fd, char c) {
    write(fd, &c, 1);
}

static void print_int(int fd, int n, int base) {
    char buf[32];
    int i = 0;
    int sign = 0;

    if (n == 0) {
        putc(fd, '0');
        return;
    }

    if (n < 0 && base == 10) {
        sign = 1;
        n = -n;
    }

    unsigned int u = (unsigned int)n;

    while (u > 0) {
        int d = u % base;
        buf[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        u /= base;
    }

    if (sign) putc(fd, '-');
    while (i > 0) putc(fd, buf[--i]);
}

static void print_string(int fd, const char *s) {
    if (!s) s = "(null)";
    while (*s) putc(fd, *s++);
}

int dprintf(int fd, const char *format, ...) {
    va_list args;
    va_start(args, format);

    const char *p = format;
    int count = 0;

    while (*p) {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 'd':
                    print_int(fd, va_arg(args, int), 10);
                    break;
                case 'x':
                    print_int(fd, va_arg(args, int), 16);
                    break;
                case 's':
                    print_string(fd, va_arg(args, char*));
                    break;
                case 'c':
                    putc(fd, (char)va_arg(args, int));
                    break;
                case '%':
                    putc(fd, '%');
                    break;
                default:
                    putc(fd, '%');
                    putc(fd, *p);
                    break;
            }
        } else {
            putc(fd, *p);
        }
        p++;
    }

    va_end(args);
    return count; /* Should define correct return value */
}

int printf(const char *format, ...) {
    /* Use dprintf to stdout (1) */
    va_list args;
    va_start(args, format);
    /* Manually unpacking because no vdprintf */
    /* ... reuse logic? Just copy for now or make a helper */
    /* Let's just implement printf separately to keep it simple without vararg passing issues in minimal libc */

    const char *p = format;
    while (*p) {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 'd':
                    print_int(1, va_arg(args, int), 10);
                    break;
                case 'x':
                    print_int(1, va_arg(args, int), 16);
                    break;
                case 's':
                    print_string(1, va_arg(args, char*));
                    break;
                case 'c':
                    putc(1, (char)va_arg(args, int));
                    break;
                case '%':
                    putc(1, '%');
                    break;
                default:
                    putc(1, '%');
                    putc(1, *p);
                    break;
            }
        } else {
            putc(1, *p);
        }
        p++;
    }
    va_end(args);
    return 0;
}
