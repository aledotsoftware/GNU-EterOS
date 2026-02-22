#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h> // For O_RDONLY etc.
#include <stdio.h> // For printf in test runner

// Mock or override conflicting symbols
// We rename functions implemented in stdio.c to test them
#define snprintf my_snprintf
#define sprintf my_sprintf
#define printf my_printf
#define vcbprintf my_vcbprintf
#define fprintf my_fprintf
#define fopen my_fopen
#define fclose my_fclose
#define fread my_fread
#define fwrite my_fwrite
#define fseek my_fseek
#define ftell my_ftell
#define rewind my_rewind
#define fflush my_fflush
#define fputc my_fputc
#define fgetc my_fgetc
#define fgets my_fgets
#define fputs my_fputs
#define stdin my_stdin
#define stdout my_stdout
#define stderr my_stderr

// We need to ensure stdio.c uses our mocked stdio.h (empty) for FILE struct definition
// And we need to ensure it uses system headers for everything else
// But wait, stdio.c includes <stdio.h>. If we use -I tests/mock_headers, it gets empty file.
// So we must include system stdio.h FIRST in this file (above), which defines FILE.
// But stdio.c defines struct _IO_FILE { ... }.
// System FILE is likely struct _IO_FILE too (on glibc).
// If we let stdio.c define struct _IO_FILE, it might conflict with system definition.
// OR we can rename struct _IO_FILE in stdio.c using a macro?
// struct _IO_FILE is hardcoded in stdio.c. We can't rename it easily with macro unless we define _IO_FILE macro.
// #define _IO_FILE my_IO_FILE
// Then stdio.c: struct my_IO_FILE { ... };
// And FILE *f = (FILE*)malloc(sizeof(FILE));
// stdio.c uses FILE.
// If FILE is defined by system stdio.h, it is struct _IO_FILE *.
// So malloc(sizeof(FILE)) is malloc(sizeof(struct _IO_FILE)).
// If we rename _IO_FILE to my_IO_FILE, then sizeof(FILE) refers to system struct.
// But stdio.c casts it to FILE*.
// This seems messy.

// Better approach:
// Don't include system stdio.h at all in compilation unit.
// Use dprintf (from stdio.c) or write (from unistd.h) for output.
// Use -nostdinc is extreme.
// Use -I tests/mock_headers to shadow stdio.h.
// Include necessary system headers manually in test runner.
// Manually define needed types/macros if missing.

#undef snprintf
#undef sprintf
#undef printf
#undef vcbprintf
#undef fprintf
#undef fopen
#undef fclose
#undef fread
#undef fwrite
#undef fseek
#undef ftell
#undef rewind
#undef fflush
#undef fputc
#undef fgetc
#undef fgets
#undef fputs
#undef stdin
#undef stdout
#undef stderr

// Clean slate
#define snprintf my_snprintf
#define sprintf my_sprintf
#define printf my_printf
#define vcbprintf my_vcbprintf
#define fprintf my_fprintf
#define fopen my_fopen
#define fclose my_fclose
#define fread my_fread
#define fwrite my_fwrite
#define fseek my_fseek
#define ftell my_ftell
#define rewind my_rewind
#define fflush my_fflush
#define fputc my_fputc
#define fgetc my_fgetc
#define fgets my_fgets
#define fputs my_fputs
#define stdin my_stdin
#define stdout my_stdout
#define stderr my_stderr

// Include source
#include "../userspace/libc/src/stdio.c"

#undef snprintf
#undef printf

// Helper to print using system write
void sys_puts(const char *s) {
    write(1, s, strlen(s));
    write(1, "\n", 1);
}

void sys_put_int(int n) {
    char buf[32];
    int i = 0;
    if (n == 0) {
        write(1, "0", 1);
        return;
    }
    if (n < 0) {
        write(1, "-", 1);
        n = -n;
    }
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    while (i > 0) {
        write(1, &buf[--i], 1);
    }
}

int main() {
    char buffer[100];
    int ret;

    sys_puts("Running snprintf tests...");

    // Test 1: snprintf(NULL, 0)
    ret = my_snprintf(NULL, 0, "test string");
    sys_puts("snprintf(NULL, 0) returned:");
    sys_put_int(ret);
    sys_puts("");

    if (ret >= 0) {
        sys_puts("PASS: snprintf(NULL, 0) safe.");
    } else {
        sys_puts("FAIL: snprintf(NULL, 0) error.");
        return 1;
    }

    // Test 2: Truncation
    ret = my_snprintf(buffer, 5, "Hello World");
    if (strcmp(buffer, "Hell") == 0) {
        sys_puts("PASS: snprintf truncation correct.");
    } else {
        sys_puts("FAIL: snprintf truncation incorrect.");
        return 1;
    }

    return 0;
}
