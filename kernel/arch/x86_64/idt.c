/**
 * éterOS - IDT Implementation (x86_64)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Configura la Interrupt Descriptor Table con handlers para:
 *   - Excepciones de CPU (0-31): Division by zero, Page fault, etc.
 *   - IRQs de hardware (32-47): Teclado, Timer, etc.
 *
 * Usa __attribute__((interrupt)) de GCC para generar los ISR stubs
 * automáticamente (save regs, iretq).
 */

#pragma GCC target("general-regs-only")

#include "../../../include/idt.h"
#include "../../../include/pic.h"
#include "../../../include/keyboard.h"
#include "../../../include/vga.h"
#include "../../../include/serial.h"
#include "../../../include/io.h"
#include "../../../include/string.h"

/* ========================================================================= */
/* Tabla IDT (256 entradas × 16 bytes = 4 KB)                               */
/* ========================================================================= */
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtr;

/* ========================================================================= */
/* Helpers internos                                                          */
/* ========================================================================= */

/**
 * Configura una entrada de la IDT con el handler dado.
 */
static void idt_set_gate(uint8_t vector, void* handler, uint8_t type_attr) {
    uint64_t addr = (uint64_t)handler;

    idt[vector].offset_low  = (uint16_t)(addr & 0xFFFF);
    idt[vector].selector    = KERNEL_CS;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[vector].reserved    = 0;
}

/* ========================================================================= */
/* Nombres de excepciones (para debug)                                       */
/* ========================================================================= */
static const char* exception_names[] = {
    "Division by Zero",          /*  0 */
    "Debug",                     /*  1 */
    "NMI",                       /*  2 */
    "Breakpoint",                /*  3 */
    "Overflow",                  /*  4 */
    "Bound Range Exceeded",      /*  5 */
    "Invalid Opcode",            /*  6 */
    "Device Not Available",      /*  7 */
    "Double Fault",              /*  8 */
    "Coprocessor Overrun",       /*  9 */
    "Invalid TSS",               /* 10 */
    "Segment Not Present",       /* 11 */
    "Stack-Segment Fault",       /* 12 */
    "General Protection Fault",  /* 13 */
    "Page Fault",                /* 14 */
    "Reserved",                  /* 15 */
    "x87 FP Exception",         /* 16 */
    "Alignment Check",           /* 17 */
    "Machine Check",             /* 18 */
    "SIMD FP Exception",        /* 19 */
    "Virtualization Exception",  /* 20 */
};

#define NUM_EXCEPTION_NAMES  21

/**
 * Handler genérico de excepción: muestra el error y detiene la CPU.
 */
static void handle_exception(uint8_t vector) {
    terminal_write_colored("\n  [PANIC] ", VGA_COLOR_RED, VGA_COLOR_BLACK);

    if (vector < NUM_EXCEPTION_NAMES) {
        terminal_write_string(exception_names[vector]);
    } else {
        terminal_write_string("Unknown Exception");
    }

    terminal_write_string(" (INT ");
    char buf[8];
    itoa_s(vector, buf, sizeof(buf), 10);
    terminal_write_string(buf);
    terminal_write_string(")\n");

    terminal_write_colored("  Sistema detenido.\n", VGA_COLOR_RED, VGA_COLOR_BLACK);

    serial_write_string("[PANIC] Exception: ");
    serial_write_string(vector < NUM_EXCEPTION_NAMES ? exception_names[vector] : "Unknown");
    serial_write_string("\n");

    /* Halt forever */
    __asm__ volatile ("cli");
    for (;;) { __asm__ volatile ("hlt"); }
}

/* ========================================================================= */
/* Exception Handlers (sin error code)                                       */
/* ========================================================================= */

#define EXCEPTION_HANDLER(num) \
    __attribute__((interrupt)) \
    static void isr_##num(struct interrupt_frame *frame) { \
        (void)frame; \
        handle_exception(num); \
    }

#define EXCEPTION_HANDLER_ERR(num) \
    __attribute__((interrupt)) \
    static void isr_##num(struct interrupt_frame *frame, uint64_t error_code) { \
        (void)frame; \
        (void)error_code; \
        handle_exception(num); \
    }

/* Excepciones sin error code */
EXCEPTION_HANDLER(0)
EXCEPTION_HANDLER(1)
EXCEPTION_HANDLER(2)
EXCEPTION_HANDLER(3)
EXCEPTION_HANDLER(4)
EXCEPTION_HANDLER(5)
EXCEPTION_HANDLER(6)
EXCEPTION_HANDLER(7)
EXCEPTION_HANDLER(9)
EXCEPTION_HANDLER(16)
EXCEPTION_HANDLER(18)
EXCEPTION_HANDLER(19)
EXCEPTION_HANDLER(20)

