#define __ETEROS_HOST_TEST__
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h> /* Include system string.h first to get real declarations */

/* Mock includes */
/* We use relative paths assuming running from root, or adjust with -I. */
#include "include/types.h"
#include "include/vga.h"
#include "include/string.h"
#include "include/keyboard.h"
#include "include/timer.h"
#include "include/io.h"

/* Mock Logging */
char log_buffer[65536];

void log_append(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = strlen(log_buffer);
    if (len < sizeof(log_buffer) - 200) {
        vsnprintf(log_buffer + len, sizeof(log_buffer) - len, fmt, args);
    }
    va_end(args);
}

/* VGA Mocks */
void terminal_initialize(boot_info_t* info) {
    log_append("terminal_initialize()\n");
}
void terminal_write_string(const char* str) {
    log_append("write_string: \"%s\"\n", str);
}
void terminal_write_colored(const char* str, vga_color_t fg, vga_color_t bg) {
    log_append("write_colored: \"%s\" (fg=%d)\n", str, fg);
}
void terminal_set_color(uint8_t color) {}
void terminal_putchar(char c) {}
void terminal_clear(void) {}
void terminal_set_cursor(size_t x, size_t y) {}
void terminal_scroll(void) {}

/* Keyboard Mock */
char keyboard_getchar(void) {
    return 0;
}

/* Timer Mock */
void timer_wait(uint32_t ms) {}

/* IO Mocks */
void outb(uint16_t port, uint8_t value) {}
void outw(uint16_t port, uint16_t value) {}
void outl(uint16_t port, uint32_t value) {}
uint8_t inb(uint16_t port) { return 0; }
uint16_t inw(uint16_t port) { return 0; }
uint32_t inl(uint16_t port) { return 0; }
void io_wait(void) {}
void cpu_halt(void) {}

/* String Mocks */
void itoa_s(int64_t value, char* buffer, size_t buffer_size, int base) {
    if (base == 10) {
        snprintf(buffer, buffer_size, "%lld", (long long)value);
    } else {
        snprintf(buffer, buffer_size, "?");
    }
}
void utoa_hex_s(uint64_t value, char* buffer, size_t buffer_size) {}

/* Standard library overrides - prevent recursion by undefining macros */
#undef memcpy
#undef memset
#undef memmove
#undef memcmp
#undef strlen
#undef strncpy
#undef strlcpy
#undef strcmp

void* eteros_memcpy(void* dest, const void* src, size_t n) { return memcpy(dest, src, n); }
void* eteros_memset(void* s, int c, size_t n) { return memset(s, c, n); }
void* eteros_memmove(void* dest, const void* src, size_t n) { return memmove(dest, src, n); }
int eteros_memcmp(const void* s1, const void* s2, size_t n) { return memcmp(s1, s2, n); }
size_t eteros_strlen(const char* s) { return strlen(s); }
char* eteros_strncpy(char* dest, const char* src, size_t n) { return strncpy(dest, src, n); }
size_t eteros_strlcpy(char* dest, const char* src, size_t size) {
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
    return strlen(dest);
}
int eteros_strcmp(const char* s1, const char* s2) { return strcmp(s1, s2); }


/* Include the file under test */
#include "kernel/apps/santitravel.c"

int main() {
    printf("Running SantiTravel Refactor Tests...\n");

    /* Test 1: Visited List */
    printf("Test 1: Visited List (show_destinations_list(true))\n");
    log_buffer[0] = '\0';
    show_destinations_list(true);

    if (!strstr(log_buffer, "Lugares Conocidos")) {
        printf("FAIL: Header 'Lugares Conocidos' missing.\n");
        return 1;
    }
    if (!strstr(log_buffer, "write_colored: \"Posadas\" (fg=15)")) {
        printf("FAIL: 'Posadas' missing or wrong color (expected White/15).\n");
        return 1;
    }
    if (strstr(log_buffer, "write_colored: \"Bariloche\"")) {
        printf("FAIL: 'Bariloche' found in visited list.\n");
        return 1;
    }


    /* Test 2: Pending List */
    printf("Test 2: Pending List (show_destinations_list(false))\n");
    log_buffer[0] = '\0';
    show_destinations_list(false);

    if (!strstr(log_buffer, "Lugares por Conocer")) {
        printf("FAIL: Header 'Lugares por Conocer' missing.\n");
        return 1;
    }
    if (!strstr(log_buffer, "write_colored: \"Bariloche\" (fg=7)")) {
        printf("FAIL: 'Bariloche' missing or wrong color (expected Light Grey/7).\n");
        return 1;
    }
    if (strstr(log_buffer, "write_colored: \"Posadas\"")) {
        printf("FAIL: 'Posadas' found in pending list.\n");
        return 1;
    }

    printf("SUCCESS: All tests passed.\n");
    return 0;
}
