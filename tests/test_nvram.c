#include <stdlib.h>
#include <assert.h>
#ifndef assert
#define assert(x) do { if (!(x)) { printf("ASSERT FAILED: %s\n", #x); exit(1); } } while(0)
#endif
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

// Mock IO
uint8_t cmos_registers[128];
uint8_t current_index = 0;

void outb(uint16_t port, uint8_t value) {
    if (port == 0x70) {
        current_index = value & 0x7F;
    } else if (port == 0x71) {
        cmos_registers[current_index] = value;
    }
}

uint8_t inb(uint16_t port) {
    if (port == 0x71) {
        return cmos_registers[current_index];
    }
    return 0;
}

// Stubs for io.h functions
void outw(uint16_t port, uint16_t value) { (void)port; (void)value; }
void outl(uint16_t port, uint32_t value) { (void)port; (void)value; }
uint16_t inw(uint16_t port) { (void)port; return 0; }
uint32_t inl(uint16_t port) { (void)port; return 0; }
void io_wait(void) {}
void cpu_halt(void) {}
int interrupts_enabled(void) { return 0; }
uint64_t irq_save(void) { return 0; }
void irq_restore(uint64_t flags) { (void)flags; }

// Include source file to test
#include "../kernel/arch/x86_64/boot/nvram.c"

#ifdef __ETEROS_HOST_TEST__
void* eteros_memset(void* dest, int c, size_t n) {
    return __builtin_memset(dest, c, n);
}
#endif

void test_nvram_read_write() {
    printf("Testing NVRAM read/write...\n");
    memset(cmos_registers, 0, sizeof(cmos_registers));

    nvram_write(0x10, 0xAB);
    uint8_t val = nvram_read(0x10);
    assert(val == 0xAB);
    assert(cmos_registers[0x10] == 0xAB);

    printf("PASS\n");
}

void test_boot_partition_flag() {
    printf("Testing Boot Partition Flag...\n");
    memset(cmos_registers, 0, sizeof(cmos_registers));

    nvram_set_boot_partition(1);
    assert(nvram_get_boot_partition() == 1);
    assert(cmos_registers[NVRAM_BOOT_PARTITION_REG] == 1);

    nvram_set_boot_partition(2);
    assert(nvram_get_boot_partition() == 2);

    printf("PASS\n");
}

int main() {
    test_nvram_read_write();
    test_boot_partition_flag();
    return 0;
}
