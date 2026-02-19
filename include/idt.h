/**
 * éterOS - Interrupt Descriptor Table (IDT)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Gestión de interrupciones para x86_64 Long Mode y i386 Protected Mode.
 * Tabla de 256 entradas que define los handlers para:
 *   - Excepciones de CPU (0-31)
 *   - IRQs de hardware (32-47)
 *   - Interrupciones de software (48-255)
 */

#ifndef ETEROS_IDT_H
#define ETEROS_IDT_H

#include "types.h"

/* ========================================================================= */
/* Constantes                                                                */
/* ========================================================================= */
#define IDT_ENTRIES         256

/* Selector de segmento de código (del GDT del bootloader) */
#define KERNEL_CS           0x08

/* IRQ base después del remapping del PIC */
#define IRQ_BASE            32

/* Inter-Processor Interrupts (IPI) */
#define IPI_TLB_SHOOTDOWN   0xFD

/* ========================================================================= */
/* Estructuras dependientes de la arquitectura                               */
/* ========================================================================= */

#ifdef __x86_64__

/* Tipos de gate — 64-bit */
#define IDT_GATE_INTERRUPT  0x8E    /* P=1, DPL=0, Interrupt Gate 64-bit */
#define IDT_GATE_TRAP       0x8F    /* P=1, DPL=0, Trap Gate 64-bit */
#define IDT_GATE_USER_INT   0xEE    /* P=1, DPL=3, Interrupt Gate */

/**
 * Entrada de la IDT para x86_64 (16 bytes).
 */
struct idt_entry {
    uint16_t offset_low;    /* Bits 0-15 del offset del handler  */
    uint16_t selector;      /* Selector de segmento de código    */
    uint8_t  ist;           /* Interrupt Stack Table (0 = none)  */
    uint8_t  type_attr;     /* Tipo y atributos del gate         */
    uint16_t offset_mid;    /* Bits 16-31 del offset             */
    uint32_t offset_high;   /* Bits 32-63 del offset             */
    uint32_t reserved;      /* Reservado, debe ser 0             */
} __attribute__((packed));

/**
 * Puntero a la IDT (para la instrucción LIDT) — 64-bit.
 */
struct idt_ptr {
    uint16_t limit;         /* Tamaño de la tabla - 1            */
    uint64_t base;          /* Dirección base de la tabla        */
} __attribute__((packed));

/**
 * Frame de interrupción — 64-bit.
 */
struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

#else /* i386 */

/* Tipos de gate — 32-bit */
#define IDT_GATE_INTERRUPT  0x8E    /* P=1, DPL=0, Interrupt Gate 32-bit */
#define IDT_GATE_TRAP       0x8F    /* P=1, DPL=0, Trap Gate 32-bit */
#define IDT_GATE_USER_INT   0xEE    /* P=1, DPL=3, Interrupt Gate */

/**
 * Entrada de la IDT para i386 (8 bytes).
 */
struct idt_entry {
    uint16_t offset_low;    /* Bits 0-15 del offset del handler  */
    uint16_t selector;      /* Selector de segmento de código    */
    uint8_t  zero;          /* Siempre 0                         */
    uint8_t  type_attr;     /* Tipo y atributos del gate         */
    uint16_t offset_high;   /* Bits 16-31 del offset             */
} __attribute__((packed));

/**
 * Puntero a la IDT (para la instrucción LIDT) — 32-bit.
 */
struct idt_ptr {
    uint16_t limit;         /* Tamaño de la tabla - 1            */
    uint32_t base;          /* Dirección base de la tabla        */
} __attribute__((packed));

/**
 * Frame de interrupción — 32-bit.
 */
struct interrupt_frame {
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t ss;
};

#endif /* __x86_64__ */

/* ========================================================================= */
/* API                                                                       */
/* ========================================================================= */

/**
 * Inicializa la IDT, instala handlers de excepciones e IRQs,
 * y carga la tabla con LIDT.
 */
void idt_init(void);

#endif /* ETEROS_IDT_H */
