#include <stdlib.h>
#include <assert.h>
#ifndef assert
#define assert(x) do { if (!(x)) { printf("ASSERT FAILED: %s\n", #x); exit(1); } } while(0)
#endif
/* Define Host Test Mode at the very top */
#define __ETEROS_HOST_TEST__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

/* Do NOT include <string.h> yet if we suspect it resolves to our header */
/* But we need standard string functions for the test logic */
/* If we use -Iinclude, <string.h> might be our header. */
/* If we use -iquote include, <string.h> is system header. */

/* Let's use -iquote include for compilation.
   Then <vga.h> inside vga.c will fail if it uses <>.
   vga.c uses <vga.h>.
   So we must use -Iinclude.
*/

/* If -Iinclude is used, <string.h> is ours.
   Ours includes types.h.
   If __ETEROS_HOST_TEST__ is defined, types.h includes <stdint.h> etc.
   But ours does NOT include system <string.h>.
   So we miss printf etc? No, printf is in stdio.h.
   We miss memcpy, memset etc from libc.
   But we have them declared in ours.
   And we implement them in kernel/string.c (renamed via macros).
*/

/* So:
   1. Define macro.
   2. Include standard headers that don't conflict (stdio, stdlib).
   3. Include our headers.
*/

/* Mock Buffer Address */
uint16_t* mock_vga_buffer;
/* Ensure VGA_BUFFER_ADDR expands to the pointer */
#define VGA_BUFFER_ADDR ((uintptr_t)mock_vga_buffer)

/* Include headers */
#include "../include/types.h"
#include "../include/io.h"
#include "../include/vga.h"
#include "../include/boot.h"

/* Mock Framebuffer functions */
void framebuffer_init(boot_info_t* info) { (void)info; }
void framebuffer_clear(uint32_t color) { (void)color; }
void framebuffer_putchar(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    (void)c; (void)x; (void)y; (void)fg; (void)bg;
}
void framebuffer_scroll(uint32_t pixels, uint32_t bg_color) {
    (void)pixels; (void)bg_color;
}

/* Mock I/O functions */
void outb(uint16_t port, uint8_t value) {
    (void)port;
    (void)value;
}
void outw(uint16_t port, uint16_t value) {
    (void)port;
    (void)value;
}
void outl(uint16_t port, uint32_t value) {
    (void)port;
    (void)value;
}
uint8_t inb(uint16_t port) {
    (void)port;
    return 0;
}
uint16_t inw(uint16_t port) {
    (void)port;
    return 0;
}
uint32_t inl(uint16_t port) {
    (void)port;
    return 0;
}
void io_wait(void) {
}

/* Rename kernel functions to avoid conflict with libc symbols if linked */
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

/* Include source files directly */
/* This is done to unit test static functions and access internal state (e.g., terminal_buffer)
   without exposing them in the public header. */
#include "../kernel/string.c"
#include "../kernel/drivers/video/vga.c"

/* Helper to access Renamed functions if needed, but macros handle it */

int main() {
    printf("Running VGA scroll tests...\n");

    /* Allocate mock buffer */
    mock_vga_buffer = (uint16_t*)malloc(VGA_SIZE * sizeof(uint16_t));
    if (!mock_vga_buffer) {
        perror("Failed to allocate mock VGA buffer");
        return 1;
    }

    /* Initialize terminal */
    terminal_initialize(NULL);

    /* Check if buffer is cleared */
    uint16_t empty = vga_entry(' ', terminal_color);
    for (size_t i = 0; i < VGA_SIZE; i++) {
        if (mock_vga_buffer[i] != empty) {
            fprintf(stderr, "Initialization failed at index %zu\n", i);
            return 1;
        }
    }

    /* Fill screen with a pattern */
    /* Line i will have character ('A' + i % 26) */
    for (size_t row = 0; row < VGA_HEIGHT; row++) {
        uint16_t char_entry = vga_entry('A' + (row % 26), terminal_color);
        for (size_t col = 0; col < VGA_WIDTH; col++) {
            mock_vga_buffer[row * VGA_WIDTH + col] = char_entry;
        }
    }

    /* Verify pattern before scroll */
    for (size_t row = 0; row < VGA_HEIGHT; row++) {
        uint16_t char_entry = vga_entry('A' + (row % 26), terminal_color);
        for (size_t col = 0; col < VGA_WIDTH; col++) {
            assert(mock_vga_buffer[row * VGA_WIDTH + col] == char_entry);
        }
    }

    /* Perform scroll */
    terminal_scroll();

    /* Verify scroll result */
    /* Lines 0 to VGA_HEIGHT - 2 should contain data from previous lines 1 to VGA_HEIGHT - 1 */
    for (size_t row = 0; row < VGA_HEIGHT - 1; row++) {
        /* Old row was row + 1 */
        uint16_t expected_char = vga_entry('A' + ((row + 1) % 26), terminal_color);
        for (size_t col = 0; col < VGA_WIDTH; col++) {
            if (mock_vga_buffer[row * VGA_WIDTH + col] != expected_char) {
                fprintf(stderr, "Scroll failed at row %zu, col %zu. Expected 0x%04X, got 0x%04X\n",
                        row, col, expected_char, mock_vga_buffer[row * VGA_WIDTH + col]);
                return 1;
            }
        }
    }

    /* Last line should be empty */
    for (size_t col = 0; col < VGA_WIDTH; col++) {
        if (mock_vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + col] != empty) {
            fprintf(stderr, "Last line not cleared at col %zu\n", col);
            return 1;
        }
    }

    printf("Scroll test passed!\n");

    free(mock_vga_buffer);
    return 0;
}
