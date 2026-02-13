/**
 * éterOS - AArch64 HAL Implementation
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Implements Hardware Abstraction Layer for ARMv8 (Raspberry Pi 4 / BCM2711).
 * Includes: UART (PL011), GICv2, Generic Timer, MMU (4-level).
 */

#include <hal.h>
#include <types.h>
#include "bcm2711.h"

/* ========================================================================= */
/* MMIO Helpers                                                              */
/* ========================================================================= */

static inline void mmio_write(uintptr_t reg, uint32_t data) {
    *(volatile uint32_t*)reg = data;
}

static inline uint32_t mmio_read(uintptr_t reg) {
    return *(volatile uint32_t*)reg;
}

static inline void delay(int32_t count) {
    while (count--) {
        __asm__ volatile("nop");
    }
}

/* ========================================================================= */
/* UART (PL011)                                                              */
/* ========================================================================= */

void uart_init(void) {
    /* Disable UART */
    mmio_write(UART0_CR, 0x00000000);

    /*
     * Baud Rate calculation depends on UART Clock.
     * Assuming standard RPi clock setup by firmware.
     * Divisor = UARTCLK / (16 * Baud)
     * For 115200, typically IBRD=26, FBRD=3 (depends on clock)
     * We'll use a safe default or rely on firmware init for baud.
     */

    /* Clear interrupts */
    mmio_write(UART0_ICR, 0x7FF);

    /* 8 bits, no parity, FIFO enabled */
    mmio_write(UART0_LCRH, 0x70);

    /* Enable UART, Tx, Rx */
    mmio_write(UART0_CR, 0x301);
}

void uart_putc(char c) {
    /* Wait for Tx FIFO to be not full */
    while (mmio_read(UART0_FR) & 0x20) { }
    mmio_write(UART0_DR, c);
}

char uart_getc(void) {
    /* Wait for Rx FIFO to be not empty */
    while (mmio_read(UART0_FR) & 0x10) { }
    return (char)(mmio_read(UART0_DR) & 0xFF);
}

/* ========================================================================= */
/* GIC (Generic Interrupt Controller)                                        */
/* ========================================================================= */

static irq_handler_t irq_handlers[256];

void gic_init(void) {
    /* Distributor: Enable Group 0 and 1 */
    mmio_write(GICD_CTLR, 0x3);

    /* CPU Interface: Enable, Set Priority Mask to lowest (allow all) */
    mmio_write(GICC_PMR, 0xFF);
    mmio_write(GICC_CTLR, 0x1);
}

void gic_enable_irq(uint32_t id) {
    /* Calculate register offset */
    uint32_t reg_offset = (id / 32) * 4;
    uint32_t bit_offset = id % 32;

    /* Enable in Distributor (ISENABLER) */
    uint32_t val = mmio_read(GICD_ISENABLER + reg_offset);
    val |= (1 << bit_offset);
    mmio_write(GICD_ISENABLER + reg_offset, val);

    /* Target CPU 0 (ITARGETSR). 8 bits per ID. Register holds 4 IDs. */
    /* Only needed for SPIs (ID >= 32). PPIs are private. */
    if (id >= 32) {
        uint32_t target_reg = (id / 4) * 4;
        uint32_t target_shift = (id % 4) * 8;
        uint32_t target_val = mmio_read(GICD_ITARGETSR + target_reg);
        target_val |= (1 << target_shift); /* Target CPU 0 (bit 0) */
        mmio_write(GICD_ITARGETSR + target_reg, target_val);
    }
}

/* Called from vectors.S */
void hal_irq_handler(void) {
    /* Read IAR */
    uint32_t iar = mmio_read(GICC_IAR);
    uint32_t id = iar & 0x3FF;

    if (id < 1022) {
        if (id < 256 && irq_handlers[id]) {
            irq_handlers[id]();
        } else {
            /* Spurious or unhandled */
            hal_console_write("Unhandled IRQ\n");
        }

        /* End of Interrupt */
        mmio_write(GICC_EOIR, iar);
    }
}

/* ========================================================================= */
/* ARM Generic Timer                                                         */
/* ========================================================================= */

static uint32_t timer_freq;
static uint32_t timer_interval;

