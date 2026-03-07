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
#include "../../include/fs/bcache.h"
#include "../../include/lock.h"

/* ========================================================================= */
/* Variables Globales                                                        */
/* ========================================================================= */

/* Dirección de inicio del kernel y su fin (definidos en linker.ld) */
extern uint8_t _kernel_end;

/* Ubicación del Bitmap: Lo pondremos después del kernel */
static uint8_t* pmm_bitmap = NULL;
static uint64_t pmm_bitmap_size = 0; /* En bytes */

/* Array de Reference Counts (2 bytes por página) */
static uint16_t* pmm_ref_counts = NULL;

/* Spinlock para proteger el estado del PMM en SMP */
static spinlock_t pmm_lock = 0;

static uint64_t total_ram = 0;
static uint64_t used_ram = 0;
static uint64_t total_pages = 0;

/* Dirección donde termina la memoria usada por el kernel + bitmap + bootloader info */
static uint64_t free_mem_start = 0;

/* Optimization: Rover for Next-Fit allocation strategy */
static uint64_t last_free_idx = 0;

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
    spin_lock(&pmm_lock);
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
    spin_unlock(&pmm_lock);
}

/* Marca un rango de direcciones físicas como LIBRES (si existen en el mapa E820) */
static void pmm_mark_region_free(uint64_t base, uint64_t size) {
    /* Internal helper called by pmm_init (single threaded), but for consistency we lock */
    spin_lock(&pmm_lock);
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
    spin_unlock(&pmm_lock);
}

/* ========================================================================= */
/* API PMM                                                                   */
/* ========================================================================= */

