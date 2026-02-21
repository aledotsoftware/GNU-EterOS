#ifndef ETEROS_APIC_H
#define ETEROS_APIC_H

#include <types.h>

/* Local APIC Registers (offsets from Base) */
#define LAPIC_ID            0x20
#define LAPIC_VER           0x30
#define LAPIC_TPR           0x80
#define LAPIC_EOI           0xB0
#define LAPIC_LDR           0xD0
#define LAPIC_DFR           0xE0
#define LAPIC_SVR           0xF0
#define LAPIC_ESR           0x280
#define LAPIC_ICR_LOW       0x300
#define LAPIC_ICR_HIGH      0x310
#define LAPIC_LVT_TIMER     0x320
#define LAPIC_LVT_THERMAL   0x330
#define LAPIC_LVT_PERF      0x340
#define LAPIC_LVT_LINT0     0x350
#define LAPIC_LVT_LINT1     0x360
#define LAPIC_LVT_ERROR     0x370
#define LAPIC_TICR          0x380
#define LAPIC_TCCR          0x390
#define LAPIC_TDCR          0x3E0

/* ICR Delivery Modes */
#define ICR_FIXED           0x000
#define ICR_LOWEST          0x100
#define ICR_SMI             0x200
#define ICR_NMI             0x400
#define ICR_INIT            0x500
#define ICR_STARTUP         0x600

/* ICR Destinations */
#define ICR_PHYSICAL        0x0000
#define ICR_LOGICAL         0x0800

/* ICR Delivery Status */
#define ICR_IDLE            0x0000
#define ICR_SEND_PENDING    0x1000

/* ICR Levels */
#define ICR_DEASSERT        0x0000
#define ICR_ASSERT          0x4000

/* ICR Triggers */
#define ICR_EDGE            0x0000
#define ICR_LEVEL           0x8000

/* ICR Destination Shorthands */
#define ICR_NO_SHORTHAND    0x00000
#define ICR_SELF            0x40000
#define ICR_ALL_INCL_SELF   0x80000
#define ICR_ALL_EXCL_SELF   0xC0000

/* LAPIC Timer Vector */
#define LAPIC_TIMER_VECTOR  0x40

void lapic_init(void);
void lapic_eoi(void);
void lapic_send_ipi(uint32_t apic_id, uint32_t vector);
void lapic_send_init(uint32_t apic_id);
void lapic_send_startup(uint32_t apic_id, uint8_t vector);
uint32_t lapic_get_id(void);

void lapic_timer_init(uint32_t frequency);
void lapic_timer_handler(void);

#endif /* ETEROS_APIC_H */
