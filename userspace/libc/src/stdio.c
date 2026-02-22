#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

struct _IO_FILE {
    int fd;
    int error;
    int eof;
};

static FILE __stdin = {0, 0, 0};
static FILE __stdout = {1, 0, 0};
static FILE __stderr = {2, 0, 0};

FILE *stdin = &__stdin;
FILE *stdout = &__stdout;
FILE *stderr = &__stderr;

/* Internal formatting function pointer type */
typedef void (*putc_func)(int c, void *arg);

struct buf_arg {
    char *buf;
    size_t size;
    size_t pos;
};

static void buf_putc(int c, void *arg) {
    struct buf_arg *ba = (struct buf_arg*)arg;
    if (ba->size > 0 && ba->pos < ba->size - 1) {
        ba->buf[ba->pos++] = (char)c;
    }
}

static void fd_putc(int c, void *arg) {
    int fd = (int)(long)arg;
    write(fd, &c, 1);
}

static int vcbprintf(putc_func out, void *arg, const char *fmt, va_list ap) {
    int count = 0;
    const char *p = fmt;

    while (*p) {
        if (*p != '%') {
            out(*p, arg);
            count++;
            p++;
            continue;
        }

        p++; // Skip '%'

        // Flags
        int f_minus = 0;
        int f_plus = 0;
        int f_space = 0;
        int f_hash = 0;
        int f_zero = 0;

        while (1) {
            if (*p == '-') f_minus = 1;
            else if (*p == '+') f_plus = 1;
            else if (*p == ' ') f_space = 1;
            else if (*p == '#') f_hash = 1;
            else if (*p == '0') f_zero = 1;
            else break;
            p++;
        }

        // Width
        int width = 0;
        if (*p == '*') {
            width = va_arg(ap, int);
            if (width < 0) {
                f_minus = 1;
                width = -width;
            }
            p++;
        } else {
            while (*p >= '0' && *p <= '9') {
                width = width * 10 + (*p - '0');
                p++;
            }
        }

        // Precision
        int prec = -1;
        if (*p == '.') {
            p++;
            prec = 0;
            if (*p == '*') {
                prec = va_arg(ap, int);
                p++;
            } else {
                while (*p >= '0' && *p <= '9') {
                    prec = prec * 10 + (*p - '0');
                    p++;
                }
            }
        }

        // Length
        int len_l = 0;
        int len_ll = 0;
        int len_h = 0;
        if (*p == 'l') {
            len_l = 1;
            p++;
            if (*p == 'l') {
                len_ll = 1;
                p++;
            }
        } else if (*p == 'h') {
            len_h = 1;
            p++;
            if (*p == 'h') { // hh
                p++;
            }
        } else if (*p == 'z') { // size_t
            len_l = 1; // treat as long
            p++;
        }

        // Specifier
        char spec = *p++;
        if (!spec) break;

        if (spec == 'd' || spec == 'i') {
            long long val;
            if (len_ll) val = va_arg(ap, long long);
            else if (len_l) val = va_arg(ap, long);
            else val = va_arg(ap, int);

            char buf[32];
            int i = 0;
            unsigned long long uval;
            int neg = 0;

            if (val < 0) {
                neg = 1;
                uval = -val;
            } else {
                uval = val;
            }

            if (uval == 0) {
                if (prec != 0) buf[i++] = '0';
            } else {
                while (uval) {
                    buf[i++] = (uval % 10) + '0';
                    uval /= 10;
                }
            }

            int len = i;
            int zeros = 0;
            if (prec > len) zeros = prec - len;

            int sign = 0;
            if (neg) sign = '-';
            else if (f_plus) sign = '+';
            else if (f_space) sign = ' ';

            int total_len = len + zeros + (sign ? 1 : 0);
            int padding = 0;
            if (width > total_len) padding = width - total_len;

            if (!f_minus && !f_zero) {
                while (padding--) { out(' ', arg); count++; }
            }

            if (sign) { out(sign, arg); count++; }

            if (!f_minus && f_zero) { // Zero padding comes after sign
                while (padding--) { out('0', arg); count++; }
            }

            while (zeros--) { out('0', arg); count++; }
            while (i > 0) { out(buf[--i], arg); count++; }

            if (f_minus) {
                while (padding--) { out(' ', arg); count++; }
            }

        } else if (spec == 'u' || spec == 'x' || spec == 'X' || spec == 'o' || spec == 'p') {
            unsigned long long val;
            if (spec == 'p') val = (unsigned long long)va_arg(ap, void*);
            else if (len_ll) val = va_arg(ap, unsigned long long);
            else if (len_l) val = va_arg(ap, unsigned long);
            else val = va_arg(ap, unsigned int);

            int base = 10;
            if (spec == 'x' || spec == 'X' || spec == 'p') base = 16;
            if (spec == 'o') base = 8;

            char buf[32];
            int i = 0;

            if (val == 0) {
                if (prec != 0) buf[i++] = '0';
                /* For p, maybe print (nil)? Standard says implementation defined. */
            } else {
                while (val) {
                    int d = val % base;
                    if (d < 10) buf[i++] = d + '0';
                    else buf[i++] = d - 10 + ((spec == 'X') ? 'A' : 'a');
                    val /= base;
                }
            }

            int len = i;
            int zeros = 0;
            if (prec > len) zeros = prec - len;

            int prefix_len = 0;
            if (f_hash && val != 0) {
                if (base == 8) prefix_len = 1; // 0
                if (base == 16) prefix_len = 2; // 0x
            }
            if (spec == 'p') prefix_len = 2; // 0x

            int total_len = len + zeros + prefix_len;
            int padding = 0;
            if (width > total_len) padding = width - total_len;

            if (!f_minus && !f_zero) {
                while (padding--) { out(' ', arg); count++; }
            }

            if (prefix_len) {
                if (spec == 'p' || base == 16) {
                    out('0', arg); out((spec == 'X')?'X':'x', arg); count+=2;
                } else if (base == 8) {
                    out('0', arg); count++;
                }
            }

            if (!f_minus && f_zero) {
                while (padding--) { out('0', arg); count++; }
            }

            while (zeros--) { out('0', arg); count++; }
            while (i > 0) { out(buf[--i], arg); count++; }

            if (f_minus) {
                while (padding--) { out(' ', arg); count++; }
            }

        } else if (spec == 's') {
            const char *s = va_arg(ap, char*);
            if (!s) s = "(null)";
            int len = strlen(s);
            if (prec >= 0 && prec < len) len = prec;

            int padding = 0;
            if (width > len) padding = width - len;

            if (!f_minus) {
                while (padding--) { out(' ', arg); count++; }
            }

            for (int k=0; k<len; k++) { out(s[k], arg); count++; }

            if (f_minus) {
                while (padding--) { out(' ', arg); count++; }
            }
        } else if (spec == 'c') {
            int c = va_arg(ap, int);
            int padding = 0;
            if (width > 1) padding = width - 1;

            if (!f_minus) {
                while (padding--) { out(' ', arg); count++; }
            }
            out(c, arg); count++;
            if (f_minus) {
                while (padding--) { out(' ', arg); count++; }
            }
        } else if (spec == '%') {
            out('%', arg); count++;
        }
    }
    return count;
}

