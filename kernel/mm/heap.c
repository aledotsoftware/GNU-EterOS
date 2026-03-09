/**
 * éterOS - Simple Memory Allocator (Explicit Free List + Coalescing)
 * 
 * Implementación de un gestor de memoria dinámica simple.
 * Usa una lista doblemente enlazada de bloques libres (Explicit Free List).
 * El heap vive en la región identity-mapped del bootloader (0-4GB).
 */

#include "../../include/types.h"
#include "../../include/string.h"
#include "../../include/mm.h"
#include "../../include/serial.h"
#include "../../include/pmm.h" /* Para PAGE_ALIGN_UP y PAGE_SIZE */
#include <assert.h>
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
#define HEAP_FOOTER_MAGIC 0x464F4F54 /* "FOOT" */

/* Estructura de cabecera de bloque */
typedef struct block_header {
    size_t size;
    uint32_t magic;
    uint8_t is_free;
    struct block_header* next;
    struct block_header* prev;
} block_header_t;

/* Node for the explicit free list, stored inside the free block payload */
typedef struct free_node {
    struct block_header* next_free;
    struct block_header* prev_free;
} free_node_t;

static block_header_t* heap_start = NULL;
#define NUM_BUCKETS 10
static block_header_t* free_buckets[NUM_BUCKETS] = {NULL}; /* Heads of the segregated free lists */
static size_t memory_used = 0;
static size_t memory_total = 0;

/* Spinlock para proteger el heap en entornos SMP */
static spinlock_t heap_lock = 0;
bool mm_initialized = false;

/* ========================================================================= */
/* Helpers                                                                   */
/* ========================================================================= */

