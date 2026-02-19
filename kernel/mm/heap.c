/**
 * éterOS - Simple Memory Allocator (Next-Fit + Coalescing)
 * 
 * Implementación de un gestor de memoria dinámica simple.
 * Usa una lista doblemente enlazada de bloques libres y ocupados.
 * El heap vive en la región identity-mapped del bootloader (0-4GB).
 */

#include "../../include/types.h"
#include "../../include/string.h"
#include "../../include/mm.h"
#include "../../include/serial.h"
#include "../../include/pmm.h" /* Para PAGE_ALIGN_UP y PAGE_SIZE */
#include "../../include/lock.h"
#include "../../include/io.h"

/* ========================================================================= */
/* Configuración del Heap                                                    */
/* ========================================================================= */

/* Alineación de bloques */
#define HEAP_ALIGNMENT   8

/* Dirección donde termina el kernel (símbolo del linker) */
extern uint8_t _kernel_end;

/* Constante de seguridad: Mínimo 1MB para evitar la zona baja reservada */
#define ARCH_RESERVED_MEM_END  0x100000

/* Magic Number para validación de heap */
#define HEAP_MAGIC 0x48454150 /* "HEAP" */

/* Estructura de cabecera de bloque */
typedef struct block_header {
    size_t size;
    uint32_t magic;
    uint8_t is_free;
    struct block_header* next;
    struct block_header* prev;
} block_header_t;

static block_header_t* heap_start = NULL;
static block_header_t* last_alloc = NULL; /* Puntero para Next-Fit strategy */
static size_t memory_used = 0;
static size_t memory_total = 0;

/* Spinlock para proteger el heap en entornos SMP */
static spinlock_t heap_lock = 0;

/* ========================================================================= */
/* Helpers                                                                   */
/* ========================================================================= */

