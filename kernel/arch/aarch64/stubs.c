/**
 * éterOS - AArch64 Stubs for x86 compatibility
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Implements legacy APIs (VGA, Keyboard, Serial, PCI) using HAL.
 */

#include <hal.h>
#include <types.h>
#include <vga.h>
#include <serial.h>
#include <keyboard.h>
#include <pci.h>
#include <net/e1000.h>
#include <net/dhcp.h>

/* ========================================================================= */
/* VGA / Console Stubs                                                       */
/* ========================================================================= */

void terminal_initialize(boot_info_t* boot_info) {
    (void)boot_info;
    hal_console_init();
}

void terminal_putchar(char c) {
    hal_console_putchar(c);
}

void terminal_write_string(const char* str) {
    hal_console_write(str);
}

void terminal_write_colored(const char* str, vga_color_t fg, vga_color_t bg) {
    (void)fg; (void)bg;
    hal_console_write(str);
}

void terminal_clear(void) {
    hal_console_clear();
}

void terminal_set_color(uint8_t color) {
    (void)color;
}

void terminal_set_cursor(size_t x, size_t y) {
    (void)x; (void)y;
}

/* ========================================================================= */
/* Serial Stubs                                                              */
/* ========================================================================= */

int serial_init(void) {
    /* UART inited by HAL */
    return 0;
}

void serial_putchar(char c) {
    hal_console_putchar(c);
}

void serial_write_string(const char* str) {
    hal_console_write(str);
}

char serial_read(void) {
    return hal_input_getchar();
}

int serial_received(void) {
    return hal_input_available();
}

void serial_irq_handler(void) {
    /* No-op */
}

/* ========================================================================= */
/* Keyboard Stubs                                                            */
/* ========================================================================= */

void keyboard_init(void) {
    hal_input_init();
}

char keyboard_getchar(void) {
    return hal_input_getchar();
}

bool keyboard_has_input(void) {
    return hal_input_available();
}

void keyboard_process_scancode(uint8_t scancode) {
    (void)scancode;
}

/* ========================================================================= */
/* PCI / Network Stubs                                                       */
/* ========================================================================= */

void pci_scan_bus(void) {
    hal_console_write("PCI: Not supported on AArch64 (yet)\n");
}

static uint8_t dummy_mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00};

uint8_t* e1000_get_mac(void) {
    return dummy_mac;
}

int e1000_init(pci_device_t* pci_device) {
    (void)pci_device;
    return -1;
}

void dhcp_discover(void) {
    hal_console_write("DHCP: Not supported (No NIC)\n");
}

void sysmon_run(void) {
    hal_console_write("Sysmon: Not supported on AArch64 (yet)\n");
}
