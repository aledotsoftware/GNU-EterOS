#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock types
// typedef uint32_t size_t; // Removed conflict
typedef uint32_t uint32_t;
typedef int32_t int32_t;
typedef uint64_t uint64_t;
typedef uint8_t uint8_t;

// Mock constants
#define TIMER_HZ 1000

// Mock Functions
uint32_t framebuffer_get_width(void) { return 1024; }
uint32_t framebuffer_get_height(void) { return 768; }
uint32_t framebuffer_get_pitch(void) { return 1024 * 4; }
uint32_t framebuffer_get_bpp(void) { return 32; }

static uint32_t mock_fb_buffer[1024 * 768];
uint32_t* framebuffer_get_buffer(void) { return mock_fb_buffer; }

uint32_t framebuffer_get_bpp(void) { return 32; }
uint32_t framebuffer_get_pitch(void) { return 1024 * 4; }

static uint32_t mock_fb[1024 * 768];
uint32_t* framebuffer_get_buffer(void) { return mock_fb; }

void framebuffer_clear(uint32_t color) {
    printf("[MOCK] Framebuffer clear: 0x%08X\n", color);
}

void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    // Only print first few pixels to avoid spam
    static int count = 0;
    if (count < 5) {
        printf("[MOCK] Putpixel (%d, %d) = 0x%08X\n", x, y, color);
        count++;
    }
}

void terminal_clear(void) {
    printf("[MOCK] Terminal clear.\n");
}

void hal_console_write(const char* s) {
    printf("%s", s);
}

// Mock Initrd
void* initrd_read_file(const char* name, uint32_t* size) {
    printf("[MOCK] Reading file: %s\n", name);
    if (strcmp(name, "logo.raw") == 0) {
        *size = 200 * 200 * 4;
        void* data = malloc(*size);
        // Fill with dummy data
        memset(data, 0xAB, *size);
        return data;
    }
    return NULL;
}

// Mock Timer
uint64_t current_ticks = 0;
uint64_t timer_get_ticks(void) {
    return current_ticks++;
}

// The function to test (copied from kernel/main.c)
static void show_splash(void) {
    uint32_t size = 0;
    void* logo_data = initrd_read_file("logo.raw", &size);
    if (!logo_data) {
        hal_console_write("[SPLASH] Logo not found in initrd.\n");
        return;
    }

    /* Use framebuffer functions to draw */
    uint32_t screen_w = framebuffer_get_width();
    uint32_t screen_h = framebuffer_get_height();

    /* Clear to White */
    framebuffer_clear(0xFFFFFFFF);

    /* Center image (200x200) */
    int32_t start_x = (screen_w - 200) / 2;
    int32_t start_y = (screen_h - 200) / 2;

    if (start_x < 0) start_x = 0;
    if (start_y < 0) start_y = 0;

    uint32_t* pixel_data = (uint32_t*)logo_data;

    /* Simulate the loop but limit iterations for speed in test */
    /* In real kernel code, this iterates all pixels */
    printf("[MOCK] Drawing splash screen at (%d, %d)...\n", start_x, start_y);

    /* ⚡ BOLT Optimization: Direct memory access for 32bpp Framebuffer */
    if (framebuffer_get_bpp() == 32) {
        uint32_t* fb_buf = framebuffer_get_buffer();
        uint32_t fb_pitch = framebuffer_get_pitch();

        if (fb_buf) {
            for (int y = 0; y < 200; y++) {
                int draw_y = start_y + y;
                if (draw_y >= (int)screen_h) break;

                uint32_t* dest_row = (uint32_t*)((uint8_t*)fb_buf + (draw_y * fb_pitch) + (start_x * 4));
                uint32_t* src_row = pixel_data + (y * 200);

                for (int x = 0; x < 200; x++) {
                    int draw_x = start_x + x;
                    if (draw_x >= (int)screen_w) break;

                    dest_row[x] = src_row[x];
                }
            }
            printf("[MOCK] Fast 32-bit path executed.\n");
        }
    } else {
        /* Fallback for other depths */
        for (int y = 0; y < 200; y++) {
            for (int x = 0; x < 200; x++) {
                 uint32_t color = pixel_data[y * 200 + x];
                 framebuffer_putpixel(start_x + x, start_y + y, color);
            }
        }
    }

    /* Wait ~2 seconds (busy wait on timer ticks to keep scheduler running) */
    /* Simulate time passing */
    printf("[MOCK] Waiting 2 seconds...\n");
    uint64_t end_ticks = current_ticks + (2 * TIMER_HZ);
    while (current_ticks < end_ticks) {
        // Increment ticks manually to simulate ISR
        current_ticks += 100; // Jump ahead for test speed
    }

    /* Clear screen to black and reset cursor for shell */
    terminal_clear();
}

int main() {
    printf("--- Testing Splash Screen Logic ---\n");
    show_splash();
    printf("--- Test Complete ---\n");
    return 0;
}
