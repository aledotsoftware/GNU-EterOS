#ifndef ETEROS_GDT_H
#define ETEROS_GDT_H

#include "types.h"

/**
 * Inicializa la Global Descriptor Table (GDT) y el Task State Segment (TSS).
 * Mueve la GDT a una ubicación segura en el kernel y configura los segmentos.
 */
void gdt_init(void);

#endif /* ETEROS_GDT_H */
