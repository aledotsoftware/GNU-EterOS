/**
 * éterOS - 8259 PIC Implementation
 * Copyright (c) 2025 Tudex Networks. All rights reserved.
 *
 * Inicialización y control del Programmable Interrupt Controller.
 * Remapea IRQs para evitar conflictos con excepciones de CPU.
 */

#include <pic.h>
#include <io.h>

/* ========================================================================= */
/* Helpers internos                                                          */
/* ========================================================================= */

/* ========================================================================= */
/* API Pública                                                               */
/* ========================================================================= */

void pic_init(void) {
    /* Guardar máscaras actuales (aunque las vamos a sobrescribir) */
    /* ... */

    /* ICW1: Iniciar convite de inicialización */
    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();

    /* ICW2: Vector offset */
    outb(PIC1_DATA, 0x20);     /* Master: INT 32+ */
    io_wait();
    outb(PIC2_DATA, 0x28);     /* Slave: INT 40+ */
    io_wait();

    /* ICW3: Cascading */
    outb(PIC1_DATA, 0x04);     /* Master: Slave at IRQ2 */
    io_wait();
    outb(PIC2_DATA, 0x02);     /* Slave: Connected to IRQ2 */
    io_wait();

    /* ICW4: Environment info */
    outb(PIC1_DATA, 0x01);     /* 8086/88 mode */
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    /* Máscaras: Deshabilitar todo excepto lo que habiliten los drivers */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);  /* EOI al slave */
    }
    outb(PIC1_COMMAND, PIC_EOI);      /* EOI al master siempre */
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) | (1 << irq);
    outb(port, value);
}
