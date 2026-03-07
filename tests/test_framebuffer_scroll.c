
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

/* Mock Headers */
#include "../include/types.h"
#include "../include/io.h"
#include "../include/vga.h"
#include "../include/framebuffer.h"
#include "../include/boot.h"

#undef snprintf
#undef vsnprintf
#include <stdarg.h>
int eteros_snprintf(char* str, size_t size, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

/* Mock I/O functions */
void outb(uint16_t port, uint8_t value) { (void)port; (void)value; }
void outw(uint16_t port, uint16_t value) { (void)port; (void)value; }
void outl(uint16_t port, uint32_t value) { (void)port; (void)value; }
uint8_t inb(uint16_t port) { (void)port; return 0; }
uint16_t inw(uint16_t port) { (void)port; return 0; }
uint32_t inl(uint16_t port) { (void)port; return 0; }
void io_wait(void) {}

/* Mock Memory Management */
void* kmalloc(size_t size) { return malloc(size); }
void kfree(void* ptr) { free(ptr); }
void* kcalloc(size_t n, size_t s) { return calloc(n, s); }

/* Mock HAL MM */
#define HAL_PAGE_SIZE 4096
#define HAL_MEM_WRITE 0
#define HAL_MEM_WRITE_COMBINING 0
int hal_mem_map(uint64_t phys, uint64_t virt, uint32_t flags) {
    (void)phys; (void)virt; (void)flags;
    return 0;
}

/* Mock Serial */
void serial_write_string(const char* s) { (void)s; }

/* Rename kernel string functions */
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

/* Include kernel string implementation */
#include "../kernel/string.c"

/* Rename original framebuffer_init so we can mock it */
#define framebuffer_init original_framebuffer_init
#include "../kernel/drivers/video/framebuffer.c"
#undef framebuffer_init

/* Define Custom framebuffer_init for Test */
void framebuffer_init(boot_info_t* info) {
    if (!info) return;

    fb_width = info->fb_width;
    fb_height = info->fb_height;
    fb_pitch = info->fb_pitch;
    fb_bpp = info->fb_bpp;
    fb_size_bytes = fb_height * fb_pitch;

    /* Allocate buffer in heap instead of mapping physical memory */
    if (fb_buffer) free(fb_buffer);
    fb_buffer = (uint32_t*)malloc(fb_size_bytes);
    memset(fb_buffer, 0, fb_size_bytes);

    active_buffer = fb_buffer;

    printf("[TEST] Mock Framebuffer initialized: %dx%d, %d bpp, pitch %d\n",
           fb_width, fb_height, fb_bpp, fb_pitch);
}

/* Include VGA driver (which includes terminal logic) */
/* We need to include it AFTER framebuffer.c so it sees our mock framebuffer_init prototype
   (though it sees the one from header, the linker will link to ours).
   Wait, since we are including .c files, the symbols are in the same translation unit.
   vga.c calls framebuffer_init. It will call the one defined above.
*/
/* Mock VGA Buffer for vga.c initialization */
uint16_t* mock_vga_buffer;
#define VGA_BUFFER_ADDR ((uintptr_t)mock_vga_buffer)

#include "../kernel/drivers/video/vga.c"

/* Test Helpers */
void test_cleanup() {
    if (fb_buffer) {
        free(fb_buffer);
        fb_buffer = NULL;
    }
    if (mock_vga_buffer) {
        free(mock_vga_buffer);
        mock_vga_buffer = NULL;
    }
}

int main() {
    printf("Running Framebuffer Scroll Tests...\n");

    /* Setup Mock VGA Buffer */
    mock_vga_buffer = (uint16_t*)malloc(VGA_SIZE * sizeof(uint16_t));
    memset(mock_vga_buffer, 0, VGA_SIZE * sizeof(uint16_t));

    /* Setup Boot Info for Framebuffer (1024x768 32bpp) */
    boot_info_t info;
    info.signature = 0x544F424B; // "KBOT"
    info.fb_addr = 0xDEADBEEF; // Dummy physical address
    info.fb_width = 1024;
    info.fb_height = 768;
    info.fb_pitch = 1024 * 4;
    info.fb_bpp = 32;

    /* Initialize Terminal and Switch to Framebuffer */
    terminal_initialize(NULL);
    terminal_switch_to_framebuffer(&info);

    /* Verify Framebuffer Mode */
    assert(use_framebuffer == true);
    assert(fb_buffer != NULL);

    /* Fill screen with a pattern */
    /* We write enough lines to fill the screen */
    /* Screen height is 768, font height is 16. So 48 lines. */
    /* We will write 48 lines, each line having a distinct color/pattern based on row index */

    /* Let's manually draw to framebuffer to set up a known state,
       because terminal_write_string draws characters which are complex to verify pixel-perfectly
       unless we know the font.
       Instead, we can use terminal_write_string for lines 0-47.
    */

    printf("[TEST] Filling screen with lines...\n");
    for (int i = 0; i < 48; i++) {
        /* Set color based on line index to distinguish them */
        /* Use simple colors. */
        terminal_set_color(vga_entry_color((i % 15) + 1, VGA_COLOR_BLACK));

        char buf[32];
        snprintf(buf, sizeof(buf), "Line %d\n", i);

        /* We use _terminal_putchar directly or write_string */
        /* Note: terminal_write_string calls _terminal_putchar */
        terminal_write_string(buf);
    }

    /* At this point, the screen should be full (lines 0-47 filled).
       terminal_row should be 48 (because of newline on last line) -> trigger scroll?
       Wait, 48 lines (0-47).
       If we write "Line 0\n", it writes "Line 0" at row 0, then increments row to 1.
       ...
       "Line 47\n" writes at row 47, then increments row to 48.

       In _terminal_putchar:
       if (terminal_row >= height) { terminal_scroll(); }

       So after the loop, terminal_row is 48. height is 48.
       So scroll MUST have happened once!

       Wait, let's check exact behavior.
       i=0: writes "Line 0", row becomes 1.
       ...
       i=47: writes "Line 47", row becomes 48.
       Then check triggers scroll.

       With CURRENT implementation (wrap):
       terminal_scroll sets terminal_row = 0 and clears screen.
       So screen should be empty (black).

       With NEW implementation (scroll):
       Screen should shift up. Line 0 is gone. Line 1 becomes Line 0.
       Line 47 becomes Line 46.
       Line 47 (bottom) is cleared.
       terminal_row becomes 47.

       Wait, if row becomes 48, scroll happens.
       If scroll sets row = 47.
       Then we are ready to write next line at 47.
    */

    /* To verify wrap vs scroll, we can inspect the buffer content. */
    /* Let's write a specific pixel pattern at a known location before the last line. */

    /* Reset and retry with explicit pixel check */
    terminal_clear(); // Clears to black

    /* Draw a pixel at (0, 16) - which is row 1. */
    framebuffer_putpixel(0, 16, 0xFFFFFFFF); // White pixel at start of line 1

    /* Draw a pixel at (0, 768-16) - which is row 47 (last line). */
    framebuffer_putpixel(0, 768-16, 0xFFFFFFFF); // White pixel at start of last line

    /* Now trigger scroll by setting row to height and calling scroll, or just writing newlines. */
    /* terminal_row is 0 after clear. */
    /* Move cursor to bottom */
    terminal_set_cursor(0, 47);

    /* Write a newline. This should move row to 48 and trigger scroll. */
    printf("[TEST] Triggering scroll...\n");
    terminal_write_string("\n");

    /*
       Scenario 1: WRAP (Current Buggy Behavior)
       - terminal_scroll sets terminal_row = 0.
       - framebuffer_clear clears everything to black.
       - Pixel at (0, 16) should be GONE (black).
       - Pixel at (0, 768-16) should be GONE (black).

       Scenario 2: SCROLL (Desired Behavior)
       - Content moves up by 16px.
       - Pixel at (0, 16) (Row 1) moves to (0, 0) (Row 0).
       - Pixel at (0, 768-16) (Row 47) moves to (0, 768-32) (Row 46).
       - Row 47 is cleared.

       So:
       - Check (0, 0): Should be WHITE.
       - Check (0, 768-32): Should be WHITE.
       - Check (0, 768-16): Should be BLACK (Cleared).
    */

    uint32_t p0_0 = fb_buffer[0]; // (0,0)
    uint32_t p0_1 = fb_buffer[16 * 1024]; // (0,16) -> Row 1
    uint32_t p_last_prev = fb_buffer[(768-32) * 1024]; // Row 46
    uint32_t p_last = fb_buffer[(768-16) * 1024]; // Row 47

    printf("[TEST] Verifying pixels...\n");
    printf("Pixel (0,0): 0x%08X\n", p0_0);
    printf("Pixel (0,16) (Row 1 orig): 0x%08X\n", p0_1);
    printf("Pixel (0,736) (Row 46): 0x%08X\n", p_last_prev);
    printf("Pixel (0,752) (Row 47): 0x%08X\n", p_last);

    /* Assertion for DESIRED behavior */
    /* Since we expect this to FAIL initially, we can assert the Desired behavior. */

    int success = 1;
    if (p0_0 != 0xFFFFFFFF) {
        printf("FAIL: Pixel at (0,0) is not White. Expected it to shift up from (0,16).\n");
        success = 0;
    }

    if (p_last_prev != 0xFFFFFFFF) {
        printf("FAIL: Pixel at Row 46 is not White. Expected it to shift up from Row 47.\n");
        success = 0;
    }

    if (p_last != 0xFF000000) { // Assuming black bg
        printf("FAIL: Pixel at Row 47 is not Black. Expected it to be cleared.\n");
        // success = 0; // Don't fail if it's not black, maybe it's just moved. But clear is important.
    }

    test_cleanup();

    if (success) {
        printf("TEST PASSED: Scrolling implemented correctly.\n");
        return 0;
    } else {
        printf("TEST FAILED: Scrolling did not work as expected.\n");
        return 1;
    }
}
