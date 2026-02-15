/**
 * éterOS - Physical Memory Manager (PMM)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 * 
 * Gestiona la asignación de frames físicos de 4KB usando un Bitmap.
 * Lee el mapa de memoria E820 proveído por el bootloader.
 */

#include "../../include/pmm.h"
#include "../../include/string.h"
#include "../../include/serial.h"
#include "../../include/vga.h"

/* ========================================================================= */
/* Variables Globales                                                        */
/* ========================================================================= */

/* Dirección de inicio del kernel y su fin (definidos en linker.ld) */
extern uint8_t _kernel_end;

/* Ubicación del Bitmap: Lo pondremos después del kernel */
static uint8_t* pmm_bitmap = NULL;
static uint64_t pmm_bitmap_size = 0; /* En bytes */

/* Array de Reference Counts (1 byte por página) */
static uint8_t* pmm_ref_counts = NULL;

static uint64_t total_ram = 0;
static uint64_t used_ram = 0;
static uint64_t total_pages = 0;

/* Dirección donde termina la memoria usada por el kernel + bitmap + bootloader info */
static uint64_t free_mem_start = 0;

/* ========================================================================= */
/* Helpers de Bitmap                                                         */
/* ========================================================================= */

static void bitmap_set(uint64_t bit) {
    pmm_bitmap[bit / 8] |= (1 << (bit % 8));
}

static void bitmap_unset(uint64_t bit) {
    pmm_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static int bitmap_test(uint64_t bit) {
    return (pmm_bitmap[bit / 8] & (1 << (bit % 8)));
}

/* Marca un rango de direcciones físicas como OCUPADAS */
void pmm_mark_region_used(uint64_t base, uint64_t size) {
    uint64_t start_page = base / PAGE_SIZE;
    uint64_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t page_idx = start_page + i;
        if (page_idx < total_pages) {
            if (!bitmap_test(page_idx)) {
                bitmap_set(page_idx);
                used_ram += PAGE_SIZE;
            }
        }
    }
}

/* Marca un rango de direcciones físicas como LIBRES (si existen en el mapa E820) */
static void pmm_mark_region_free(uint64_t base, uint64_t size) {
    uint64_t start_page = base / PAGE_SIZE;
    uint64_t num_pages = size / PAGE_SIZE; /* Round down for safety */

    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t page_idx = start_page + i;
        if (page_idx < total_pages) {
            if (bitmap_test(page_idx)) {
                bitmap_unset(page_idx);
                used_ram -= PAGE_SIZE;
            }
        }
    }
}

/* ========================================================================= */
/* API PMM                                                                   */
/* ========================================================================= */

