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


    /* Note: mode 0666 is implicit in syscall wrapper or handled by kernel default if passed 0 */
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
    // fflush(stream);
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
    // fflush(stream);
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
    /* Unbuffered for now, so always "success" */
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
            if (i == 0) return NULL; // EOF and no chars read
            break; // EOF after some chars
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
    return 1; // Non-negative value on success
}