static size_t align(size_t n) {
    return (n + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
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

#define MAX_HEAP_SIZE (96 * 1024 * 1024)

                    /* Usamos la primera región válida que encontremos que sea suficientemente grande */
                    /* (Mínimo 1MB para ser útil) */
                    if (available_size >= 1024 * 1024) {
                        best_start = (uintptr_t)region_start;
                        best_size = (size_t)available_size;
                        
                        /* Limit heap size to keep some physical memory for PMM/Process tables */
                        if (best_size > MAX_HEAP_SIZE) {
                            best_size = MAX_HEAP_SIZE;
                        }
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
    last_alloc = heap_start; /* Inicializar cursor Next-Fit */
    memory_total = best_size;

    /* Configurar bloque inicial */
    heap_start->size = memory_total - sizeof(block_header_t);
    heap_start->is_free = 1;
    heap_start->magic = HEAP_MAGIC;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    memory_used = 0;

    /* Important: We must tell PMM that this memory is now USED by the HEAP */
    pmm_mark_region_used((uint64_t)heap_start, memory_total);
    
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

static void* _kmalloc_impl(size_t size) {
    if (size == 0) return NULL;
    if (!heap_start) return NULL; /* Heap no inicializado */

    /* Overflow check: prevent align() wrap-around and excessive size */
    if (size > SIZE_MAX - HEAP_ALIGNMENT - sizeof(block_header_t)) {
        return NULL;
    }

    /* Safety check: prevent allocation larger than total heap size */
    if (size > memory_total) {
        return NULL;
    }

    size_t aligned_size = align(size);
    
    /* Next Fit Strategy: Start search from last_alloc */
    block_header_t* start_block = (last_alloc) ? last_alloc : heap_start;
    block_header_t* curr = start_block;

    do {
        if (curr->is_free && curr->size >= aligned_size) {
            if (curr->size >= aligned_size + sizeof(block_header_t) + HEAP_ALIGNMENT) {
                block_header_t* new_block = (block_header_t*)((uintptr_t)curr + 
                                            sizeof(block_header_t) + aligned_size);
                new_block->size = curr->size - aligned_size - sizeof(block_header_t);
                new_block->is_free = 1;
                new_block->magic = HEAP_MAGIC;
                new_block->next = curr->next;
                new_block->prev = curr;

                if (new_block->next) {
                    new_block->next->prev = new_block;
                }

                curr->size = aligned_size;
                curr->next = new_block;

                /* Update last_alloc to the new free block (remainder) for better locality next time */
                last_alloc = new_block;
            } else {
                /* Exact fit or close enough */
                /* Update last_alloc to the next block (or wrap around if NULL) */
                last_alloc = curr->next ? curr->next : heap_start;
            }
            
            curr->is_free = 0;
            memory_used += curr->size + sizeof(block_header_t);
            return (void*)((uintptr_t)curr + sizeof(block_header_t));
        }

        curr = curr->next;
        if (!curr) {
            curr = heap_start; /* Wrap around */
        }
    } while (curr != start_block); /* Stop if we've looped back to start */
    
    serial_write_string("[MM] CRITICAL: Out of Memory!\n");
    return NULL;
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    uint64_t flags = irq_save();
    spin_lock(&heap_lock);
    void* ptr = _kmalloc_impl(size);
    spin_unlock(&heap_lock);
    irq_restore(flags);
    return ptr;
}

static void _kfree_impl(void* ptr) {
    if (!ptr) return;
    if (!heap_start) return;

    block_header_t* block = (block_header_t*)((uintptr_t)ptr - sizeof(block_header_t));
    
    /* Validación dinámica de rango */
    if ((void*)block < (void*)heap_start || (void*)block >= (void*)((uintptr_t)heap_start + memory_total)) {
        serial_write_string("[MM] Error: kfree of invalid address\n");
        return;
    }

    /* Verificar Magic Number */
    if (block->magic != HEAP_MAGIC) {
        serial_write_string("[MM] Error: kfree of invalid address (bad magic)\n");
        return;
    }

    if (block->is_free) {
        serial_write_string("[MM] Warning: Double free detected\n");
        return;
    }
    
    block->is_free = 1;
    memory_used -= (block->size + sizeof(block_header_t));

    /* Coalesce O(1) */

    /* Merge with next block if free */
    if (block->next && block->next->is_free) {
        /* If last_alloc points to the block being merged (removed), update it */
        if (last_alloc == block->next) {
            last_alloc = block;
        }

        block->size += sizeof(block_header_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    /* Merge with prev block if free */
    if (block->prev && block->prev->is_free) {
        /* If last_alloc points to the block being merged (removed), update it */
        if (last_alloc == block) {
            last_alloc = block->prev;
        }

        block->prev->size += sizeof(block_header_t) + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

void kfree(void* ptr) {
    if (!ptr) return;
    uint64_t flags = irq_save();
    spin_lock(&heap_lock);
    _kfree_impl(ptr);
    spin_unlock(&heap_lock);
    irq_restore(flags);
}

void* kcalloc(size_t num, size_t size) {
    if (num > 0 && size > SIZE_MAX / num) return NULL;
    size_t total = num * size;
    void* ptr = kmalloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void* krealloc(void* ptr, size_t size) {
    if (!ptr) return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    /* Get block header */
    block_header_t* block = (block_header_t*)((uintptr_t)ptr - sizeof(block_header_t));
    
    /* Sanity check */
    if (block->magic != HEAP_MAGIC) {
        serial_write_string("[MM] Error: krealloc of invalid address\n");
        return NULL;
    }

    /* If current block is large enough, return it (we don't split for now) */
    /* Alignment is already handled in allocation size, so block->size is usable */
    if (block->size >= size) {
        return ptr;
    }

    /* Allocate new block */
    void* new_ptr = kmalloc(size);
    if (!new_ptr) return NULL;

    /* Copy old data */
    /* We copy only the data we had (block->size) or the new size, whichever is smaller? */
    /* Since we are expanding, we copy block->size. */
    memcpy(new_ptr, ptr, block->size);

    /* Free old block */
    kfree(ptr);

    return new_ptr;
}

size_t mm_get_total_memory(void) { return memory_total; }
size_t mm_get_used_memory(void) { return memory_used; }
