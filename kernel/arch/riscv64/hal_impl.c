/**
 * éterOS — RISC-V 64-bit HAL
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Implementación de la HAL para RISC-V 64-bit (S-mode).
 * Target: QEMU virt (SiFive U/HiFive compatible layout).
 */

#include <hal.h>
#include "riscv.h"
#include <string.h>

/* ========================================================================= */
/* Global State                                                              */
/* ========================================================================= */

static volatile uint64_t g_ticks = 0;
static uint64_t g_timer_interval = 0;
static irq_handler_t g_irq_handlers[64]; /* Local/Exception Handlers */
static irq_handler_t g_plic_handlers[128]; /* PLIC Handlers */

/* UART Registers (Offset from Base) */
#define UART_RBR    0x00 /* Receiver Buffer (read) */
#define UART_THR    0x00 /* Transmitter Holding (write) */
#define UART_IER    0x01 /* Interrupt Enable */
#define UART_IIR    0x02 /* Interrupt Identity */
#define UART_FCR    0x02 /* FIFO Control */
#define UART_LCR    0x03 /* Line Control */
#define UART_MCR    0x04 /* Modem Control */
#define UART_LSR    0x05 /* Line Status */

#define UART_LSR_RX_READY   0x01
#define UART_LSR_TX_EMPTY   0x20

/* ========================================================================= */
/* SBI Wrapper                                                               */
/* ========================================================================= */

static struct sbiret sbi_call(long eid, long fid, long a0, long a1, long a2, long a3, long a4, long a5) {
    struct sbiret ret;
    register long a0_reg __asm__("a0") = a0;
    register long a1_reg __asm__("a1") = a1;
    register long a2_reg __asm__("a2") = a2;
    register long a3_reg __asm__("a3") = a3;
    register long a4_reg __asm__("a4") = a4;
    register long a5_reg __asm__("a5") = a5;
    register long a6_reg __asm__("a6") = fid;
    register long a7_reg __asm__("a7") = eid;

    __asm__ volatile (
        "ecall"
        : "+r"(a0_reg), "+r"(a1_reg)
        : "r"(a2_reg), "r"(a3_reg), "r"(a4_reg), "r"(a5_reg), "r"(a6_reg), "r"(a7_reg)
        : "memory"
    );

    ret.error = a0_reg;
    ret.value = a1_reg;
    return ret;
}

/* ========================================================================= */
/* Console (UART)                                                            */
/* ========================================================================= */

void hal_console_init(void) {
    /*
     * Disable interrupts, Set 8N1 (8 bits, No parity, 1 stop bit),
     * Enable FIFO.
     */
    mmio_write8(RISCV_UART0_BASE + UART_IER, 0x00); /* Disable interrupts */
    mmio_write8(RISCV_UART0_BASE + UART_LCR, 0x03); /* 8 bits, no parity, one stop bit */
    mmio_write8(RISCV_UART0_BASE + UART_FCR, 0x01); /* Enable FIFO */
    mmio_write8(RISCV_UART0_BASE + UART_IER, 0x01); /* Enable RX interrupts */
}

void hal_console_putchar(char c) {
    /* Wait for TX holding register to be empty */
    while ((mmio_read8(RISCV_UART0_BASE + UART_LSR) & UART_LSR_TX_EMPTY) == 0);
    mmio_write8(RISCV_UART0_BASE + UART_THR, c);
}

void hal_console_write(const char* str) {
    while (*str) {
        hal_console_putchar(*str++);
    }
}

void hal_console_clear(void) {
    /* ANSI escape code to clear screen */
    hal_console_write("\033[2J\033[1;1H");
}

/* ========================================================================= */
/* Input                                                                     */
/* ========================================================================= */

void hal_input_init(void) {
    /* Already initialized in console_init (IER=1) */
}

char hal_input_getchar(void) {
    /* Blocking read */
    while ((mmio_read8(RISCV_UART0_BASE + UART_LSR) & UART_LSR_RX_READY) == 0) {
        /* Wait */
    }
    return mmio_read8(RISCV_UART0_BASE + UART_RBR);
}

bool hal_input_available(void) {
    return (mmio_read8(RISCV_UART0_BASE + UART_LSR) & UART_LSR_RX_READY) != 0;
}

/* ========================================================================= */
/* Timer                                                                     */
/* ========================================================================= */

/* Read 'time' CSR (User/Supervisor/Machine shared RO shadow) */
static inline uint64_t get_time(void) {
    unsigned long t;
    __asm__ volatile ("rdtime %0" : "=r"(t));
    return t;
}

void hal_timer_init(uint32_t hz) {
    /* Calculate interval based on generic 10MHz timebase (typical for QEMU virt) */
    uint64_t timebase = 10000000;
    g_timer_interval = timebase / hz;

    /* Enable Timer Interrupt in SIE */
    csrs(sie, SIE_STIE);

    /* Set first timeout */
    uint64_t next_event = get_time() + g_timer_interval;
    sbi_call(SBI_EID_TIMER, 0, next_event, 0, 0, 0, 0, 0);
}

