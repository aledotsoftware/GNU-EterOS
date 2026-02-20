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
#include "../../../include/mouse.h"
#include "../../../include/vga.h"
#include "../../../include/serial.h"
#include "../../../include/io.h"
#include "../../../include/string.h"
#include "../../../include/timer.h"
#include "../../../include/task.h"
#include "../../../include/vmm.h"
#include "../../../include/apic.h"

/* ========================================================================= */
/* Tabla IDT (256 entradas × 16 bytes = 4 KB)                               */
/* ========================================================================= */
_Static_assert(sizeof(struct idt_entry) == 16, "IDT Entry size must be 16 bytes");

static struct idt_entry idt[IDT_ENTRIES] __attribute__((aligned(16)));
static struct idt_ptr   idtr;

/* ========================================================================= */
/* Helpers internos                                                          */
/* ========================================================================= */

/**
 * Configura una entrada de la IDT con el handler dado.
 */
static void idt_set_gate(uint8_t vector, void* handler, uint8_t type_attr) {
    uint64_t addr = (uint64_t)handler;

    /* Access as raw bytes to guarantee layout and debug */
    uint8_t* p = (uint8_t*)&idt[vector];

    /* Offset Low (0-15) */
    p[0] = (addr) & 0xFF;
    p[1] = (addr >> 8) & 0xFF;

    /* Selector (16-31) = KERNEL_CS (0x08) */
    p[2] = 0x08;
    p[3] = 0x00;

    /* IST (32-39) = 0 */
    p[4] = 0x00;

    /* Type/Attr (40-47) */
    p[5] = type_attr;

    /* Offset Mid (48-63) */
    p[6] = (addr >> 16) & 0xFF;
    p[7] = (addr >> 24) & 0xFF;

    /* Offset High (64-95) */
    p[8] = (addr >> 32) & 0xFF;
    p[9] = (addr >> 40) & 0xFF;
    p[10] = (addr >> 48) & 0xFF;
    p[11] = (addr >> 56) & 0xFF;

    /* Reserved (96-127) */
    *(uint32_t*)&p[12] = 0;
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
static void handle_exception(uint8_t vector, struct interrupt_frame* frame, uint64_t error_code) {
    /* Check if Page Fault (14) */
    if (vector == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        if (vmm_handle_page_fault(cr2, error_code)) {
            return; /* Handled */
        }

        /* Check for Kernel Mode access to User Space (Bad User Pointer) */
        /* If we are in Kernel Mode (CS&3 == 0) and accessing a User Address, */
        /* it means copy_from_user/validate failed (e.g. unmapped page). */
        /* We should kill the task instead of panicking. */
        if ((frame->cs & 3) == 0 && cr2 >= USER_BASE && cr2 <= USER_LIMIT) {
            serial_write_string("\n[EXCEPTION] Kernel Page Fault on User Address. Bad pointer?\n");

            task_t* current = task_get_current();
            if (current) {
                char buf[32];
                serial_write_string("    Terminating PID: ");
                itoa_s(current->id, buf, sizeof(buf), 10);
                serial_write_string(buf);
                serial_write_string("\n");

                task_kill(current->id);
                schedule();
                /* Should not return */
            }
        }
    }

    /* Check if exception happened in User Mode (CS & 3 == 3) */
    if ((frame->cs & 3) == 3) {
        serial_write_string("\n[EXCEPTION] User Mode Fault detected. Terminating task.\n");

        char buf[32];
        if (vector < NUM_EXCEPTION_NAMES) {
            serial_write_string("    Reason: ");
            serial_write_string(exception_names[vector]);
            serial_write_string("\n");
        }

        serial_write_string("    RIP: 0x");
        utoa_hex_s(frame->rip, buf, sizeof(buf));
        serial_write_string(buf);
        serial_write_string("\n");

        /* Kill current task */
        task_t* current = task_get_current();
        if (current) {
            serial_write_string("    Task PID: ");
            itoa_s(current->id, buf, sizeof(buf), 10);
            serial_write_string(buf);
            serial_write_string("\n");

            task_kill(current->id);
            /* task_kill calls schedule() if killing self, so we shouldn't return here */
        }

        /* Just in case we return (e.g. error killing task), loop forever or schedule */
        schedule();
        for(;;) __asm__ volatile("hlt");
        return;
    }

    /* Kernel Panic */
    terminal_write_colored("\n  [PANIC] ", VGA_COLOR_RED, VGA_COLOR_BLACK);

    if (vector < NUM_EXCEPTION_NAMES) {
        terminal_write_string(exception_names[vector]);
    } else {
        terminal_write_string("Unknown Exception");
    }

    terminal_write_string(" (INT ");
    char buf[32];
    itoa_s(vector, buf, sizeof(buf), 10);
    terminal_write_string(buf);
    terminal_write_string(")\n");
    
    /* Print detailed info */
    terminal_write_string("    RIP: 0x");
    utoa_hex_s(frame->rip, buf, sizeof(buf));
    terminal_write_string(buf);
    
    terminal_write_string("  CS: 0x");
    utoa_hex_s(frame->cs, buf, sizeof(buf));
    terminal_write_string(buf);
    
    terminal_write_string("  RFLAGS: 0x");
    utoa_hex_s(frame->rflags, buf, sizeof(buf));
    terminal_write_string(buf);
    
    terminal_write_string("\n    Error Code: 0x");
    utoa_hex_s(error_code, buf, sizeof(buf));
    terminal_write_string(buf);
    terminal_write_string("\n");

    terminal_write_colored("  Sistema detenido.\n", VGA_COLOR_RED, VGA_COLOR_BLACK);
    
    /* Also print to serial */
    serial_write_string("[PANIC] Exception INT ");
    itoa_s(vector, buf, sizeof(buf), 10);
    serial_write_string(buf);
    serial_write_string("\n");
    serial_write_string("RIP: "); utoa_hex_s(frame->rip, buf, sizeof(buf)); serial_write_string(buf);
    serial_write_string(" CS: "); utoa_hex_s(frame->cs, buf, sizeof(buf)); serial_write_string(buf);
    serial_write_string("\nRFLAGS: "); utoa_hex_s(frame->rflags, buf, sizeof(buf)); serial_write_string(buf);
    serial_write_string(" Error: "); utoa_hex_s(error_code, buf, sizeof(buf)); serial_write_string(buf);
    serial_write_string("\nTask: ");
    extern task_t* task_get_current(void);
    task_t* panic_task = task_get_current();
    if (panic_task) {
        serial_write_string(panic_task->name);
        serial_write_string(" (state=");
        itoa_s(panic_task->state, buf, sizeof(buf), 10);
        serial_write_string(buf);
        serial_write_string(")");
    }
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
        handle_exception(num, frame, 0); \
    }

#define EXCEPTION_HANDLER_ERR(num) \
    __attribute__((interrupt)) \
    static void isr_##num(struct interrupt_frame *frame, uint64_t error_code) { \
        handle_exception(num, frame, error_code); \
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
/* ========================================================================= */
/* IRQ Handlers                                                              */
/* ========================================================================= */

/* ========================================================================= */
/* Wrapper en ASM para IRQ0 (Timer) para evitar GPF por alineación           */
/* ========================================================================= */
/* ========================================================================= */
/* Wrapper en ASM para IRQ0 (Timer) en kernel/arch/x86_64/interrupts.asm     */
/* ========================================================================= */
extern void isr_stub_timer(void);
extern void isr_stub_keyboard(void);
extern void isr_stub_serial(void);
extern void isr_stub_mouse(void);
extern void isr_stub_network(void);

/* ========================================================================= */
/* IRQ Handlers (Funciones C llamadas por los Stubs ASM)                     */
/* ========================================================================= */

/**
 * Función C para Timer (IRQ0). 
 * Llamada desde isr_stub_timer (assembly).
 */
void irq_timer_handler(void) {
    timer_tick();
    pic_send_eoi(0);
    schedule(); /* Habilitar scheduler */
}

/**
 * Función C para Teclado (IRQ1).
 * Llamada desde isr_stub_keyboard (assembly).
 */
void irq_keyboard_handler(void) {
    uint8_t scancode = inb(KB_DATA_PORT);
    keyboard_process_scancode(scancode);
    pic_send_eoi(1);
}

/**
 * IRQ4 - Puerto Serie COM1/COM3.
 * Llamada desde isr_stub_serial (assembly).
 */
void irq_serial_handler(void) {
    serial_irq_handler();
    pic_send_eoi(4);
}

/**
 * IRQ12 - Mouse PS/2.
 * Llamada desde isr_stub_mouse (assembly).
 */
void irq_mouse_handler(void) {
    uint8_t byte = inb(0x60);
    mouse_process_byte(byte);
    pic_send_eoi(12);
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

/* --- IPI Handlers --- */
extern volatile uint64_t tlb_flush_addr;
extern volatile uint64_t tlb_ack_count;

__attribute__((interrupt))
static void isr_tlb_shootdown(struct interrupt_frame *frame) {
    (void)frame;
    vmm_flush_tlb_local(tlb_flush_addr);
    __sync_fetch_and_add(&tlb_ack_count, 1);
    lapic_eoi();
}

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
    /* Usamos isr_stub_timer (assembly) -> irq_timer_handler (C) */
    idt_set_gate(IRQ_BASE + 0,  (void*)isr_stub_timer,    IDT_GATE_INTERRUPT);
    
    idt_set_gate(IRQ_BASE + 1,  (void*)isr_stub_keyboard, IDT_GATE_INTERRUPT);
    idt_set_gate(IRQ_BASE + 4,  (void*)isr_stub_serial,   IDT_GATE_INTERRUPT);
    idt_set_gate(IRQ_BASE + 11, (void*)isr_stub_network,  IDT_GATE_INTERRUPT);
    idt_set_gate(IRQ_BASE + 12, (void*)isr_stub_mouse,    IDT_GATE_INTERRUPT);

    /* IRQs restantes: handler por defecto */
    for (int i = 2; i < 16; i++) {
        if (i == 4 || i == 11 || i == 12) continue;
        idt_set_gate(IRQ_BASE + i, (void*)irq_default, IDT_GATE_INTERRUPT);
    }

    /* --- IPI Handlers --- */
    idt_set_gate(IPI_TLB_SHOOTDOWN, (void*)isr_tlb_shootdown, IDT_GATE_INTERRUPT);

    /* --- Cargar la IDT --- */
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    __asm__ volatile ("lidt %0" : : "m"(idtr));

    serial_write_string("[eterOS] IDT cargada (256 vectores).\n");
}
