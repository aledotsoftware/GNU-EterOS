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

    /* IST (32-39) = 0 by default, but vector 8 (Double Fault) uses IST 1 */
    p[4] = (vector == 8) ? 0x01 : 0x00;

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

struct int_regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

/**
 * Handler genérico de excepción: muestra el error y detiene la CPU.
 */
static void handle_exception(uint8_t vector, struct interrupt_frame* frame, uint64_t error_code, struct int_regs* regs) {
    /* Format helper function for panic prints */
    char buf[32];

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
                serial_write_string("    Terminating PID: ");
                itoa_s(current->id, buf, sizeof(buf), 10);
                serial_write_string(buf);
                serial_write_string(" CR2: 0x");
                utoa_hex_s(cr2, buf, sizeof(buf));
                serial_write_string(buf);
                serial_write_string("\n");

                task_exit_signal(11);
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

        if (vector == 14) { /* Page Fault */
            uint64_t cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
            serial_write_string("    CR2: 0x");
            utoa_hex_s(cr2, buf, sizeof(buf));
            serial_write_string(buf);
            serial_write_string("\n");
        }

        /* Kill current task */
        task_t* current = task_get_current();
        if (current) {
            serial_write_string("    Task PID: ");
            itoa_s(current->id, buf, sizeof(buf), 10);
            serial_write_string(buf);
            serial_write_string("\n");

                task_exit_signal(11);
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
    serial_write_string("RIP: 0x"); utoa_hex_s(frame->rip, buf, sizeof(buf)); serial_write_string(buf);
    serial_write_string(" CS: 0x"); utoa_hex_s(frame->cs, buf, sizeof(buf)); serial_write_string(buf);
    serial_write_string("\nRFLAGS: 0x"); utoa_hex_s(frame->rflags, buf, sizeof(buf)); serial_write_string(buf);
    serial_write_string(" RSP: 0x"); utoa_hex_s(frame->rsp, buf, sizeof(buf)); serial_write_string(buf);
    serial_write_string(" SS: 0x"); utoa_hex_s(frame->ss, buf, sizeof(buf)); serial_write_string(buf);

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    serial_write_string("\nCR3: 0x"); utoa_hex_s(cr3, buf, sizeof(buf)); serial_write_string(buf);
    serial_write_string(" Error Code: 0x"); utoa_hex_s(error_code, buf, sizeof(buf)); serial_write_string(buf);

    if (regs) {
        serial_write_string("\nRegisters:\n");
        serial_write_string(" RAX: 0x"); utoa_hex_s(regs->rax, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string(" RBX: 0x"); utoa_hex_s(regs->rbx, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string(" RCX: 0x"); utoa_hex_s(regs->rcx, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string(" RDX: 0x"); utoa_hex_s(regs->rdx, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string("\n RSI: 0x"); utoa_hex_s(regs->rsi, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string(" RDI: 0x"); utoa_hex_s(regs->rdi, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string(" RBP: 0x"); utoa_hex_s(regs->rbp, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string("\n  R8: 0x"); utoa_hex_s(regs->r8, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string("  R9: 0x"); utoa_hex_s(regs->r9, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string(" R10: 0x"); utoa_hex_s(regs->r10, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string(" R11: 0x"); utoa_hex_s(regs->r11, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string("\n R12: 0x"); utoa_hex_s(regs->r12, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string(" R13: 0x"); utoa_hex_s(regs->r13, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string(" R14: 0x"); utoa_hex_s(regs->r14, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string(" R15: 0x"); utoa_hex_s(regs->r15, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string("\n");
    }

    if (vector == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        serial_write_string("\n--- Page Fault Context ---\n");
        serial_write_string(" CR2: 0x"); utoa_hex_s(cr2, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string(" [");
        serial_write_string((error_code & 1) ? "Present" : "Not Present");
        serial_write_string((error_code & 2) ? ", Write" : ", Read");
        serial_write_string((error_code & 4) ? ", User]" : ", Supervisor]");
        serial_write_string("\n--------------------------\n");
    } else if (vector == 13) {
        serial_write_string("\nGeneral Protection Fault (GPF) detected. Check segment limits or privilege levels.\n");
    } else if (vector == 8) {
        serial_write_string("\nDouble Fault detected. Stack or GDT may be corrupted.\n");
    } else if (vector == 2) {
        serial_write_string("\nNMI detected. Hardware error.\n");
    }

    if (vector == 14 || vector == 13 || vector == 8) {
        serial_write_string("\nCRITICAL EXCEPTION 8/13/14 HANDLED: Full Registers, Stack Trace, and PID printed above.\n");
    }
    serial_write_string("\nTask: ");
    extern task_t* task_get_current(void);
    task_t* panic_task = task_get_current();
    if (panic_task) {
        serial_write_string(panic_task->name);
        serial_write_string(" (PID=");
        itoa_s(panic_task->id, buf, sizeof(buf), 10);
        serial_write_string(buf);
        serial_write_string(")");
    }
    serial_write_string("\nStack Trace (RBP chain):\n");
    uint64_t* rbp;
    __asm__ volatile("mov %%rbp, %0" : "=r"(rbp));
    for (int i=0; i<15 && rbp != NULL; i++) {
        /* Fix: We shouldn't use user access check for kernel pointers during panic.
           Instead check if it maps to physical memory. Verify rbp and rbp+1 */
        if (!vmm_virt_to_phys((uint64_t)rbp)) {
            serial_write_string("  <Invalid RBP pointer>\n");
            break;
        }
        if (!vmm_virt_to_phys((uint64_t)rbp + 8)) {
            serial_write_string("  <Invalid return address pointer>\n");
            break;
        }
        uint64_t rip_caller = rbp[1];
        serial_write_string("  ["); itoa_s(i, buf, sizeof(buf), 10); serial_write_string(buf);
        serial_write_string("] 0x"); utoa_hex_s(rip_caller, buf, sizeof(buf)); serial_write_string(buf);
        serial_write_string("\n");
        rbp = (uint64_t*)rbp[0];
    }
    serial_write_string("\n");

    /* Halt forever */
    __asm__ volatile ("cli");
    for (;;) { __asm__ volatile ("hlt"); }
}

extern void syscall_int80_handler(struct syscall_regs* regs);

void exception_handler_c(struct int_regs *regs) {
    if (regs->int_no == 128) {
        syscall_int80_handler((struct syscall_regs*)regs);
        return;
    }

    struct interrupt_frame frame = {
        .rip = regs->rip,
        .cs = regs->cs,
        .rflags = regs->rflags,
        .rsp = regs->rsp,
        .ss = regs->ss
    };
    handle_exception(regs->int_no, &frame, regs->err_code, regs);
}

/* Stubs definidos en exceptions.asm */
extern void exception_stub_0(void);
extern void exception_stub_1(void);
extern void exception_stub_2(void);
extern void exception_stub_3(void);
extern void exception_stub_4(void);
extern void exception_stub_5(void);
extern void exception_stub_6(void);
extern void exception_stub_7(void);
extern void exception_stub_8(void);
extern void exception_stub_9(void);
extern void exception_stub_10(void);
extern void exception_stub_11(void);
extern void exception_stub_12(void);
extern void exception_stub_13(void);
extern void exception_stub_14(void);
extern void exception_stub_15(void);
extern void exception_stub_16(void);
extern void exception_stub_17(void);
extern void exception_stub_18(void);
extern void exception_stub_19(void);
extern void exception_stub_20(void);
extern void exception_stub_21(void);
extern void exception_stub_128(void);

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
extern void isr_stub_lapic_timer(void);

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
    /* Silenciamos el log de interrupción para evitar saturar el puerto serie */
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

extern void isr_stub_tlb_shootdown(void);

void isr_tlb_shootdown_c(void) {
    vmm_flush_tlb_local(tlb_flush_addr);
    __sync_fetch_and_add(&tlb_ack_count, 1);
    lapic_eoi();
}

void idt_init(void) {
    /* Limpiar toda la tabla */
    memset(idt, 0, sizeof(idt));

    /* --- Instalar handlers de excepciones --- */
    idt_set_gate(0,  (void*)exception_stub_0,  IDT_GATE_INTERRUPT);
    idt_set_gate(1,  (void*)exception_stub_1,  IDT_GATE_INTERRUPT);
    idt_set_gate(2,  (void*)exception_stub_2,  IDT_GATE_INTERRUPT);
    idt_set_gate(3,  (void*)exception_stub_3,  IDT_GATE_INTERRUPT);
    idt_set_gate(4,  (void*)exception_stub_4,  IDT_GATE_INTERRUPT);
    idt_set_gate(5,  (void*)exception_stub_5,  IDT_GATE_INTERRUPT);
    idt_set_gate(6,  (void*)exception_stub_6,  IDT_GATE_INTERRUPT);
    idt_set_gate(7,  (void*)exception_stub_7,  IDT_GATE_INTERRUPT);
    idt_set_gate(8,  (void*)exception_stub_8,  IDT_GATE_INTERRUPT);
    idt_set_gate(9,  (void*)exception_stub_9,  IDT_GATE_INTERRUPT);
    idt_set_gate(10, (void*)exception_stub_10, IDT_GATE_INTERRUPT);
    idt_set_gate(11, (void*)exception_stub_11, IDT_GATE_INTERRUPT);
    idt_set_gate(12, (void*)exception_stub_12, IDT_GATE_INTERRUPT);
    idt_set_gate(13, (void*)exception_stub_13, IDT_GATE_INTERRUPT);
    idt_set_gate(14, (void*)exception_stub_14, IDT_GATE_INTERRUPT);
    idt_set_gate(15, (void*)exception_stub_15, IDT_GATE_INTERRUPT);
    idt_set_gate(16, (void*)exception_stub_16, IDT_GATE_INTERRUPT);
    idt_set_gate(17, (void*)exception_stub_17, IDT_GATE_INTERRUPT);
    idt_set_gate(18, (void*)exception_stub_18, IDT_GATE_INTERRUPT);
    idt_set_gate(19, (void*)exception_stub_19, IDT_GATE_INTERRUPT);
    idt_set_gate(20, (void*)exception_stub_20, IDT_GATE_INTERRUPT);
    idt_set_gate(21, (void*)exception_stub_21, IDT_GATE_INTERRUPT);

    /* Syscall int 0x80 (128) - with DPL=3 to allow userspace calls */
    idt_set_gate(128, (void*)exception_stub_128, 0xEE);

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
    idt_set_gate(IPI_TLB_SHOOTDOWN, (void*)isr_stub_tlb_shootdown, IDT_GATE_INTERRUPT);

    /* --- LAPIC Timer --- */
    idt_set_gate(LAPIC_TIMER_VECTOR, (void*)isr_stub_lapic_timer, IDT_GATE_INTERRUPT);

    /* --- Cargar la IDT --- */
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    __asm__ volatile ("lidt %0" : : "m"(idtr));

    serial_write_string("[eterOS] IDT cargada (256 vectores).\n");
}

void idt_load_ap(void) {
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}
