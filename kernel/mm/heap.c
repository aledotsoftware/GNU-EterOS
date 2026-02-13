/**
 * éterOS - Simple Memory Allocator (First-Fit + Coalescing)
 * 
 * Implementación de un gestor de memoria dinámica simple.
 * Usa una lista enlazada de bloques libres y ocupados.
 * El heap vive en la región identity-mapped del bootloader (0-4GB).
 */

#include "../../include/types.h"
#include "../../include/string.h"
#include "../../include/mm.h"
#include "../../include/serial.h"
#include "../../include/pmm.h" /* Para PAGE_ALIGN_UP y PAGE_SIZE */

/* ========================================================================= */
/* Configuración del Heap                                                    */
/* ========================================================================= */

/* Alineación de bloques */
#define HEAP_ALIGNMENT   8

/* Dirección donde termina el kernel (símbolo del linker) */
extern uint8_t _kernel_end;

/* Constante de seguridad: Mínimo 1MB para evitar la zona baja reservada */
#define ARCH_RESERVED_MEM_END  0x100000

/* Estructura de cabecera de bloque */
typedef struct block_header {
    size_t size;
    uint8_t is_free;
    struct block_header* next;
} block_header_t;

static block_header_t* heap_start = NULL;
static size_t memory_used = 0;
static size_t memory_total = 0;

/* ========================================================================= */
/* Helpers                                                                   */
/* ========================================================================= */

static size_t align(size_t n) {
    return (n + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
}

static void coalesce(void) {
    block_header_t* curr = heap_start;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += sizeof(block_header_t) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

/* ========================================================================= */
/* API Pública                                                               */
/* ========================================================================= */

void mm_init(boot_info_t* boot_info) {
    /* 1. Calcular inicio seguro del heap */
    uintptr_t kernel_end_addr = (uintptr_t)&_kernel_end;
    uintptr_t candidate_addr = PAGE_ALIGN_UP(kernel_end_addr);

    /* Enforzar mínimo de seguridad (1MB) */
    if (candidate_addr < ARCH_RESERVED_MEM_END) {
        candidate_addr = ARCH_RESERVED_MEM_END;
    }

    uintptr_t best_start = 0;
    size_t best_size = 0;

    /* 2. Buscar región válida en el mapa de memoria */
    if (boot_info && boot_info->mem_map_addr) {
        struct memory_map* mem_map = (struct memory_map*)((uintptr_t)boot_info->mem_map_addr);

        for (uint32_t i = 0; i < mem_map->entry_count; i++) {
            struct e820_entry* entry = &mem_map->entries[i];

            if (entry->type == E820_USABLE) {
                uint64_t region_start = entry->base_addr;
                uint64_t region_end = entry->base_addr + entry->length;

                /* Limitar a 4GB (límite del identity map actual) */
                if (region_end > 0x100000000ULL) {
                    region_end = 0x100000000ULL;
                }

                /* Alinear inicio de región a 4KB */
                region_start = PAGE_ALIGN_UP(region_start);

                /* Ajustar inicio de región si se solapa con el área reservada/kernel */
                if (region_start < candidate_addr) {
                    region_start = candidate_addr;
                }

                /* Verificar si queda espacio válido */
                if (region_end > region_start) {
                    uint64_t available_size = region_end - region_start;

                    /* Usamos la primera región válida que encontremos que sea suficientemente grande */
                    /* (Mínimo 1MB para ser útil) */
                    if (available_size >= 1024 * 1024) {
                        best_start = (uintptr_t)region_start;
                        best_size = (size_t)available_size;
                        break; /* Encontramos nuestro heap */
                    }
                }
            }
        }
    }

    /* Fallback si no hay mapa o no se encontró región */
    if (best_start == 0) {
        serial_write_string("[MM] WARNING: No memory map or valid region found. Using fallback.\n");
        /* Fallback: 4MB start, 8MB size (antiguo comportamiento, pero seguro) */
        best_start = 0x00400000;
        /* Asegurarnos que no pisamos el kernel si creció mucho */
        if (best_start < candidate_addr) best_start = candidate_addr;
        /* Asegurar alineación */
        best_start = PAGE_ALIGN_UP(best_start);
        best_size = 8 * 1024 * 1024;
    }

    /* Inicializar variables globales */
    heap_start = (block_header_t*)best_start;
    memory_total = best_size;

    /* Configurar bloque inicial */
    heap_start->size = memory_total - sizeof(block_header_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;
    memory_used = 0;

    /* Log */
    char buf[64];
    serial_write_string("[MM] Heap inicializado dinamicamente.\n");
    serial_write_string("     Start: 0x");
    utoa_hex_s((uint64_t)heap_start, buf, sizeof(buf));
    serial_write_string(buf);
    serial_write_string("\n     Size:  ");
    itoa_s(memory_total / (1024*1024), buf, sizeof(buf), 10);
    serial_write_string(buf);
    serial_write_string(" MB\n");
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    if (!heap_start) return NULL; /* Heap no inicializado */

    size_t aligned_size = align(size);
    block_header_t* curr = heap_start;
    
    while (curr) {
        if (curr->is_free && curr->size >= aligned_size) {
            if (curr->size >= aligned_size + sizeof(block_header_t) + HEAP_ALIGNMENT) {
                block_header_t* new_block = (block_header_t*)((uintptr_t)curr + 
                                            sizeof(block_header_t) + aligned_size);
                new_block->size = curr->size - aligned_size - sizeof(block_header_t);
                new_block->is_free = 1;
                new_block->next = curr->next;
                curr->size = aligned_size;
                curr->next = new_block;
            }
            
            curr->is_free = 0;
            memory_used += curr->size + sizeof(block_header_t);
            return (void*)((uintptr_t)curr + sizeof(block_header_t));
        }
        curr = curr->next;
    }
    
    serial_write_string("[MM] CRITICAL: Out of Memory!\n");
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;
    if (!heap_start) return;

    block_header_t* block = (block_header_t*)((uintptr_t)ptr - sizeof(block_header_t));
    
    /* Validación dinámica de rango */
    if ((void*)block < (void*)heap_start || (void*)block >= (void*)((uintptr_t)heap_start + memory_total)) {
        serial_write_string("[MM] Error: kfree of invalid address\n");
        return;
    }

    if (block->is_free) {
        serial_write_string("[MM] Warning: Double free detected\n");
        return;
    }
    
    block->is_free = 1;
    memory_used -= (block->size + sizeof(block_header_t));
    coalesce();
}

void* kcalloc(size_t num, size_t size) {
    if (num > 0 && size > SIZE_MAX / num) return NULL;
    size_t total = num * size;
    void* ptr = kmalloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

size_t mm_get_total_memory(void) { return memory_total; }
size_t mm_get_used_memory(void) { return memory_used; }
