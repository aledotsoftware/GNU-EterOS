#ifndef ETEROS_GDT_H
#define ETEROS_GDT_H

#include "types.h"

/**
 * Inicializa la Global Descriptor Table (GDT) y el Task State Segment (TSS).
 * Mueve la GDT a una ubicación segura en el kernel y configura los segmentos.
 */
void gdt_init(void);

/**
 * Actualiza el RSP0 del TSS (Stack Pointer para Ring 0).
 * Se debe llamar en cada cambio de tarea para que las interrupciones
 * usen el stack del kernel de la nueva tarea.
 */
void tss_set_rsp0(uint64_t rsp0);

#endif /* ETEROS_GDT_H */
