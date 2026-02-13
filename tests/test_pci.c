/**
 * Test Suite for PCI Driver
 *
 * Compile with:
 * gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_pci.c kernel/string.c -o tests/test_pci
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

/* Define Host Test Mode */
#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

/* Mock Definitions for IO */
static uint32_t io_pci_address = 0;
static uint32_t io_pci_data_out = 0;

/* Mock PCI Configuration Space */
/* Simple struct to hold a single device we want to test */
static struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subclass;
    uint16_t header_type;
} mock_device = { 0, 1, 0, 0x8086, 0x100E, 0x0200, 0x00 };

/* Flag to enable sparse mock mode (returns 0xFFFF for everything except mock_device) */
static int mock_sparse_mode = 0;

/* IO Mocks */
void outb(uint16_t port, uint8_t value) {
    (void)port; (void)value;
}

void outw(uint16_t port, uint16_t value) {
    (void)port; (void)value;
}

void outl(uint16_t port, uint32_t value) {
    if (port == 0xCF8) {
        io_pci_address = value;
    } else if (port == 0xCFC) {
        io_pci_data_out = value;
    }
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
    if (port == 0xCFC) {
        /* Decode address */
        uint32_t addr = io_pci_address;
        uint8_t bus = (addr >> 16) & 0xFF;
        uint8_t slot = (addr >> 11) & 0x1F;
        uint8_t func = (addr >> 8) & 0x07;
        uint8_t offset = (addr & 0xFC); /* Register (offset aligned to 4) */

        /* If sparse mode is on, check against mock device */
        if (mock_sparse_mode) {
            if (bus == mock_device.bus && slot == mock_device.slot && func == mock_device.func) {
                /* Return data for mock device */
                /* For simplicity, we just return Vendor/Device ID for offset 0 */
                /* And other fields if requested */
                if (offset == 0x00) {
                    /* Vendor ID (low) + Device ID (high) */
                    return (uint32_t)(mock_device.vendor_id | (mock_device.device_id << 16));
                } else if (offset == 0x08) {
                    /* Revision (low) + Prog IF + Subclass + Class (high) */
                    /* Returning generic class/subclass from mock_device */
                    /* subclass field in mock_device is actually just 16 bits for subclass?
                       No, let's assume it's Class code (high) + Subclass (low) */
                    return (uint32_t)(0x00 | (0x00 << 8) | (mock_device.subclass << 16));
                } else if (offset == 0x0C) {
                    /* Cache Line Size + Latency Timer + Header Type + BIST */
                    return (uint32_t)(0x00 | (0x00 << 8) | (mock_device.header_type << 16) | (0x00 << 24));
                }
                return 0;
            } else {
                return 0xFFFFFFFF; /* Device not found */
            }
        }

        /* For low-level tests, we return a predictable pattern based on offset */
        /* Pattern: 0xAABBCCDD */
        return 0xAABBCCDD;
    }
    return 0;
}

void io_wait(void) {}
void cpu_halt(void) {}
int interrupts_enabled(void) { return 0; }
uint64_t irq_save(void) { return 0; }
void irq_restore(uint64_t flags) { (void)flags; }


/* VGA Mocks */
#include "../include/vga.h"
void terminal_initialize(boot_info_t* boot_info) { (void)boot_info; }
void terminal_set_color(uint8_t color) { (void)color; }
void terminal_putchar(char c) { putchar(c); }
void terminal_write_string(const char* str) { printf("%s", str); }
void terminal_write_colored(const char* str, vga_color_t fg, vga_color_t bg) {
    (void)fg; (void)bg;
    printf("%s", str);
}
void terminal_clear(void) {}
void terminal_set_cursor(size_t x, size_t y) { (void)x; (void)y; }
void terminal_scroll(void) {}

/* Serial Mocks */
#include "../include/serial.h"
int serial_init(void) { return 0; }
void serial_write_string(const char* str) {
    /* Optionally print to stderr or ignore */
    /* fprintf(stderr, "%s", str); */
    (void)str;
}
void serial_putchar(char c) { (void)c; }
char serial_read(void) { return 0; }
int serial_received(void) { return 0; }


