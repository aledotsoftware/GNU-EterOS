#include "shell_internal.h"
#include "../../include/mm.h"
#include "../../include/pmm.h"
#include "../../include/pci.h"
#include "../../include/string.h"

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
