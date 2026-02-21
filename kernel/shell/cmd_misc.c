#include "shell_internal.h"
#include "../../include/mm.h"
#include "../../include/pmm.h"
#include "../../include/pci.h"
#include "../../include/string.h"
#include <gfx/window.h>
#include <hal.h>

void cmd_echo(const char* args) {
    terminal_write_string(args);
    terminal_write_string("\n");
}

void cmd_mem(const char* args) {
    (void)args;
    terminal_write_string("\n");
    terminal_write_colored("  Mapa de Memoria del Sistema:\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_string("    0x00000 - 0x07BFF  IVT / BDA\n");
    terminal_write_string("    0x07C00 - 0x07DFF  Stage 1 (MBR)\n");
    terminal_write_string("    0x07E00 - 0x09DFF  Stage 2\n");
    terminal_write_string("    0x10000 - 0x1FFFF  Ether-Core (Kernel)\n");
    terminal_write_string("    0x70000 - 0x72FFF  Page Tables\n");
    terminal_write_string("    0x90000            Stack Top\n");
    terminal_write_string("    0xB8000 - 0xBFFFF  VGA Buffer\n");
    terminal_write_string("    0x400000 - 0xC00000 Kernel Heap (8 MB)\n");

    terminal_write_string("\n");
    terminal_write_colored("  Estado del Heap (Kernel):\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

    char buffer[32];
    size_t total = mm_get_total_memory();
    size_t used = mm_get_used_memory();

    terminal_write_string("    Total:  ");
    itoa_s((int64_t)total, buffer, sizeof(buffer), 10);
    terminal_write_colored(buffer, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string(" bytes\n");

    terminal_write_string("    Usado:  ");
    itoa_s((int64_t)used, buffer, sizeof(buffer), 10);
    terminal_write_colored(buffer, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string(" bytes\n");

    terminal_write_string("    Libre:  ");
    itoa_s((int64_t)(total - used), buffer, sizeof(buffer), 10);
    terminal_write_colored(buffer, VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    terminal_write_string(" bytes\n");

    terminal_write_string("\n");
}

void cmd_lspci(const char* args) {
    (void)args;
    pci_scan_bus();
}

void cmd_clear(const char* args) {
    (void)args;
    terminal_initialize(NULL);
}

void cmd_test_compositor(const char* args) {
    (void)args;
    terminal_write_string("Testing Basic Compositor...\n");

    compositor_init();

    /* Create Window 1 (Red) */
    window_t* w1 = window_create(50, 50, 200, 150, WIN_VISIBLE);
    if (!w1) {
        terminal_write_string("Failed to create Window 1\n");
        return;
    }

    /* Fill Buffer Red */
    for (int i = 0; i < 200*150; i++) w1->buffer[i] = 0xFFFF0000;

    compositor_add_window(w1);

    /* Create Window 2 (Green, overlapping) */
    window_t* w2 = window_create(150, 100, 200, 150, WIN_VISIBLE);
    if (!w2) {
        terminal_write_string("Failed to create Window 2\n");
        return;
    }

    /* Fill Buffer Green */
    for (int i = 0; i < 200*150; i++) w2->buffer[i] = 0xFF00FF00;

    compositor_add_window(w2); /* w2 is now on top */

    /* Create Window 3 (Blue with transparency hole) */
    window_t* w3 = window_create(100, 200, 100, 100, WIN_VISIBLE);
    if (!w3) {
        terminal_write_string("Failed to create Window 3\n");
        return;
    }

    /* Fill Buffer Blue, but center transparent */
    for (int y = 0; y < 100; y++) {
        for (int x = 0; x < 100; x++) {
            if (x > 25 && x < 75 && y > 25 && y < 75) {
                w3->buffer[y*100 + x] = 0x00000000; /* Transparent */
            } else {
                w3->buffer[y*100 + x] = 0xFF0000FF;
            }
        }
    }

    compositor_add_window(w3);

    terminal_write_string("Rendering...\n");
    compositor_render();

    terminal_write_string("Check screen. Windows should be visible.\n");
}
