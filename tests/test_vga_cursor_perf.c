#include <stdio.h>
#include <string.h>

#define __ETEROS_HOST_TEST__ 1

/* Preprocessor hacks to build the file */
#define _terminal_putchar  eteros__terminal_putchar
#define terminal_putchar   eteros_terminal_putchar
#define terminal_write_string eteros_terminal_write_string
#define terminal_initialize eteros_terminal_initialize

#define outb eteros_outb
#define inb eteros_inb

#include "../include/types.h"
#include "../include/boot.h"
#include "../include/vga.h"

uint16_t mock_vga_buffer[VGA_SIZE];

/* Mock I/O Functions */
int outb_calls = 0;
void eteros_outb(uint16_t port, uint8_t val) {
    (void)port;
    (void)val;
    outb_calls++;
}

uint8_t eteros_inb(uint16_t port) {
    (void)port;
    return 0;
}

/* Mock Framebuffer Functions */
uint32_t framebuffer_get_height(void) { return 600; }
uint32_t framebuffer_get_width(void) { return 800; }
void framebuffer_scroll(uint32_t pixels, uint32_t bg_color) { (void)pixels; (void)bg_color; }
void framebuffer_putchar(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) { (void)c; (void)x; (void)y; (void)fg; (void)bg; }
void framebuffer_clear(uint32_t bg) { (void)bg; }
void framebuffer_init(boot_info_t* info) { (void)info; }


/* Include the actual source file */
#undef VGA_BUFFER_ADDR
#define VGA_BUFFER_ADDR ((uintptr_t)mock_vga_buffer)

#include "../kernel/drivers/video/vga.c"

int main() {
    eteros_terminal_initialize(NULL);
    outb_calls = 0; // Reset after init

    const char* test_str = "This is a performance test for the VGA driver cursor update function.";

    // Simulate terminal_write_string loop directly, since the real one uses the public terminal_putchar
    // Wait, the real terminal_write_string in vga.c uses _terminal_putchar!
    // Let's verify how it's implemented. Oh, terminal_write_string uses _terminal_putchar.
    // The issue is terminal_putchar uses _terminal_putchar AND vga_update_cursor().
    // So writing a string using multiple terminal_putchar() calls will be slow.
    // Let's test terminal_putchar directly to simulate `dev_tty_write` which uses it.

    for (size_t i = 0; i < strlen(test_str); i++) {
        eteros_terminal_putchar(test_str[i]);
    }

    printf("Wrote %lu characters using terminal_putchar.\n", (unsigned long)strlen(test_str));
    printf("outb() called %d times.\n", outb_calls);

    outb_calls = 0;
    eteros_terminal_write_string(test_str);
    printf("Wrote %lu characters using terminal_write_string.\n", (unsigned long)strlen(test_str));
    printf("outb() called %d times.\n", outb_calls);

    return 0;
}