uint64_t hal_timer_ticks(void) {
    return g_ticks;
}

/* ========================================================================= */
/* Interrupt Handling                                                        */
/* ========================================================================= */

void hal_interrupts_enable(void) {
    csrs(sstatus, SSTATUS_SIE);
}

void hal_interrupts_disable(void) {
    csrc(sstatus, SSTATUS_SIE);
}

void hal_irq_install(uint8_t vector, irq_handler_t handler) {
    if (vector < 16) {
        if (vector < 64) g_irq_handlers[vector] = handler;
    } else {
        uint8_t plic_irq = vector - 16;
        if (plic_irq < 128) {
            g_plic_handlers[plic_irq] = handler;

            /* Enable in PLIC for Context 1 (S-mode) */
            uint32_t enable_off = PLIC_ENABLE + (plic_irq / 32) * 4;
            uint32_t enable_bit = 1 << (plic_irq % 32);
            uint32_t current = mmio_read32(RISCV_PLIC_BASE + enable_off);
            mmio_write32(RISCV_PLIC_BASE + enable_off, current | enable_bit);

            /* Set Priority to 1 (lowest active) */
            mmio_write32(RISCV_PLIC_BASE + PLIC_PRIORITY + (plic_irq * 4), 1);
        }
    }
}

/* Trap Handler (C dispatcher) */
#ifdef __riscv
__attribute__((interrupt("supervisor")))
#else
__attribute__((interrupt))
#endif
void trap_handler(void) {
    uint64_t scause = csrr(scause);

    /* Check if it's an interrupt (MSB set) */
    if (scause & SCAUSE_INTR) {
        uint64_t code = scause & SCAUSE_CODE_MASK;

        if (code == IRQ_S_TIMER) {
            /* Timer Interrupt */
            g_ticks++;

            /* Schedule next timer event */
            uint64_t next_event = get_time() + g_timer_interval;
            sbi_call(SBI_EID_TIMER, 0, next_event, 0, 0, 0, 0, 0);
        }
        else if (code == IRQ_S_EXT) {
            /* External Interrupt (PLIC) */
            uint32_t irq = mmio_read32(RISCV_PLIC_BASE + PLIC_CLAIM);

            if (irq > 0) {
                /* Execute handler */
                if (irq < 128 && g_plic_handlers[irq]) {
                    g_plic_handlers[irq]();
                }

                /* Complete Interrupt */
                mmio_write32(RISCV_PLIC_BASE + PLIC_CLAIM, irq);
            }
        }
        else if (code == IRQ_S_SOFT) {
            /* Software Interrupt */
            csrc(sip, SIE_SSIE); /* Clear pending */
        }
    } else {
        /* Exception (Sync) */
        hal_cpu_halt();
    }
}

/* ========================================================================= */
/* Initialization                                                            */
/* ========================================================================= */

void hal_init(void) {
    /* 1. Setup Trap Vector */
    csrw(stvec, (uint64_t)trap_handler);

    /* 2. Init PLIC */
    /* Set Threshold to 0 (allow all priorities > 0) */
    mmio_write32(RISCV_PLIC_BASE + PLIC_THRESHOLD, 0);
}

/* ========================================================================= */
/* CPU                                                                       */
/* ========================================================================= */

void hal_cpu_halt(void) {
    __asm__ volatile ("wfi");
}

void hal_cpu_enable_interrupts_and_halt(void) {
    hal_interrupts_enable();
    hal_cpu_halt();
}

void hal_cpu_reset(void) {
    /* SBI Shutdown (EID 0x08) */
    sbi_call(SBI_EID_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0);
    while (1);
}

/* ========================================================================= */
/* MMU                                                                       */
/* ========================================================================= */

/* Simple page table setup for identity mapping 1GB at 0x80000000 */
static uint64_t root_pt[512] __attribute__((aligned(4096)));

void hal_mmu_init(void) {
#if ETEROS_HAS_MMU
    /* Zero out table */
    memset(root_pt, 0, sizeof(root_pt));

    /* Map 0x00000000 - 0x40000000 (Low 1GB, includes MMIO 0x10000000, 0x0C000000) */
    uint64_t flags_mmio = PTE_V | PTE_R | PTE_W | PTE_A | PTE_D;
    root_pt[0] = (0 << 10) | flags_mmio; /* PPN 0 -> Phys 0x0 */

    /* Map 0x80000000 - 0xC0000000 (Kernel RAM 1GB) */
    uint64_t flags_kernel = PTE_V | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D | PTE_G;
    root_pt[2] = ((0x80000000UL >> 12) << 10) | flags_kernel;

    /* Set SATP */
    uint64_t satp_val = SATP_MODE_SV39 | ((uint64_t)root_pt >> 12);

    csrw(satp, satp_val);
    __asm__ volatile("sfence.vma");
#endif
}

/* ========================================================================= */
/* Debug                                                                     */
/* ========================================================================= */

void hal_debug_putchar(char c) {
    hal_console_putchar(c);
}

void hal_debug_write(const char* str) {
    hal_console_write(str);
}