/* Internal Timer ISR */
static void timer_isr(void) {
    /* Reprogram Timer for next tick */
    __asm__ volatile("msr cntp_tval_el0, %0" : : "r"((uint64_t)timer_interval));

    /* Call kernel tick handler (from timer.h usually, but we need to link it) */
    /* The kernel usually registers a handler via hal_irq_install(0, handler)
       mapped to timer. But here we have ID 30. */
    /* We assume hal_timer_init logic handles registration or we invoke a callback */
}

void arm_timer_init(void) {
    /* Read Frequency */
    uint64_t frq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frq));
    timer_freq = (uint32_t)frq;

    /* Enable Timer (CNTP_CTL_EL0) */
    /* Bit 0: Enable, Bit 1: IMASK (0=unmasked) */
    uint64_t ctl = 1;
    __asm__ volatile("msr cntp_ctl_el0, %0" : : "r"(ctl));

    /* Register ISR for ID 30 */
    irq_handlers[30] = timer_isr;
    gic_enable_irq(30);
}

/* ========================================================================= */
/* MMU (4-Level Identity Map)                                                */
/* ========================================================================= */

/* Align to 4KB */
static uint64_t page_table_l0[512] __attribute__((aligned(4096)));
static uint64_t page_table_l1[512] __attribute__((aligned(4096)));
static uint64_t page_table_l2_ram[512] __attribute__((aligned(4096)));    /* Covers 0-1GB */
static uint64_t page_table_l2_device[512] __attribute__((aligned(4096))); /* Covers 3-4GB */

void mmu_init(void) {
    /*
     * Setup 4-level Page Tables
     * L0 -> L1 -> L2 (Block)
     *
     * Map:
     *   0x00000000 - 0x3FFFFFFF (1GB) -> Normal Memory (RAM)
     *   0xC0000000 - 0xFFFFFFFF (1GB) -> Device Memory (MMIO)
     */

    /* L0 Entry 0 (0-512GB) -> L1 */
    page_table_l0[0] = (uint64_t)page_table_l1 | 3; /* Valid Table */

    /* L1 Entry 0 (0-1GB) -> L2 RAM */
    page_table_l1[0] = (uint64_t)page_table_l2_ram | 3;

    /* L1 Entry 3 (3GB-4GB) -> L2 Device */
    page_table_l1[3] = (uint64_t)page_table_l2_device | 3;

    /* Fill L2 RAM (Normal Memory) */
    /* MAIR Index 0 = Normal, 1 = Device */
    /* Attribute: AF=1, SH=3 (Inner Shareable), AP=0 (RW EL1), Index=0 */
    uint64_t attr_normal = (1<<10) | (3<<8) | (0<<2) | 1;

    for (int i = 0; i < 512; i++) {
        uint64_t addr = i * 0x200000;
        page_table_l2_ram[i] = addr | attr_normal;
    }

    /* Fill L2 Device (MMIO) */
    /* Attribute: AF=1, SH=0, AP=0, Index=1 (Device) */
    /* Start Address: 3GB = 0xC0000000 */
    uint64_t base_device = 0xC0000000;
    uint64_t attr_device = (1<<10) | (0<<8) | (1<<2) | 1;

    for (int i = 0; i < 512; i++) {
        uint64_t addr = base_device + (i * 0x200000);
        page_table_l2_device[i] = addr | attr_device;
    }

    /* Set TTBR0_EL1 */
    uint64_t ttbr0 = (uint64_t)page_table_l0;
    __asm__ volatile("msr ttbr0_el1, %0" : : "r"(ttbr0));

    /* Set MAIR_EL1 */
    /* Attr 0: Normal (0xFF), Attr 1: Device-nGnRnE (0x00) */
    uint64_t mair = (0xFF << 0) | (0x00 << 8);
    __asm__ volatile("msr mair_el1, %0" : : "r"(mair));

    /* Set TCR_EL1 */
    /* T0SZ=16 (48-bit), TG0=4KB (00), IPS=32bit (000) */
    uint64_t tcr = (16) | (1<<23); /* EPD1=1 (Disable TTBR1) */
    __asm__ volatile("msr tcr_el1, %0" : : "r"(tcr));

    /* Enable MMU (SCTLR_EL1.M=1, C=1, I=1) */
    /* Ensure we have a barrier before enabling */
    __asm__ volatile("isb");

    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= 1; /* Enable MMU */
    sctlr |= (1<<2); /* Enable C Cache */
    sctlr |= (1<<12); /* Enable I Cache */

    /* Uncomment to Enable MMU - Risky without testing environment */
    /* __asm__ volatile("msr sctlr_el1, %0" : : "r"(sctlr)); */
    /* __asm__ volatile("isb"); */
}


