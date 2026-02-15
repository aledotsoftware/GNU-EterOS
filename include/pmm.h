#ifndef ETEROS_PMM_H
#define ETEROS_PMM_H

#include "types.h"

/* ========================================================================= */
/* Estructuras del Mapa de Memoria (E820 BIOS)                               */
/* ========================================================================= */

/* Dirección física donde el bootloader guardó el mapa */
#define MEM_MAP_ADDR  0x5000

/* Tipos de memoria E820 */
#define E820_USABLE     1
#define E820_RESERVED   2
#define E820_ACPI_RECLAIM 3
#define E820_ACPI_NVS   4
#define E820_BAD_MEMORY 5

/* Entrada individual del mapa de memoria */
struct e820_entry {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t extended_attr;
} __attribute__((packed));

/* Estructura completa del mapa */
struct memory_map {
    uint32_t entry_count;
    struct e820_entry entries[];
} __attribute__((packed));

/* ========================================================================= */
/* Definiciones del PMM                                                      */
/* ========================================================================= */

#define PAGE_SIZE       4096
#define PAGE_SHIFT      12

/* Función para alinear una dirección al siguiente límite de página */
#define PAGE_ALIGN_UP(addr)   (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~(PAGE_SIZE - 1))

/* ========================================================================= */
/* API Pública del PMM                                                       */
/* ========================================================================= */

/**
 * Inicializa el Gestor de Memoria Física.
 * Lee el mapa de memoria E820, calcula la RAM total y configura el bitmap.
 */
void pmm_init(void);

/**
 * Asigna una página física libre (4 KB).
 * @return Dirección física de la página, o 0 si no hay memoria.
 */
void* pmm_alloc_page(void);

/**
 * Libera una página física.
 * @param addr Dirección física de la página a liberar.
 */
void pmm_free_page(void* addr);

/**
 * Incrementa el contador de referencias de una página física.
 * Usado para Copy-on-Write y memoria compartida.
 */
void pmm_ref_page(void* addr);

/**
 * Decrementa el contador de referencias.
 * Alias de pmm_free_page (que ahora maneja refcounting).
 */
void pmm_unref_page(void* addr);

/**
 * Obtiene el contador de referencias actual de una página.
 */
uint8_t pmm_get_ref_count(void* addr);

/**
 * Marca un rango de memoria física como OCUPADO.
 * Usado por el Heap (mm_init) para evitar que el PMM asigne páginas dentro del heap.
 */
void pmm_mark_region_used(uint64_t base, uint64_t size);

/**
 * Obtener estadísticas
 */
uint64_t pmm_get_total_ram(void);
uint64_t pmm_get_free_ram(void);
uint64_t pmm_get_used_ram(void);

#endif /* ETEROS_PMM_H */
