/**
 * éterOS - 8259 PIC Implementation
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Inicialización y control del Programmable Interrupt Controller.
 * Remapea IRQs para evitar conflictos con excepciones de CPU.
 */

#include "../../../include/pic.h"
#include "../../../include/io.h"

/* ========================================================================= */
/* Helpers internos                                                          */
/* ========================================================================= */

/**
 * Espera un ciclo de I/O para dar tiempo al PIC de procesar.
 */
static void pic_io_wait(void) {
    outb(0x80, 0);  /* Puerto 0x80 = POST debug, usado como delay */
}

/* ========================================================================= */
/* API Pública                                                               */
/* ========================================================================= */

void pic_init(void) {
    /* ICW1: Iniciar secuencia de inicialización (modo cascada + ICW4 needed) */
    outb(PIC1_COMMAND, 0x11);
    pic_io_wait();
    outb(PIC2_COMMAND, 0x11);
    pic_io_wait();

    /* ICW2: Definir vectores base de interrupción */
    outb(PIC1_DATA, 0x20);     /* Master PIC: IRQ 0-7  -> INT 32-39 */
    pic_io_wait();
    outb(PIC2_DATA, 0x28);     /* Slave PIC:  IRQ 8-15 -> INT 40-47 */
    pic_io_wait();

    /* ICW3: Configurar cascada (slave en IRQ2 del master) */
    outb(PIC1_DATA, 0x04);     /* Master: slave en pin 2 */
    pic_io_wait();
    outb(PIC2_DATA, 0x02);     /* Slave: cascade identity 2 */
    pic_io_wait();

    /* ICW4: Modo 8086/88 */
    outb(PIC1_DATA, 0x01);
    pic_io_wait();
    outb(PIC2_DATA, 0x01);
    pic_io_wait();

    /* Restaurar máscaras (enmascarar todo por defecto) */
    outb(PIC1_DATA, 0xFF);     /* Enmascarar todas las IRQs del master */
    outb(PIC2_DATA, 0xFF);     /* Enmascarar todas las IRQs del slave  */
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
