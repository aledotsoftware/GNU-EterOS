/**
 * éterOS - 8259 PIC (Programmable Interrupt Controller)
 * Copyright (c) 2025 Tudex Networks. All rights reserved.
 *
 * El PIC traduce IRQs de hardware en interrupciones de CPU.
 * Se remapea IRQ 0-7 a INT 32-39 e IRQ 8-15 a INT 40-47
 * para evitar conflictos con las excepciones de CPU (0-31).
 */

#ifndef ETEROS_PIC_H
#define ETEROS_PIC_H

#include "types.h"

/* ========================================================================= */
/* Puertos del PIC                                                           */
/* ========================================================================= */
#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

/* Comandos */
#define PIC_EOI         0x20    /* End of Interrupt */

/* ========================================================================= */
/* API                                                                       */
/* ========================================================================= */

/**
 * Inicializa y remapea los PICs maestro y esclavo.
 * IRQ 0-7  -> INT 32-39
 * IRQ 8-15 -> INT 40-47
 */
void pic_init(void);

/**
 * Envía End-of-Interrupt al PIC correspondiente.
 * @param irq Número de IRQ (0-15)
 */
void pic_send_eoi(uint8_t irq);

/**
 * Habilita una IRQ específica (desenmascara).
 * @param irq Número de IRQ (0-15)
 */
void pic_unmask_irq(uint8_t irq);

/**
 * Deshabilita una IRQ específica (enmascara).
 * @param irq Número de IRQ (0-15)
 */
void pic_mask_irq(uint8_t irq);

#endif /* ETEROS_PIC_H */
