/**
 * éterOS — aarch64 HAL Implementation
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 */

#include <hal.h>

/* Global State */
static volatile uint64_t sys_ticks = 0;

void hal_init(void) {
    hal_console_init();
    hal_timer_init(100);
    hal_interrupts_enable();
}

void hal_interrupts_enable(void) {
    __asm__ volatile("msr daifclr, #2");
}

void hal_interrupts_disable(void) {
    __asm__ volatile("msr daifset, #2");
}

void hal_irq_install(uint8_t vector, irq_handler_t handler) {
    (void)vector; (void)handler;
}

void hal_cpu_halt(void) {
    __asm__ volatile("wfi");
}

void hal_cpu_reset(void) {
    /* Implementation specific to board (e.g. RPi Mailbox or PSCI) */
    while(1);
}

/* Console Stubs (UART PL011 or similar) */
void hal_console_init(void) { }
void hal_console_putchar(char c) { (void)c; }
void hal_console_write(const char* str) { while(*str) hal_console_putchar(*str++); }
void hal_console_clear(void) { }

/* Input Stubs */
void hal_input_init(void) { }
char hal_input_getchar(void) { return 0; }
bool hal_input_available(void) { return false; }

/* Timer Stubs */
void hal_timer_init(uint32_t hz) { (void)hz; }
uint64_t hal_timer_ticks(void) { return sys_ticks; }

/* Debug */
void hal_debug_putchar(char c) { hal_console_putchar(c); }
void hal_debug_write(const char* str) { hal_console_write(str); }

#if ETEROS_HAS_MMU
void hal_mmu_init(void) { }
#endif