/* Excepciones con error code */
EXCEPTION_HANDLER_ERR(8)
EXCEPTION_HANDLER_ERR(10)
EXCEPTION_HANDLER_ERR(11)
EXCEPTION_HANDLER_ERR(12)
EXCEPTION_HANDLER_ERR(13)
EXCEPTION_HANDLER_ERR(14)
EXCEPTION_HANDLER_ERR(17)
EXCEPTION_HANDLER_ERR(21)

/* ========================================================================= */
/* IRQ Handlers                                                              */
/* ========================================================================= */

/**
 * IRQ0 - Timer (PIT). Por ahora solo envía EOI.
 */
__attribute__((interrupt))
static void irq_timer(struct interrupt_frame *frame) {
    (void)frame;
    pic_send_eoi(0);
}

/**
 * IRQ1 - Teclado PS/2. Lee scancode y lo pasa al driver de teclado.
 */
__attribute__((interrupt))
static void irq_keyboard(struct interrupt_frame *frame) {
    (void)frame;
    uint8_t scancode = inb(KB_DATA_PORT);
    keyboard_process_scancode(scancode);
    pic_send_eoi(1);
}

/**
 * Handler por defecto para IRQs no manejadas.
 */
__attribute__((interrupt))
static void irq_default(struct interrupt_frame *frame) {
    (void)frame;
    /* Enviar EOI a ambos PICs por si acaso */
    outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

/* ========================================================================= */
/* Inicialización                                                            */
/* ========================================================================= */

void idt_init(void) {
    /* Limpiar toda la tabla */
    memset(idt, 0, sizeof(idt));

    /* --- Instalar handlers de excepciones --- */
    idt_set_gate(0,  (void*)isr_0,  IDT_GATE_INTERRUPT);
    idt_set_gate(1,  (void*)isr_1,  IDT_GATE_INTERRUPT);
    idt_set_gate(2,  (void*)isr_2,  IDT_GATE_INTERRUPT);
    idt_set_gate(3,  (void*)isr_3,  IDT_GATE_INTERRUPT);
    idt_set_gate(4,  (void*)isr_4,  IDT_GATE_INTERRUPT);
    idt_set_gate(5,  (void*)isr_5,  IDT_GATE_INTERRUPT);
    idt_set_gate(6,  (void*)isr_6,  IDT_GATE_INTERRUPT);
    idt_set_gate(7,  (void*)isr_7,  IDT_GATE_INTERRUPT);
    idt_set_gate(8,  (void*)isr_8,  IDT_GATE_INTERRUPT);
    idt_set_gate(9,  (void*)isr_9,  IDT_GATE_INTERRUPT);
    idt_set_gate(10, (void*)isr_10, IDT_GATE_INTERRUPT);
    idt_set_gate(11, (void*)isr_11, IDT_GATE_INTERRUPT);
    idt_set_gate(12, (void*)isr_12, IDT_GATE_INTERRUPT);
    idt_set_gate(13, (void*)isr_13, IDT_GATE_INTERRUPT);
    idt_set_gate(14, (void*)isr_14, IDT_GATE_INTERRUPT);
    idt_set_gate(16, (void*)isr_16, IDT_GATE_INTERRUPT);
    idt_set_gate(17, (void*)isr_17, IDT_GATE_INTERRUPT);
    idt_set_gate(18, (void*)isr_18, IDT_GATE_INTERRUPT);
    idt_set_gate(19, (void*)isr_19, IDT_GATE_INTERRUPT);
    idt_set_gate(20, (void*)isr_20, IDT_GATE_INTERRUPT);
    idt_set_gate(21, (void*)isr_21, IDT_GATE_INTERRUPT);

    /* --- Instalar handlers de IRQs (32-47) --- */
    idt_set_gate(IRQ_BASE + 0,  (void*)irq_timer,    IDT_GATE_INTERRUPT);
    idt_set_gate(IRQ_BASE + 1,  (void*)irq_keyboard, IDT_GATE_INTERRUPT);

    /* IRQs restantes: handler por defecto */
    for (int i = 2; i < 16; i++) {
        idt_set_gate(IRQ_BASE + i, (void*)irq_default, IDT_GATE_INTERRUPT);
    }

    /* --- Cargar la IDT --- */
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    __asm__ volatile ("lidt %0" : : "m"(idtr));

    serial_write_string("[eterOS] IDT cargada (256 vectores).\n");
}