void pmm_init(void) {
    terminal_write_string("[PMM] Inicializando Gestor de Memoria Fisica...\n");

    /* 1. Leer mapa E820 para calcular RAM Total */
    struct memory_map* mem_map = (struct memory_map*)MEM_MAP_ADDR;
    
    /* Sanity Check básico: count debe ser razonable (>0, <100) */
    if (mem_map->entry_count == 0 || mem_map->entry_count > 128) {
        terminal_write_colored("[PMM] ERROR: Mapa de memoria invalido!\n", VGA_COLOR_RED, VGA_COLOR_BLACK);
        /* Fallback: Asumir 128MB si falla la detección */
        total_ram = 128 * 1024 * 1024;
    } else {
        /* Calcular máxima dirección física disponible */
        for (uint32_t i = 0; i < mem_map->entry_count; i++) {
            struct e820_entry* entry = &mem_map->entries[i];
            
            /* Debug info a Serial */
            char buf[64];
            serial_write_string("[PMM] Region: Base=0x");
            utoa_hex_s(entry->base_addr, buf, sizeof(buf)); serial_write_string(buf);
            serial_write_string(" Len=0x");
            utoa_hex_s(entry->length, buf, sizeof(buf)); serial_write_string(buf);
            serial_write_string(" Type=");
            itoa_s(entry->type, buf, sizeof(buf), 10); serial_write_string(buf);
            serial_write_string("\n");

            if (entry->type == E820_USABLE) {
                uint64_t region_end = entry->base_addr + entry->length;
                if (region_end > total_ram) {
                    total_ram = region_end;
                }
            }
        }
    }

    if (total_ram == 0) total_ram = 32 * 1024 * 1024; /* Fallback panic prevention */

    total_pages = total_ram / PAGE_SIZE;
    
    /* Mostrar RAM total detectada */
    char ram_str[32];
    itoa_s(total_ram / (1024*1024), ram_str, sizeof(ram_str), 10);
    terminal_write_string("[PMM] RAM Total Detectada: ");
    terminal_write_string(ram_str);
    terminal_write_string(" MB\n");

    /* 2. Ubicar el Bitmap en una zona segura (4MB) */
    /* ⚡ BOLT: Evitamos colisiones con kernel (1MB), stacks (0.6MB) y tablas (2MB) */
    pmm_bitmap = (uint8_t*)0x400000;
    
    /* Calcular tamaño del bitmap (1 bit por página) */
    pmm_bitmap_size = total_pages / 8;
    /* Asegurar que cubra todo (round up) */
    if (total_pages % 8) pmm_bitmap_size++;

    /* Ubicar el RefCounts array después del bitmap (Alineado a 4KB) */
    pmm_ref_counts = (uint8_t*)PAGE_ALIGN_UP((uint64_t)pmm_bitmap + pmm_bitmap_size);
    
    /* Inicializar bitmap: MARCAR TODO COMO USADO (1) por defecto */
    /* Luego liberaremos solo las regiones USABLE del mapa E820 */
    memset(pmm_bitmap, 0xFF, pmm_bitmap_size);
    /* Inicializar refcounts: MARCAR TODO COMO USADO (1) por defecto */
    memset(pmm_ref_counts, 1, total_pages);

    used_ram = total_ram; /* Temporalmente todo usado */

    /* 3. Procesar mapa E820 y liberar regiones usables */
    for (uint32_t i = 0; i < mem_map->entry_count; i++) {
        struct e820_entry* entry = &mem_map->entries[i];
        if (entry->type == E820_USABLE) {
            pmm_mark_region_free(entry->base_addr, entry->length);
        }
    }

    /* 4. Marcar regiones críticas como OCUPADAS de nuevo */
    
    /* a) Primer 1MB (BIOS, VGA, Stack, Bootloader, Kernel) */
    pmm_mark_region_used(0x0, 0x100000); 

    /* b) Reservar explícitamente el espacio del Bitmap y RefCounts */
    uint64_t pmm_end = (uint64_t)pmm_ref_counts + total_pages;
    pmm_mark_region_used((uint64_t)pmm_bitmap, pmm_bitmap_size);
    pmm_mark_region_used((uint64_t)pmm_ref_counts, total_pages);
    
    /* c) Tablas de paginación del bootloader (0x70000-0x76000, ya dentro del 1MB) */

    free_mem_start = PAGE_ALIGN_UP(pmm_end);

    /* Stats Report */
    char free_str[32];
    itoa_s(pmm_get_free_ram() / 1024, free_str, sizeof(free_str), 10);
    terminal_write_string("[PMM] RAM Libre: ");
    terminal_write_string(free_str);
    terminal_write_string(" KB\n");
}

void* pmm_alloc_page(void) {
    /* Algoritmo First-Fit simple en el bitmap */
    /* TODO: Optimizar guardando el último índice libre (rover) */
    
    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            /* Encontrado frame libre */
            bitmap_set(i);
            if (pmm_ref_counts) pmm_ref_counts[i] = 1;
            used_ram += PAGE_SIZE;
            return (void*)(i * PAGE_SIZE);
        }
    }
    
    serial_write_string("[PMM] OOM! No more physical pages.\n");
    return NULL;
}

void pmm_free_page(void* addr) {
    uint64_t page_idx = (uint64_t)addr / PAGE_SIZE;
    
    if (page_idx >= total_pages) return;
    
    if (pmm_ref_counts) {
        if (pmm_ref_counts[page_idx] > 0) {
            pmm_ref_counts[page_idx]--;
        }
        if (pmm_ref_counts[page_idx] > 0) {
            /* Still referenced, do not free */
            return;
        }
    }

    if (bitmap_test(page_idx)) {
        bitmap_unset(page_idx);
        used_ram -= PAGE_SIZE;
    }
}

void pmm_ref_page(void* addr) {
    uint64_t page_idx = (uint64_t)addr / PAGE_SIZE;
    if (page_idx >= total_pages) return;

    if (pmm_ref_counts) {
        /* Cap at 255 to prevent overflow */
        if (pmm_ref_counts[page_idx] < 255) {
            pmm_ref_counts[page_idx]++;
        }
    }
}

void pmm_unref_page(void* addr) {
    pmm_free_page(addr);
}

uint8_t pmm_get_ref_count(void* addr) {
    uint64_t page_idx = (uint64_t)addr / PAGE_SIZE;
    if (page_idx >= total_pages) return 0;
    return pmm_ref_counts ? pmm_ref_counts[page_idx] : 0;
}

uint64_t pmm_get_total_ram(void) { return total_ram; }
uint64_t pmm_get_free_ram(void) { return total_ram - used_ram; }
uint64_t pmm_get_used_ram(void) { return used_ram; }