int printf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vcbprintf(fd_putc, (void*)(long)1, format, ap);
    va_end(ap);
    return ret;
}

int dprintf(int fd, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vcbprintf(fd_putc, (void*)(long)fd, format, ap);
    va_end(ap);
    return ret;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    struct buf_arg ba = {str, size, 0};
    int ret = vcbprintf(buf_putc, &ba, format, ap);
    if (size > 0) {
        if (ba.pos < size) str[ba.pos] = 0;
        else str[size-1] = 0;
    }
    va_end(ap);
    return ret;
}

int fprintf(FILE *stream, const char *format, ...) {
    if (!stream) return -1;
    va_list ap;
    va_start(ap, format);
    int ret = vcbprintf(fd_putc, (void*)(long)stream->fd, format, ap);
    va_end(ap);
    return ret;
}

FILE *fopen(const char *pathname, const char *mode) {
    int flags = 0;
    int m_read = 0, m_write = 0, m_append = 0, m_create = 0, m_trunc = 0;

    const char *p = mode;
    while (*p) {
        switch (*p) {
            case 'r': m_read = 1; break;
            case 'w': m_write = 1; m_create = 1; m_trunc = 1; break;
            case 'a': m_write = 1; m_create = 1; m_append = 1; break;
            case '+': m_read = 1; m_write = 1; break;
            case 'b': break; /* Ignore binary */
        }
        p++;
    }

    if (m_read && m_write) flags = O_RDWR;
    else if (m_write) flags = O_WRONLY;
    else flags = O_RDONLY;

    if (m_create) flags |= O_CREAT;
    if (m_trunc) flags |= O_TRUNC;
    if (m_append) flags |= O_APPEND;

    int fd = open(pathname, flags);
    if (fd < 0) return NULL;

    FILE *f = (FILE *)malloc(sizeof(FILE));
    if (!f) {
        close(fd);
        return NULL;
    }
    f->fd = fd;
    f->error = 0;
    f->eof = 0;
    return f;
}