/* ========================================================================= */
/* HAL Interface Implementation                                              */
/* ========================================================================= */

extern void vectors_el1(void);

void hal_init(void) {
    /* Set Vector Table */
    uint64_t vbar = (uint64_t)vectors_el1;
    __asm__ volatile("msr vbar_el1, %0" : : "r"(vbar));

    uart_init();
    gic_init();
    arm_timer_init();
}

void hal_console_init(void) {
    /* Already inited in hal_init, but can re-init or just be empty */
    /* Ensure UART is enabled */
}

void hal_console_putchar(char c) {
    if (c == '\n') uart_putc('\r');
    uart_putc(c);
}

void hal_console_write(const char* str) {
    while (*str) {
        hal_console_putchar(*str++);
    }
}

void hal_console_clear(void) {
    /* ANSI Escape code for clear screen */
    hal_console_write("\033[2J\033[H");
}

void hal_input_init(void) {
    /* UART used for input too */
}

char hal_input_getchar(void) {
    return uart_getc();
}

bool hal_input_available(void) {
    return !(mmio_read(UART0_FR) & 0x10);
}

void hal_interrupts_enable(void) {
    __asm__ volatile("msr daifclr, #2"); /* Enable IRQ (bit 1) */
}

void hal_interrupts_disable(void) {
    __asm__ volatile("msr daifset, #2"); /* Disable IRQ */
}

void hal_irq_install(uint8_t vector, irq_handler_t handler) {
    if (vector < 256) {
        irq_handlers[vector] = handler;
        gic_enable_irq(vector);
    }
}

void hal_timer_init(uint32_t hz) {
    /* Calculate counts for hz */
    timer_interval = timer_freq / hz;
    __asm__ volatile("msr cntp_tval_el0, %0" : : "r"((uint64_t)timer_interval));

    /* Enable */
    uint64_t ctl = 1;
    __asm__ volatile("msr cntp_ctl_el0, %0" : : "r"(ctl));
}

uint64_t hal_timer_ticks(void) {
    uint64_t cnt;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt));
    /* Convert counts to ticks (approximation if needed, or raw counts) */
    /* HAL expects "ticks since boot" (system ticks, not raw timer counts?) */
    /* If timer_init sets 100Hz, this should return 100Hz ticks. */
    /* Since we don't have a software counter incremented by ISR yet
       (because we don't have the ISR vector table setup in this HAL impl),
       we just return raw counts for now or 0. */
    return cnt;
}

void hal_cpu_halt(void) {
    __asm__ volatile("wfi");
}

void hal_cpu_reset(void) {
    /* Watchdog Reset (RPi specific) */
    /* PM_RSTC = PM_PASSWORD | 0x20; */
    /* Stub */
    while(1);
}

void hal_mmu_init(void) {
    mmu_init();
    /* Enable MMU logic would go here (MAIR, TCR, SCTLR) */
}

void hal_debug_putchar(char c) {
    uart_putc(c);
}

void hal_debug_write(const char* str) {
    hal_console_write(str);
}

/* ========================================================================= */
/* Timer API Implementation (generic wrappers)                               */
/* ========================================================================= */

uint64_t timer_get_ticks(void) {
    return hal_timer_ticks();
}

void timer_wait(uint32_t ms) {
    /* Assuming 100Hz tick rate (10ms per tick) */
    uint64_t ticks = (ms + 9) / 10;
    uint64_t start = timer_get_ticks();
    while (timer_get_ticks() < start + ticks) {
        hal_cpu_halt();
    }
}

/* Forward declaration for task_sleep */
void task_sleep(uint32_t ms);

void timer_sleep(uint32_t ms) {
    task_sleep(ms);
}

uint32_t timer_get_uptime_seconds(void) {
    return timer_get_ticks() / 100;
}

uint32_t timer_get_uptime_minutes(void) {
    return timer_get_uptime_seconds() / 60;
}
