#ifndef ETEROS_GDT_H
#define ETEROS_GDT_H

#include "types.h"

/**
 * Inicializa la Global Descriptor Table (GDT) y el Task State Segment (TSS).
 * Mueve la GDT a una ubicación segura en el kernel y configura los segmentos.
 */
void gdt_init(void);

/**
 * Actualiza el valor de RSP0 en el TSS.
 * Esto es necesario en cada cambio de contexto para que,
 * cuando ocurra una interrupción desde Ring 3, el CPU sepa
 * dónde encontrar el stack del kernel para el proceso actual.
 *
 * @param rsp0 Dirección del tope del stack de kernel (stack top).
 */
void tss_set_rsp0(uint64_t rsp0);

#endif /* ETEROS_GDT_H */