int fclose(FILE *stream) {
    if (!stream) return EOF;
    int ret = close(stream->fd);
    free(stream);
    return (ret < 0) ? EOF : 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || !ptr) return 0;
    if (size == 0 || nmemb == 0) return 0;

    size_t bytes_to_read = size * nmemb;
    ssize_t ret = read(stream->fd, ptr, bytes_to_read);

    if (ret < 0) {
        stream->error = 1;
        return 0;
    }
    if (ret == 0) {
        stream->eof = 1;
        return 0;
    }

    return (size_t)ret / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || !ptr) return 0;
    if (size == 0 || nmemb == 0) return 0;

    size_t bytes_to_write = size * nmemb;
    ssize_t ret = write(stream->fd, ptr, bytes_to_write);

    if (ret < 0) {
        stream->error = 1;
        return 0;
    }

    return (size_t)ret / size;
}

int fseek(FILE *stream, long offset, int whence) {
    if (!stream) return -1;
    int64_t ret = lseek(stream->fd, offset, whence);
    if (ret < 0) {
        stream->error = 1;
        return -1;
    }
    stream->eof = 0;
    return 0;
}

long ftell(FILE *stream) {
    if (!stream) return -1;
    return (long)lseek(stream->fd, 0, SEEK_CUR);
}

void rewind(FILE *stream) {
    fseek(stream, 0, SEEK_SET);
}

int fflush(FILE *stream) {
    (void)stream;
    return 0;
}

int fputc(int c, FILE *stream) {
    if (!stream) return EOF;
    unsigned char uc = (unsigned char)c;
    if (fwrite(&uc, 1, 1, stream) != 1) return EOF;
    return (int)uc;
}

int fgetc(FILE *stream) {
    if (!stream) return EOF;
    unsigned char c;
    if (fread(&c, 1, 1, stream) != 1) return EOF;
    return (int)c;
}

char *fgets(char *s, int size, FILE *stream) {
    if (!stream || !s || size <= 0) return NULL;

    int i = 0;
    int c;
    while (i < size - 1) {
        c = fgetc(stream);
        if (c == EOF) {
            if (i == 0) return NULL;
            break;
        }
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    s[i] = '\0';
    return s;
}

int fputs(const char *s, FILE *stream) {
    if (!stream || !s) return EOF;
    size_t len = strlen(s);
    if (fwrite(s, 1, len, stream) != len) return EOF;
    return 1;
}
