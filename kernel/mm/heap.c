/**
 * éterOS - Simple Memory Allocator (First-Fit + Coalescing)
 * 
 * Implementación de un gestor de memoria dinámica simple.
 * Usa una lista enlazada de bloques libres y ocupados.
 * 
 * Características:
 * - Algoritmo First Fit: Busca el primer bloque libre que quepa.
 * - Coalescencia: Al liberar, fusiona bloques adyacentes libres.
 * - Splitting: Divide bloques grandes en (ocupado + libre restante).
 */

#include "../../include/types.h"
#include "../../include/string.h"
#include "../../include/mm.h"
#include "../../include/serial.h"

/* ========================================================================= */
/* Configuración del Heap                                                    */
/* ========================================================================= */

/* Dirección física fija para el Heap: 4MB */
/* Esto asume que el kernel no crece más allá de 4MB (actualmente ~30KB) */
#define HEAP_START_ADDR  ((void*)0x00400000)
#define HEAP_SIZE        ((size_t)(8 * 1024 * 1024)) /* 8 MB */
#define HEAP_ALIGNMENT   8 /* Alinear a 8 bytes (64-bit word) */

/* Estructura de cabecera de bloque */
typedef struct block_header {
    size_t size;                /* Tamaño de datos útiles (excluyendo header) */
    uint8_t is_free;            /* 1 si está libre, 0 si ocupado */
    struct block_header* next;  /* Puntero al siguiente bloque */ 
} block_header_t;

/* Puntero al inicio de la lista */
static block_header_t* heap_start = (block_header_t*)HEAP_START_ADDR;

/* Estadísticas */
static size_t memory_used = 0;
static size_t memory_total = HEAP_SIZE;

/* ========================================================================= */
/* Helpers Internos                                                          */
/* ========================================================================= */

static void logging(const char* msg) {
    /* Descomentar para debug detallado */
    // serial_write_string("[MM] ");
    // serial_write_string(msg);
    // serial_write_string("\n");
}

/* Alinea un tamaño al múltiplo de HEAP_ALIGNMENT */
static size_t align(size_t n) {
    return (n + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
}

/* Fusiona bloques libres adyacentes */
static void coalesce(void) {
    block_header_t* curr = heap_start;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            /* Fusionar con el siguiente */
            curr->size += sizeof(block_header_t) + curr->next->size;
            curr->next = curr->next->next;
            /* No avanzamos 'curr' para intentar fusionar con el siguiente nuevo */
        } else {
            curr = curr->next;
        }
    }
}

/* ========================================================================= */
/* API Pública                                                               */
/* ========================================================================= */

void mm_init(void) {
    /* Inicializar el primer bloque que cubre todo el heap */
    heap_start->size = HEAP_SIZE - sizeof(block_header_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;
    
    memory_used = 0;
    
    serial_write_string("[MM] Heap inicializado en 0x00400000 (8 MB)\n");
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    size_t aligned_size = align(size);
    block_header_t* curr = heap_start;
    
    while (curr) {
        if (curr->is_free && curr->size >= aligned_size) {
            /* Encontramos un bloque libre suficientemente grande */
            
            /* Verificar si podemos dividir el bloque (Split) */
            if (curr->size >= aligned_size + sizeof(block_header_t) + HEAP_ALIGNMENT) {
                /* Calcular dirección del nuevo bloque */
                block_header_t* new_block = (block_header_t*)((uintptr_t)curr + 
                                            sizeof(block_header_t) + aligned_size);
                
                /* Configurar nuevo bloque */
                new_block->size = curr->size - aligned_size - sizeof(block_header_t);
                new_block->is_free = 1;
                new_block->next = curr->next;
                
                /* Actualizar bloque actual */
                curr->size = aligned_size;
                curr->next = new_block;
            }
            
            curr->is_free = 0;
            memory_used += curr->size + sizeof(block_header_t);
            
            /* Debug log */
            // logging("Allocated block");
            
            /* Retornar puntero a los datos (justo después del header) */
            return (void*)((uintptr_t)curr + sizeof(block_header_t));
        }
        curr = curr->next;
    }
    
    /* Out of Memory */
    serial_write_string("[MM] CRITICAL: Out of Memory!\n");
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    /* Obtener puntero al header (retroceder sizeof(header)) */
    block_header_t* block = (block_header_t*)((uintptr_t)ptr - sizeof(block_header_t));
    
    /* Sanity check básico */
    if ((void*)block < HEAP_START_ADDR || (void*)block >= (void*)((uintptr_t)HEAP_START_ADDR + HEAP_SIZE)) {
        serial_write_string("[MM] Error: kfree of invalid address\n");
        return;
    }

    if (block->is_free) {
        serial_write_string("[MM] Warning: Double free detected\n");
        return;
    }
    
    block->is_free = 1;
    memory_used -= (block->size + sizeof(block_header_t));
    
    /* Intentar fusionar bloques libres para reducir fragmentación */
    coalesce();
}

void* kcalloc(size_t num, size_t size) {
    if (num > 0 && size > SIZE_MAX / num) {
        return NULL;
    }

    size_t total = num * size;
    void* ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

size_t mm_get_total_memory(void) {
    return memory_total;
}

size_t mm_get_used_memory(void) {
    return memory_used;
}
