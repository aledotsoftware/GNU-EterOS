#include <apic.h>
#include <acpi.h>
#include <serial.h>
#include <io.h>
#include <timer.h>

static uint64_t lapic_base = 0;

static uint32_t lapic_read(uint32_t reg) {
    return *((volatile uint32_t*)(lapic_base + reg));
}

static void lapic_write(uint32_t reg, uint32_t data) {
    *((volatile uint32_t*)(lapic_base + reg)) = data;
}

void lapic_init(void) {
    lapic_base = (uint64_t)acpi_get_lapic_addr();
    
    char buf[64];
    serial_write_string("[APIC] Initializing Local APIC at 0x");
    utoa_hex_s(lapic_base, buf, 64);
    serial_write_string(buf);
    serial_write_string("\n");

    /* Software Enable APIC + Spurious Vector 0xFF */
    lapic_write(LAPIC_SVR, lapic_read(LAPIC_SVR) | 0x1FF);
    
    /* Set TPR to 0 to allow all interrupts */
    lapic_write(LAPIC_TPR, 0);
}

void lapic_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

uint32_t lapic_get_id(void) {
    if (!lapic_base) return 0;
    return lapic_read(LAPIC_ID) >> 24;
}

void lapic_send_ipi(uint32_t apic_id, uint32_t vector) {
    /* Wait for previous IPI to clear */
    while (lapic_read(LAPIC_ICR_LOW) & ICR_SEND_PENDING);
    
    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_ICR_LOW, vector);
}

void lapic_send_init(uint32_t apic_id) {
    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    /* Mode INIT (0x500) + Assert (0x4000) */
    lapic_write(LAPIC_ICR_LOW, ICR_INIT | ICR_PHYSICAL | ICR_ASSERT | ICR_EDGE | ICR_NO_SHORTHAND);
    
    /* BIOS requires some delay after INIT */
    timer_wait(10);
}

void lapic_send_startup(uint32_t apic_id, uint8_t vector) {
    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    /* Mode STARTUP (0x600) */
    lapic_write(LAPIC_ICR_LOW, ICR_STARTUP | ICR_PHYSICAL | ICR_ASSERT | ICR_EDGE | ICR_NO_SHORTHAND | vector);
    
    timer_wait(1);
}