/* Include the file under test */
#include "../kernel/drivers/pci/pci.c"


/* Test Main */
int main() {
    printf("Running PCI Driver Tests...\n");

    /* Test 1: Low-Level Access (pci_read_word) - Aligned */
    {
        printf("Test 1: Aligned Read (Offset 0)...\n");
        io_pci_address = 0;
        /* Offset 0 -> Should read lower 16 bits of 0xAABBCCDD -> 0xCCDD */
        uint16_t val = pci_read_word(0, 0, 0, 0);

        /* Check address written to CONFIG_ADDRESS */
        /* Enable bit (31) should be set. Offset 0. */
        assert(io_pci_address == (0x80000000 | 0));
        assert(val == 0xCCDD);

        printf("Test 1: Aligned Read (Offset 2)...\n");
        /* Offset 2 -> Should read upper 16 bits of 0xAABBCCDD -> 0xAABB */
        val = pci_read_word(0, 0, 0, 2);

        /* Address should still be offset 0 (aligned to 4 bytes) -> 0x80000000 */
        assert((io_pci_address & 0xFC) == 0);
        assert(val == 0xAABB);
    }

    /* Test 2: Low-Level Access - Unaligned (Canary Test) */
    {
        printf("Test 2: Unaligned Read (Offset 1)...\n");
        /* Current implementation behavior:
           offset 1 & 2 is 0. Shift is 0.
           Reads lower 16 bits (same as offset 0).
        */
        uint16_t val = pci_read_word(0, 0, 0, 1);

        assert((io_pci_address & 0xFC) == 0);
        assert(val == 0xCCDD); /* Canary: Confirming current odd behavior */

        printf("Test 2: Unaligned Read (Offset 3)...\n");
        /* Current implementation behavior:
           offset 3 & 2 is 2. Shift is 16.
           Reads upper 16 bits (same as offset 2).
        */
        val = pci_read_word(0, 0, 0, 3);

        assert((io_pci_address & 0xFC) == 0);
        assert(val == 0xAABB); /* Canary */
    }

    /* Test 3: Low-Level Write */
    {
        printf("Test 3: pci_write_word...\n");
        /* Mock writes */
        /* To test write, we need to inspect what was written to 0xCFC (io_pci_data_out) */

        /* Write to offset 0 (lower word) */
        /* Implementation does a Read-Modify-Write.
           It reads the current value (which our mock returns as 0xAABBCCDD).
           It modifies the lower word to 0x1234.
           It writes back 0xAABB1234.
        */
        pci_write_word(0, 0, 0, 0, 0x1234);
        assert(io_pci_data_out == 0xAABB1234);

        /* Write to offset 2 (upper word) */
        /* It reads 0xAABBCCDD.
           Modifies upper word to 0x5678.
           Writes back 0x5678CCDD.
        */
        pci_write_word(0, 0, 0, 2, 0x5678);
        assert(io_pci_data_out == 0x5678CCDD);
    }

    /* Test 4: High-Level Scan (pci_find_device) */
    {
        printf("Test 4: Sparse Device Scan...\n");
        mock_sparse_mode = 1;

        /* Find existing device */
        pci_device_t dev;
        int found = pci_find_device(0x8086, 0x100E, &dev);

        assert(found == 1);
        assert(dev.bus == 0);
        assert(dev.slot == 1);
        assert(dev.vendor_id == 0x8086);
        assert(dev.device_id == 0x100E);

        /* Search for non-existing device */
        found = pci_find_device(0x9999, 0x9999, &dev);
        assert(found == 0);

        mock_sparse_mode = 0;
    }

    /* Test 5: pci_scan_bus (Visual/Integration) */
    {
        printf("Test 5: pci_scan_bus output...\n");
        /* We just run it to ensure no crashes and basic coverage of the loop */
        /* Output is printed to stdout via mock */
        mock_sparse_mode = 1;
        pci_scan_bus();
        mock_sparse_mode = 0;
    }

    printf("All PCI tests passed!\n");
    return 0;
}
