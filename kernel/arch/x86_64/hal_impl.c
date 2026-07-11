/**
 * éterOS — x86_64 HAL Implementation
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Target: x86_64 (Intel/AMD)
 */

#include <hal.h>
#include <types.h>
#include <string.h>
#include <vga.h>
#include <serial.h>
#include <gdt.h>
#include <idt.h>
#include <pic.h>
#include <keyboard.h>
#include <mouse.h>
#include <timer.h>
#include <io.h>
#include <vmm.h>
#include <boot.h>
#include <syscall.h>
#include <input/event.h>

/* ========================================================================= */
/* HAL Implementation                                                        */
/* ========================================================================= */

void hal_init(void) {
    /* 1. Initialize Console (VGA + Serial) */
    terminal_write_string("[HAL] console\n");
    hal_console_init();

    /* 2. Initialize GDT */
    terminal_write_string("[HAL] gdt\n");
    gdt_init();

    /* 3. Initialize PIC (Remap IRQs) */
    terminal_write_string("[HAL] pic\n");
    pic_init();
    pic_unmask_irq(4); /* Serial COM1 */

    /* 4. Initialize IDT */
    terminal_write_string("[HAL] idt\n");
    idt_init();

    /* 5. Initialize Syscalls (MSRs) */
    terminal_write_string("[HAL] syscall\n");
    syscall_init();

    /* 6. Initialize Input (Event System + Keyboard) */
    terminal_write_string("[HAL] input\n");
    input_init();
    hal_input_init();

    /* 6. Initialize Timer (PIT) */
    /* Default to 100Hz as per original kmain */
    terminal_write_string("[HAL] timer\n");
    hal_timer_init(100);

    /* 7. Interrupts stay disabled here.
     * They are enabled later from kmain once PMM/VMM/scheduler are ready.
     */
    terminal_write_string("[HAL] irq-late\n");
    serial_write_string("[HAL] Interrupts deferred until late boot.\n");
}

void hal_interrupts_enable(void) {
    __asm__ volatile ("sti");
}

void hal_interrupts_disable(void) {
    __asm__ volatile ("cli");
}

void hal_irq_install(uint8_t vector, irq_handler_t handler) {
    /* x86 IDT handles installing ISRs via idt_set_gate in idt.c */
    /* This function is a placeholder if we want dynamic IRQ registration */
    (void)vector;
    (void)handler;
}

void hal_cpu_halt(void) {
    __asm__ volatile ("hlt");
}

void hal_cpu_enable_interrupts_and_halt(void) {
    __asm__ volatile ("sti; hlt");
}

void hal_cpu_reset(void) {
    /* Method 1: Keyboard Controller Pulse */
    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);

    /* Method 2: Triple Fault */
    /* Intentionally cause a triple fault by loading an invalid IDT */
    /* struct { uint16_t limit; uint64_t base; } __attribute__((packed)) idtr = { 0, 0 }; */
    /* __asm__ volatile ("lidt %0; int3" : : "m"(idtr)); */

    while(1) hal_cpu_halt();
}

/* ---- Console (VGA + Serial) ---- */

void hal_console_init(void) {
    /* Get Boot Info from fixed address 0xA000 */
    boot_info_t* boot_info = (boot_info_t*)BOOT_INFO_ADDR;

    /* Initialize VGA Terminal */
    terminal_initialize(boot_info);

    /* Initialize Serial Port (COM1) */
    serial_init();
}

void hal_console_putchar(char c) {
    /* Output to VGA */
    /* Note: terminal_write_char handles newlines but expects (char, color) arguments */
    /* We don't have direct char output in vga.h that matches exactly,
       but we have terminal_write_string. */
    char buf[2] = { c, '\0' };
    terminal_write_string(buf);

    /* Also output to Serial for debugging */
    serial_putchar(c);
}

void hal_console_write(const char* str) {
    terminal_write_string(str);
    serial_write_string(str);
}

void hal_console_clear(void) {
    terminal_clear();
}

/* ---- Input (Keyboard) ---- */

void hal_input_init(void) {
    keyboard_init();
    mouse_init();
    pic_unmask_irq(1); /* Enable Keyboard IRQ */
}

char hal_input_getchar(void) {
    return keyboard_getchar();
}

bool hal_input_available(void) {
    return keyboard_has_input();
}

/* ---- Timer (PIT) ---- */

void hal_timer_init(uint32_t hz) {
    (void)hz; /* Suppress unused parameter warning */
    timer_init(); /* Initializes PIT to default 100Hz in current impl */
    /* If we want custom Hz, we need to modify timer_init to take Hz args.
       Current timer.c defines TIMER_HZ 100.
       We will stick to default for now. */
    pic_unmask_irq(0); /* Enable Timer IRQ */
}

uint64_t hal_timer_ticks(void) {
    return timer_get_ticks();
}

/* ---- Memory (MMU) ---- */

#if ETEROS_HAS_MMU
void hal_mmu_init(void) {
    vmm_init();
}
#endif

/* ---- Debug ---- */

void hal_debug_putchar(char c) {
    serial_putchar(c);
}

void hal_debug_write(const char* str) {
    serial_write_string(str);
}
