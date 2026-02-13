/* Define Host Test Mode at the very top */
#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

/* Mock Headers */
#include "../include/types.h"
#include "../include/io.h"
#include "../include/mm.h"
#include "../include/string.h"
#include "../include/vga.h"
#include "../include/serial.h"
#include "../include/pci.h"
#include "../include/timer.h"

/* Mock Variables */
uint64_t mock_ticks = 0;
uint8_t* mock_mmio_base = NULL;

/* Mock Implementations */

/* Timer */
void timer_init(void) {}
void timer_tick(void) { mock_ticks++; }
uint64_t timer_get_ticks(void) {
    /* Every time this is called, we increment ticks to simulate time passing */
    return mock_ticks++;
}
uint32_t timer_get_uptime_seconds(void) { return mock_ticks / 100; }
uint32_t timer_get_uptime_minutes(void) { return mock_ticks / 6000; }

/* IO */
void outb(uint16_t port, uint8_t value) { (void)port; (void)value; }
void outw(uint16_t port, uint16_t value) { (void)port; (void)value; }
void outl(uint16_t port, uint32_t value) { (void)port; (void)value; }
uint8_t inb(uint16_t port) { (void)port; return 0; }
uint16_t inw(uint16_t port) { (void)port; return 0; }
uint32_t inl(uint16_t port) { (void)port; return 0; }
void io_wait(void) {}

/* Memory */
void mm_init(boot_info_t* boot_info) { (void)boot_info; }
void* kmalloc(size_t size) { return calloc(1, size); }
void* kcalloc(size_t num, size_t size) { return calloc(num, size); }
void kfree(void* ptr) { free(ptr); }
size_t mm_get_total_memory(void) { return 0; }
size_t mm_get_used_memory(void) { return 0; }

/* PCI */
int pci_find_device(uint16_t vendor_id, uint16_t device_id, pci_device_t* dev) {
    (void)vendor_id; (void)device_id;
    dev->bus = 0; dev->slot = 3; dev->function = 0;
    return 1; /* Found */
}
uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    (void)bus; (void)slot; (void)func;
    if (offset == PCI_OFFSET_COMMAND) return 0;
    if (offset == PCI_OFFSET_BAR0) return 0x1000;
    if (offset == PCI_OFFSET_BAR0 + 2) return 0;
    return 0;
}
void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    (void)bus; (void)slot; (void)func; (void)offset; (void)value;
}
uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    (void)bus; (void)slot; (void)func; (void)offset;
    return 0;
}
void pci_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    (void)bus; (void)slot; (void)func; (void)offset; (void)value;
}

/* VGA */
void terminal_initialize(boot_info_t* boot_info) { (void)boot_info; }
void terminal_write_string(const char* data) { printf("%s", data); }
void terminal_write_colored(const char* data, vga_color_t text_color, vga_color_t back_color) {
    (void)text_color; (void)back_color;
    printf("%s", data);
}
void terminal_scroll(void) {}

/* Serial */
int serial_init(void) { return 0; }
void serial_write_string(const char* data) { printf("%s", data); }
void serial_putchar(char c) { putchar(c); }

/* Rename functions */
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

/* Include Source Code directly */
#include "../kernel/string.c"
#include "../kernel/drivers/net/e1000.c"

void handle_alarm(int sig) {
    (void)sig;
    fprintf(stderr, "\nFATAL: Test timed out! Infinite loop detected.\n");
    exit(1);
}

int main() {
    printf("Running E1000 Timeout Test...\n");

    /* Set an alarm to kill the test if it hangs (simulating infinite loop) */
    signal(SIGALRM, handle_alarm);
    alarm(3); /* 3 seconds real time should be plenty */

    /* Allocate mock MMIO space */
    mock_mmio_base = (uint8_t*)calloc(1, 0x10000);
    mmio_base = mock_mmio_base;
    e1000_active = 1;

    e1000_init_tx();
    e1000_init_rx();

    /* Prepare a packet */
    uint8_t packet[64];
    memset(packet, 0xAA, sizeof(packet));

    printf("Sending packet (expecting timeout logic to break loop)...\n");

    /* Reset mock ticks */
    mock_ticks = 0;

    /* In the vulnerable code, this will hang forever (until alarm kills it).
       In the fixed code, this will return after mock ticks increase. */
    e1000_send_packet(packet, sizeof(packet));

    printf("\nTest Passed: Function returned!\n");

    free(mock_mmio_base);
    return 0;
}
