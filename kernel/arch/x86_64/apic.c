#include <apic.h>
#include <acpi.h>
#include <serial.h>
#include <io.h>
#include <timer.h>
#include <string.h>
#include <task.h>
#include <vmm.h>

static uint64_t lapic_base = 0;

static uint32_t lapic_read(uint32_t reg) {
    return *((volatile uint32_t*)(lapic_base + reg));
}

static void lapic_write(uint32_t reg, uint32_t data) {
    *((volatile uint32_t*)(lapic_base + reg)) = data;
}

void lapic_init(void) {
    lapic_base = (uint64_t)acpi_get_lapic_addr();
    
    /* El LAPIC ya esta mapeado por el BSP/Bootloader (Identitiy Map 4GB).
       Evitamos vmm_map_page aqui para prevenir deadlocks de TLB shootdown durante el boot de APs. */
    
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
    /* For Fixed delivery mode (Edge), we just send the vector. 
       ICR_ASSERT is usually only for Level-triggered INIT/Startup sequences. */
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

/* ========================================================================= */
/* LAPIC Timer Support                                                       */
/* ========================================================================= */

void lapic_timer_handler(void) {
    lapic_eoi();
    schedule();
}

static uint32_t calibrated_ticks_per_ms = 0;

void lapic_timer_init(uint32_t frequency) {
    if (!lapic_base) return;

    uint32_t ticks_per_target;

    /* Si ya hemos calibrado en otro CPU, reutilizamos el valor para evitar
       esperas redundantes (y posibles deadlocks si los IRQs estan enmascarados). */
    if (calibrated_ticks_per_ms > 0) {
        uint64_t ticks_per_second = (uint64_t)calibrated_ticks_per_ms * 1000;
        ticks_per_target = (uint32_t)(ticks_per_second / frequency);
    } else {
        /* 1. Set Divide Configuration Register to /16 */
        lapic_write(LAPIC_TDCR, 0x03);

        /* 2. Prepare for calibration */
        /* Wait for the start of a new tick to align measurement */
        uint64_t current_tick = timer_get_ticks();
        while (timer_get_ticks() == current_tick) {
            __asm__ volatile("pause");
        }

        /* Now we are at the beginning of a tick interval */
        /* Set Initial Count to Max to start downcounting */
        lapic_write(LAPIC_TICR, 0xFFFFFFFF);

        /* Wait for ONE full tick (1ms because TIMER_HZ=1000) */
        current_tick = timer_get_ticks();
        while (timer_get_ticks() == current_tick) {
            __asm__ volatile("pause");
        }

        /* 3. Stop Timer and Read Elapsed */
        uint32_t current_count = lapic_read(LAPIC_TCCR);
        calibrated_ticks_per_ms = 0xFFFFFFFF - current_count;

        uint64_t ticks_per_second = (uint64_t)calibrated_ticks_per_ms * 1000;
        ticks_per_target = (uint32_t)(ticks_per_second / frequency);
    }

    if (ticks_per_target == 0) ticks_per_target = 1000;

    /* 5. Setup Timer in Periodic Mode */
    /* Vector 0x40, Periodic (0x20000) */
    lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_VECTOR | 0x20000);

    /* 6. Set Divide Config again (just to be safe) */
    lapic_write(LAPIC_TDCR, 0x03);

    /* 7. Start Timer */
    lapic_write(LAPIC_TICR, ticks_per_target);
}
