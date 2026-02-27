#define __ETEROS_HOST_TEST__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* Mock Headers */
#include "../include/types.h"
#include "../include/io.h"
#include "../include/vga.h"
#include "../include/framebuffer.h"
#include "../include/boot.h"

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

/* Redefine function names in string.c to avoid conflict with libc */
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
// NOW include string.c, so it defines eteros_memcpy
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

    if (fb_buffer) free(fb_buffer);
    fb_buffer = (uint32_t*)malloc(fb_size_bytes);
    memset(fb_buffer, 0, fb_size_bytes);

    active_buffer = fb_buffer;
}

/* Mock VGA Buffer for vga.c initialization */
uint16_t* mock_vga_buffer;
#define VGA_BUFFER_ADDR ((uintptr_t)mock_vga_buffer)

#include "../kernel/drivers/video/vga.c"

#include <time.h>

/* Benchmark Helpers */
void benchmark_current_scroll() {
    // Current terminal_scroll uses framebuffer_scroll
    terminal_scroll();
}

void benchmark_old_scroll() {
    // Simulate old behavior: clear screen and reset row
    framebuffer_clear(fb_bg);
    terminal_row = 0;
}

int main() {
    /* Setup Mock VGA Buffer */
    mock_vga_buffer = (uint16_t*)malloc(VGA_SIZE * sizeof(uint16_t));

    /* Setup Boot Info for Framebuffer (1024x768 32bpp) */
    boot_info_t info;
    info.signature = 0x544F424B;
    info.fb_addr = 0xDEADBEEF;
    info.fb_width = 1024;
    info.fb_height = 768;
    info.fb_pitch = 1024 * 4;
    info.fb_bpp = 32;

    terminal_initialize(NULL);
    terminal_switch_to_framebuffer(&info);

    /* Fill screen with random data to make memmove do actual work */
    for (uint32_t i = 0; i < fb_width * fb_height; i++) {
        fb_buffer[i] = i;
    }

    /* Measure Current Implementation */
    clock_t start = clock();
    int iterations = 1000;
    for(int i = 0; i < iterations; i++) {
        benchmark_current_scroll();
    }
    clock_t end = clock();
    double time_current = (double)(end - start) / 1000000.0;

    /* Measure Old Implementation */
    start = clock();
    for(int i = 0; i < iterations; i++) {
        benchmark_old_scroll();
    }
    end = clock();
    double time_old = (double)(end - start) / 1000000.0;

    printf("Benchmark Results (%d iterations):\n", iterations);
    printf("Current (Scroll + Clear Line): %.6f s\n", time_current);
    printf("Old (Clear Screen):            %.6f s\n", time_old);

    /* Note: Old implementation might be faster in raw memory ops because
       rep stosq (memset) is extremely optimized by hardware for zeroing/filling,
       while memmove (rep movsq) involves reading AND writing.
       HOWEVER, the user experience of "clearing screen" (losing context) is the main issue.
       But let's see the numbers.
       Actually, memmove moves (Height-1) lines. Memset clears Height lines.
       Moving 3MB is R+W. Clearing 3MB is W only.
       So clearing IS faster technically.
       But scrolling is FUNCTIONALLY required.
       Wait, usually scrolling only clears 1 line (memset small area) + moves big area.
       The "bad" implementation clears entire screen (memset big area).

       Memory bandwidth:
       Scroll: Read (H-1) + Write (H-1) + Write (1 line)
       Clear: Write (H)

       So Clear is definitely faster than Scroll.
       So the prompt "clearing... is slow" might refer to the fact that it requires *redrawing* content if you wanted to keep it?
       But the code just says `terminal_row = 0`, so it just wipes history.

       The prompt says: "Clearing the entire framebuffer on scroll is visually jarring and slow."
       "Slow" might be subjective or refer to the *perception* or maybe I am wrong about memory speed.
       Or maybe it implies that to *maintain* context you would have to redraw everything?
       But the "bad" code simply *didn't* maintain context. It just wrapped.

       Anyway, I will report the numbers.
    */

    free(mock_vga_buffer);
    free(fb_buffer);
    return 0;
}