void pmm_init(void) {
    serial_write_string("[PMM] Inicializando Gestor de Memoria Fisica...\n");
    terminal_write_string("[PMM] Inicializando Gestor de Memoria Fisica...\n");

    /* 1. Leer mapa E820 para calcular RAM Total */
#ifdef __ETEROS_HOST_TEST__
    extern struct memory_map* host_mem_map;
    struct memory_map* mem_map = host_mem_map;
#else
    struct memory_map* mem_map = (struct memory_map*)MEM_MAP_ADDR;
#endif
    
    /* Sanity Check básico: count debe ser razonable (>0, <100) */
    char dbg[64];
    serial_write_string("[PMM] E820 entry_count raw = ");
    itoa_s((int)mem_map->entry_count, dbg, sizeof(dbg), 10);
    serial_write_string(dbg);
    serial_write_string("\n");
    if (mem_map->entry_count == 0 || mem_map->entry_count > 128) {
        serial_write_string("[PMM] ERROR: Mapa de memoria invalido!\n");
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

    serial_write_string("[PMM] total_ram = ");
    itoa_s((int)(total_ram / (1024*1024)), dbg, sizeof(dbg), 10);
    serial_write_string(dbg);
    serial_write_string(" MB\n");

    total_pages = total_ram / PAGE_SIZE;
    
    /* Mostrar RAM total detectada */
    char ram_str[32];
    itoa_s(total_ram / (1024*1024), ram_str, sizeof(ram_str), 10);
    terminal_write_string("[PMM] RAM Total Detectada: ");
    terminal_write_string(ram_str);
    terminal_write_string(" MB\n");

    /* 2. Ubicar el Bitmap en una zona segura (4MB) */
    /* ⚡ BOLT: Evitamos colisiones con kernel (1MB), stacks (0.6MB) y tablas (2MB) */
#ifdef __ETEROS_HOST_TEST__
    extern uint8_t* mock_pmm_bitmap;
    extern uint16_t* mock_pmm_ref_counts;

    /* Calcular tamaño del bitmap (1 bit por página) */
    pmm_bitmap_size = total_pages / 8;
    if (total_pages % 8) pmm_bitmap_size++;

    pmm_bitmap = mock_pmm_bitmap;
    pmm_ref_counts = mock_pmm_ref_counts;
#else
    pmm_bitmap = (uint8_t*)0x400000;
    
    /* Calcular tamaño del bitmap (1 bit por página) */
    pmm_bitmap_size = total_pages / 8;
    /* Asegurar que cubra todo (round up) */
    if (total_pages % 8) pmm_bitmap_size++;

    /* Ubicar el RefCounts array después del bitmap (Alineado a 4KB) */
    pmm_ref_counts = (uint16_t*)PAGE_ALIGN_UP((uint64_t)pmm_bitmap + pmm_bitmap_size);
#endif
    
    /* Inicializar bitmap: MARCAR TODO COMO USADO (1) por defecto */
    /* Luego liberaremos solo las regiones USABLE del mapa E820 */
    memset(pmm_bitmap, 0xFF, pmm_bitmap_size);
    /* Inicializar refcounts: MARCAR TODO COMO USADO (1) por defecto */
    /* Use loop since memset sets bytes, and we want uint16_t entries to be 1 */
    for (uint64_t i = 0; i < total_pages; i++) {
        pmm_ref_counts[i] = 1;
    }

    used_ram = total_ram; /* Temporalmente todo usado */
    serial_write_string("[PMM] Bitmap at 0x400000, processing E820 regions...\n");

    /* 3. Procesar mapa E820 y liberar regiones usables */
    for (uint32_t i = 0; i < mem_map->entry_count; i++) {
        struct e820_entry* entry = &mem_map->entries[i];
        if (entry->type == E820_USABLE) {
            pmm_mark_region_free(entry->base_addr, entry->length);
        }
    }

    /* 4. Marcar regiones críticas como OCUPADAS de nuevo */
    
    /* a) Primer 1MB (BIOS, VGA, Stack, Bootloader, Kernel Loader) */
    pmm_mark_region_used(0x0, 0x100000);
    if(!pmm_bitmap) serial_write_string("[PMM] ASSERT FAILED: Bitmap is NULL\n");

    /* b) Kernel (ahora en 1MB+) */
    extern uint8_t _kernel_start, _kernel_end;
    uint64_t k_start = (uint64_t)&_kernel_start;
    uint64_t k_end = (uint64_t)&_kernel_end;
    pmm_mark_region_used(k_start, k_end - k_start);

    /* c) Reservar explícitamente el espacio del Bitmap y RefCounts */
    uint64_t ref_counts_size = total_pages * sizeof(uint16_t);
    uint64_t p_bitmap = (uint64_t)pmm_bitmap;
    uint64_t p_ref_counts = (uint64_t)pmm_ref_counts;
    pmm_mark_region_used(p_bitmap, pmm_bitmap_size);
    pmm_mark_region_used(p_ref_counts, ref_counts_size);
    /* Initrd is loaded at 0x15000 (below 1MB), already covered by 0-1MB reservation. */
    
    uint64_t pmm_end = p_ref_counts + ref_counts_size;
    free_mem_start = PAGE_ALIGN_UP(pmm_end);

    /* Stats Report */
    char free_str[32];
    itoa_s(pmm_get_free_ram() / 1024, free_str, sizeof(free_str), 10);
    terminal_write_string("[PMM] RAM Libre: ");
    terminal_write_string(free_str);
    terminal_write_string(" KB\n");

    serial_write_string("[PMM] Init complete. Free RAM: ");
    serial_write_string(free_str);
    serial_write_string(" KB, total_pages=");
    itoa_s((int)total_pages, free_str, sizeof(free_str), 10);
    serial_write_string(free_str);
    serial_write_string("\n");
}

static void* pmm_alloc_page_impl(void) {
    /* ⚡ BOLT Optimization: Next-Fit Strategy + Word-wise Scanning
     * Instead of linear bit-by-bit search (O(N)), we scan 64 pages at once
     * and start from the last allocated position.
     * Complexity: O(1) Amortized, O(N/64) Worst Case.
     */

    uint64_t word_idx = last_free_idx / 64;
    uint64_t bit_idx = last_free_idx % 64;
    
    /* Calculate total words (rounded up) */
    uint64_t total_words = (total_pages + 63) / 64;
    uint64_t* bitmap_words = (uint64_t*)pmm_bitmap;

    /* Pass 1: Scan from rover to end */
    for (uint64_t i = word_idx; i < total_words; i++) {
        uint64_t word = bitmap_words[i];
        if (word != ~0ULL) { /* If word has at least one free bit (0) */

            /* Logic to skip bits before bit_idx in the first word */
            int start_bit = (i == word_idx) ? bit_idx : 0;

            if (i == word_idx && start_bit > 0) {
                 /* Optimized scan for partial first word using ctzll */
                 /* We want to find the first 0 bit at or after start_bit. */
                 /* Invert word so 0s become 1s. Mask out bits below start_bit. */
                 uint64_t mask = ~((1ULL << start_bit) - 1);
                 uint64_t inv_word = ~word & mask;

                 if (inv_word != 0) {
                     int bit = __builtin_ctzll(inv_word);
                     uint64_t page = i * 64 + bit;
                     if (page >= total_pages) goto check_wrap;

                     bitmap_set(page);
                     if (pmm_ref_counts) pmm_ref_counts[page] = 1;
                     used_ram += PAGE_SIZE;
                     last_free_idx = page + 1;
                 if (last_free_idx >= total_pages) last_free_idx = 0;
                     return (void*)(page * PAGE_SIZE);
                 }
                 continue;
            }

            /* Fast scan using builtin (finds first zero bit) */
            /* __builtin_ctzll finds # of trailing zeros. We want trailing ones if we invert? */
            /* We want the position of the first '0'. */
            /* ~word turns 0s to 1s. ctzll(~word) finds position of first 1 (which was 0). */

            uint64_t inv_word = ~word;
            int bit = __builtin_ctzll(inv_word);

            uint64_t page = i * 64 + bit;
            if (page >= total_pages) goto check_wrap;

            bitmap_set(page);
            if (pmm_ref_counts) pmm_ref_counts[page] = 1;
            used_ram += PAGE_SIZE;
            last_free_idx = page + 1;
                 if (last_free_idx >= total_pages) last_free_idx = 0;
            return (void*)(page * PAGE_SIZE);
        }
    }

check_wrap:
    /* Pass 2: Wrap around from 0 to rover */
    for (uint64_t i = 0; i <= word_idx; i++) {
        uint64_t word = bitmap_words[i];
        if (word != ~0ULL) {
            int end_bit = (i == word_idx) ? bit_idx : 64;

            if (end_bit == 64) {
                 /* Full word scan */
                 uint64_t inv_word = ~word;
                 int bit = __builtin_ctzll(inv_word);
                 uint64_t page = i * 64 + bit;

                 if (page >= total_pages) return NULL;

                 bitmap_set(page);
                 if (pmm_ref_counts) pmm_ref_counts[page] = 1;
                 used_ram += PAGE_SIZE;
                 last_free_idx = page + 1;
                 if (last_free_idx >= total_pages) last_free_idx = 0;
                 return (void*)(page * PAGE_SIZE);
            } else {
                 /* Partial scan (last word of the loop) */
                 for (int bit = 0; bit < end_bit; bit++) {
                     if (!(word & (1ULL << bit))) {
                         uint64_t page = i * 64 + bit;
                         if (page >= total_pages) return NULL;

                         bitmap_set(page);
                         if (pmm_ref_counts) pmm_ref_counts[page] = 1;
                         used_ram += PAGE_SIZE;
                         last_free_idx = page + 1;
                 if (last_free_idx >= total_pages) last_free_idx = 0;
                         return (void*)(page * PAGE_SIZE);
                     }
                 }
            }
        }
    }
    
    /* serial_write_string("[PMM] OOM! No more physical pages.\n"); */
    return NULL;
}

void* pmm_alloc_page(void) {
    if (!pmm_bitmap) {
        serial_write_string("[PMM] FATAL: pmm_alloc_page called before pmm_init!\n");
        __asm__ volatile("cli");
        for(;;) __asm__ volatile("hlt");
    }

    spin_lock(&pmm_lock);
    void* ptr = pmm_alloc_page_impl();
    spin_unlock(&pmm_lock);

    if (ptr) return ptr;

    /* First failure: Try to reclaim memory from caches */
    serial_write_string("[PMM] OOM Warning. Reclaiming caches...\n");
    bcache_invalidate_all();

    /* Retry */
    spin_lock(&pmm_lock);
    ptr = pmm_alloc_page_impl();
    spin_unlock(&pmm_lock);

    if (ptr) return ptr;

    /* Soft Failure: Return NULL and let callers handle it gracefully.
     * All kernel components now check for allocation failures. */
    serial_write_string("[PMM] WARNING: OOM. Returning NULL to caller.\n");

    return NULL;
}

void pmm_free_page(void* addr) {
    spin_lock(&pmm_lock);
    uint64_t page_idx = (uint64_t)addr / PAGE_SIZE;
    
    if (page_idx >= total_pages) {
        spin_unlock(&pmm_lock);
        return;
    }
    
    if (pmm_ref_counts) {
        if (pmm_ref_counts[page_idx] > 0) {
            pmm_ref_counts[page_idx]--;
        }
        if (pmm_ref_counts[page_idx] > 0) {
            /* Still referenced, do not free */
            spin_unlock(&pmm_lock);
            return;
        } else if (pmm_ref_counts[page_idx] == 0) {
            if (!bitmap_test(page_idx)) {
                serial_write_string("[PMM] ERROR: Double free detected!\n");
                spin_unlock(&pmm_lock);
                return;
            }
        }
    }

    if (bitmap_test(page_idx)) {
        bitmap_unset(page_idx);
        used_ram -= PAGE_SIZE;
        /* Note: We do NOT reset last_free_idx here to avoid fragmentation
           and maintain Next-Fit behavior (allocations keep moving forward). */
    }
    spin_unlock(&pmm_lock);
}

void pmm_ref_page(void* addr) {
    spin_lock(&pmm_lock);
    uint64_t page_idx = (uint64_t)addr / PAGE_SIZE;
    if (page_idx >= total_pages) {
        spin_unlock(&pmm_lock);
        return;
    }

    if (pmm_ref_counts) {
        /* Cap at 65535 to prevent overflow */
        if (pmm_ref_counts[page_idx] < 65535) {
            pmm_ref_counts[page_idx]++;
        }
    }
    spin_unlock(&pmm_lock);
}

void pmm_unref_page(void* addr) {
    pmm_free_page(addr);
}

uint16_t pmm_get_ref_count(void* addr) {
    /* Reading refcount without lock might be racy but for checking it's mostly ok.
       Better to lock if exact value needed, but this is usually debug or hint.
       Wait, let's lock to be safe. */
    spin_lock(&pmm_lock);
    uint64_t page_idx = (uint64_t)addr / PAGE_SIZE;
    if (page_idx >= total_pages) {
        spin_unlock(&pmm_lock);
        return 0;
    }
    uint16_t val = pmm_ref_counts ? pmm_ref_counts[page_idx] : 0;
    spin_unlock(&pmm_lock);
    return val;
}

uint64_t pmm_get_total_ram(void) { return total_ram; }
uint64_t pmm_get_free_ram(void) { return total_ram - used_ram; }
uint64_t pmm_get_used_ram(void) { return used_ram; }