static size_t align(size_t n) {
    size_t aligned = (n + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
    /* Minimum allocation size must fit free_node_t to support the free list */
    if (aligned < sizeof(free_node_t)) {
        aligned = sizeof(free_node_t);
        /* Re-align if sizeof(free_node_t) is weird, though it should be 16 bytes on 64-bit */
        aligned = (aligned + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
    }
    return aligned;
}

static inline int get_bucket_index(size_t size) {
    if (size <= 32) return 0;
    if (size <= 64) return 1;
    if (size <= 128) return 2;
    if (size <= 256) return 3;
    if (size <= 512) return 4;
    if (size <= 1024) return 5;
    if (size <= 2048) return 6;
    if (size <= 4096) return 7;
    if (size <= 8192) return 8;
    return 9;
}

static free_node_t* get_free_node(block_header_t* block) {
    return (free_node_t*)((uintptr_t)block + sizeof(block_header_t));
}

static void add_to_free_list(block_header_t* block) {
    if (!block || !block->is_free) return;

    int bucket = get_bucket_index(block->size);
    free_node_t* node = get_free_node(block);

    node->next_free = free_buckets[bucket];
    node->prev_free = NULL;

    if (free_buckets[bucket]) {
        free_node_t* head_node = get_free_node(free_buckets[bucket]);
        head_node->prev_free = block;
    }

    free_buckets[bucket] = block;
}

static void remove_from_free_list(block_header_t* block) {
    if (!block || !block->is_free) return;

    int bucket = get_bucket_index(block->size);
    free_node_t* node = get_free_node(block);

    if (node->prev_free) {
        free_node_t* prev_node = get_free_node(node->prev_free);
        prev_node->next_free = node->next_free;
    } else {
        free_buckets[bucket] = node->next_free;
    }

    if (node->next_free) {
        free_node_t* next_node = get_free_node(node->next_free);
        next_node->prev_free = node->prev_free;
    }

    node->next_free = NULL;
    node->prev_free = NULL;
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
    for (int i = 0; i < NUM_BUCKETS; i++) {
        free_buckets[i] = NULL;
    }
    memory_total = best_size;

    /* Configurar bloque inicial */
    heap_start->size = memory_total - sizeof(block_header_t);
    heap_start->is_free = 1;
    heap_start->magic = HEAP_MAGIC;
    heap_start->next = NULL;
    heap_start->prev = NULL;

    /* Add initial block to free list */
    add_to_free_list(heap_start);

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
    if (!mm_initialized) return NULL;

    /* Overflow check: prevent align() wrap-around and excessive size */
    if (size > SIZE_MAX - HEAP_ALIGNMENT - sizeof(block_header_t)) {
        return NULL;
    }

    /* Safety check: prevent allocation larger than total heap size */
    if (size > memory_total) {
        return NULL;
    }

    size_t aligned_size = align(size + sizeof(uint32_t)); /* Add space for footer */
    
    /* Segregated Free List Strategy: O(1) mostly */
    int start_bucket = get_bucket_index(aligned_size);

    for (int b = start_bucket; b < NUM_BUCKETS; b++) {
        block_header_t* curr = free_buckets[b];

        while (curr) {
            /* Sanity check */
            if (!curr->is_free) {
                 /* Should not happen if list maintenance is correct */
                 /* Skip or remove? Safest to just move on for now. */
                 curr = get_free_node(curr)->next_free;
                 continue;
            }

            if (curr->size >= aligned_size) {
                /* Found a fit! */

                /* Remove from free list first - we will re-add the remainder if split */
                remove_from_free_list(curr);

                /* Check if we can split */
                /* We need enough space for header + data (aligned) + min free block size (sizeof(free_node_t)) */
                /* align() already ensures minimum size for data is sizeof(free_node_t) */
                /* So we need: size >= aligned_size + sizeof(block_header_t) + sizeof(free_node_t) */

                if (curr->size >= aligned_size + sizeof(block_header_t) + sizeof(free_node_t)) {
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

                    /* Add the new remainder block to the free list */
                    add_to_free_list(new_block);
                }

                curr->is_free = 0;
                memory_used += curr->size + sizeof(block_header_t);

                /* Set footer magic */
                uint32_t* footer = (uint32_t*)((uintptr_t)curr + sizeof(block_header_t) + curr->size - sizeof(uint32_t));
                *footer = HEAP_FOOTER_MAGIC;

                return (void*)((uintptr_t)curr + sizeof(block_header_t));
            }

            curr = get_free_node(curr)->next_free;
        }
    }
    
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
        ASSERT(0 && "Heap corruption detected: Invalid magic number");
        return;
    }

    if (block->is_free) {
        serial_write_string("[MM] Warning: Double free detected\n");
        ASSERT(0 && "Double free detected");
        return;
    }

    /* Check footer magic */
    uint32_t* footer = (uint32_t*)((uintptr_t)block + sizeof(block_header_t) + block->size - sizeof(uint32_t));
    if (*footer != HEAP_FOOTER_MAGIC) {
        serial_write_string("[MM] Error: kfree of invalid address (bad footer magic)\n");
        ASSERT(0 && "Heap corruption detected: Invalid footer magic number");
        return;
    }
    
    block->is_free = 1;
    memory_used -= (block->size + sizeof(block_header_t));

    /* Add to free list initially. If we merge, we might remove it/adjust it. */
    /* Coalescing strategy:
       Check next. If free, REMOVE next from free list, merge into current.
       Check prev. If free, REMOVE current from free list (if we added it? or just don't add),
       merge current into prev. Prev is already in free list.
    */

    /* Merge with next block if free */
    if (block->next && block->next->is_free) {
        /* Remove the next block from free list as it's being absorbed */
        remove_from_free_list(block->next);

        block->size += sizeof(block_header_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    /* Merge with prev block if free */
    if (block->prev && block->prev->is_free) {
        /* Prev is already free, so it should be in the free list.
           We merge 'block' into 'prev'.
           Because the size changes, it might belong to a different bucket now.
           We must remove it from its old bucket and add it back.
        */
        remove_from_free_list(block->prev);

        block->prev->size += sizeof(block_header_t) + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
        add_to_free_list(block->prev);
    } else {
        /* Only add to free list if we didn't merge into a previous block */
        add_to_free_list(block);
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
    /* kmalloc already locks, so this is safe */
    void* ptr = kmalloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void* krealloc(void* ptr, size_t size) {
    if (!ptr) return kmalloc(size);

    uint64_t flags = irq_save();
    spin_lock(&heap_lock);

    if (size == 0) {
        _kfree_impl(ptr);
        spin_unlock(&heap_lock);
        irq_restore(flags);
        return NULL;
    }

    /* Get block header */
    block_header_t* block = (block_header_t*)((uintptr_t)ptr - sizeof(block_header_t));
    
    /* Sanity check */
    if (block->magic != HEAP_MAGIC) {
        serial_write_string("[MM] Error: krealloc of invalid address\n");
        ASSERT(0 && "Heap corruption detected during krealloc");
        spin_unlock(&heap_lock);
        irq_restore(flags);
        return NULL;
    }

    /* If current block is large enough, return it (we don't split for now) */
    /* Alignment is already handled in allocation size, so block->size is usable */
    if (block->size >= size) {
        spin_unlock(&heap_lock);
        irq_restore(flags);
        return ptr;
    }

    /* Allocate new block using internal unlocked allocator */
    void* new_ptr = _kmalloc_impl(size);
    if (!new_ptr) {
        spin_unlock(&heap_lock);
        irq_restore(flags);
        return NULL;
    }

    /* Copy old data */
    /* We copy only the data we had (block->size) */
    memcpy(new_ptr, ptr, block->size);

    /* Free old block using internal unlocked deallocator */
    _kfree_impl(ptr);

    spin_unlock(&heap_lock);
    irq_restore(flags);
    return new_ptr;
}

size_t mm_get_total_memory(void) { return memory_total; }
size_t mm_get_used_memory(void) { return memory_used; }
