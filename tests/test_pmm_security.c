#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <string.h>
/* Undefine string macros to avoid recursion if include/string.h was picked up */
#ifdef memset
#undef memset
#endif
#ifdef memcpy
#undef memcpy
#endif
#ifdef strcmp
#undef strcmp
#endif
#ifdef strlen
#undef strlen
#endif

#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

/* Undefine macros from include/stdio.h if we picked it up */
#ifdef snprintf
#undef snprintf
#endif
#ifdef vsnprintf
#undef vsnprintf
#endif

/* Capture standard library functions before they are renamed */
static void* (*std_memset)(void*, int, size_t) = memset;
static void* (*std_memcpy)(void*, const void*, size_t) = memcpy;
static int (*std_strcmp)(const char*, const char*) = strcmp;
static size_t (*std_strlen)(const char*) = strlen;

/* Undefine PAGE_SIZE if defined by system headers to avoid conflict with pmm.h */
#ifdef PAGE_SIZE
#undef PAGE_SIZE
#endif

/* Include kernel headers */
#include "../include/string.h"
#include "../include/pmm.h"
#include "../include/vga.h"

/* Implement string functions */
void* eteros_memset(void* dest, int c, size_t n) { return std_memset(dest, c, n); }
void* eteros_memcpy(void* dest, const void* src, size_t n) { return std_memcpy(dest, src, n); }
int eteros_strcmp(const char* s1, const char* s2) { return std_strcmp(s1, s2); }
size_t eteros_strlen(const char* s) { return std_strlen(s); }

void itoa_s(int64_t value, char* buffer, size_t buffer_size, int base) {
    if (base == 10) snprintf(buffer, buffer_size, "%lld", (long long)value);
    else if (base == 16) snprintf(buffer, buffer_size, "%llx", (long long)value);
    else snprintf(buffer, buffer_size, "?");
}

void utoa_hex_s(uint64_t value, char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%llx", (unsigned long long)value);
}

/* Mock Kernel/Hardware functions */
void serial_write_string(const char* str) {
    /* printf("[SERIAL] %s", str); */
    (void)str;
}

void terminal_write_string(const char* str) {
    /* printf("[TERMINAL] %s", str); */
    (void)str;
}

void terminal_write_colored(const char* str, vga_color_t fg, vga_color_t bg) {
    /* printf("[TERMINAL COLOR] %s", str); */
    (void)str;
    (void)fg;
    (void)bg;
}

void bcache_invalidate_all(void) {}

/* Mock memory map and PMM buffers */
struct memory_map* host_mem_map;
/* 1MB buffers enough for 32GB RAM representation */
uint8_t* mock_pmm_bitmap;
uint16_t* mock_pmm_ref_counts;

/* Define _kernel_end mock */
uint8_t _kernel_end = 0;

/* Include PMM implementation */
#include "../kernel/mm/pmm.c"

void test_pmm_initialization() {
    printf("Running test_pmm_initialization... ");

    /* Setup mock memory map */
    host_mem_map = (struct memory_map*)malloc(sizeof(struct memory_map) + sizeof(struct e820_entry) * 2);
    if (!host_mem_map) { printf("Failed to malloc host_mem_map\n"); exit(1); }
    host_mem_map->entry_count = 1;
    host_mem_map->entries[0].base_addr = 0x100000; /* 1MB */
    host_mem_map->entries[0].length = 0x4000000;   /* 64MB */
    host_mem_map->entries[0].type = E820_USABLE;
    printf("host_mem_map setup done\n");

    /* Setup buffers */
    /* 64MB RAM + 1MB base = ~65MB. ~16k pages.
       Bitmap: 16k / 8 = 2KB. Refcounts: 16k * 2 = 32KB.
       Allocate plenty. */
    mock_pmm_bitmap = (uint8_t*)malloc(1024 * 1024);
    if (!mock_pmm_bitmap) { printf("Failed to malloc mock_pmm_bitmap\n"); exit(1); }
    mock_pmm_ref_counts = (uint16_t*)malloc(1024 * 1024 * sizeof(uint16_t));
    if (!mock_pmm_ref_counts) { printf("Failed to malloc mock_pmm_ref_counts\n"); exit(1); }

    /* Reset buffers */
    memset(mock_pmm_bitmap, 0, 1024*1024);
    memset(mock_pmm_ref_counts, 0, 1024*1024*sizeof(uint16_t));
    printf("buffers setup done\n");
    fflush(stdout);

    /* Run init */
    printf("Calling pmm_init...\n");
    fflush(stdout);
    pmm_init();
    printf("pmm_init returned\n");
    fflush(stdout);

    /* Check total pages calculation */
    /* Total RAM should be 0x100000 + 0x4000000 = 0x4100000 (65MB) */
    /* Total pages = 0x4100000 / 4096 = 16640 */
    assert(total_pages == 0x4100000 / 4096);

    /* Verify initialization of refcounts (should be 1 for used regions, 1 for free regions initially,
       but pmm_mark_region_free doesn't change refcounts, so all should be 1?
       Wait, pmm_init:
       1. memset bitmap 0xFF (used)
       2. loop refcounts = 1 (used)
       3. pmm_mark_region_free (unset bitmap bits)

       So yes, refcounts should all be 1.
    */
    for (uint64_t i = 0; i < total_pages; i++) {
        assert(pmm_ref_counts[i] == 1);
    }

    printf("PASSED\n");
}

void test_pmm_ref_increment() {
    printf("Running test_pmm_ref_increment... ");

    /* Pick a page index that is valid */
    uint64_t test_page_idx = 100;
    void* addr = (void*)(test_page_idx * PAGE_SIZE);

    /* Current refcount should be 1 from init */
    assert(pmm_get_ref_count(addr) == 1);

    /* Increment */
    pmm_ref_page(addr);
    assert(pmm_get_ref_count(addr) == 2);

    /* Increment again */
    pmm_ref_page(addr);
    assert(pmm_get_ref_count(addr) == 3);

    printf("PASSED\n");
}

void test_pmm_ref_overflow_saturation() {
    printf("Running test_pmm_ref_overflow_saturation... ");

    uint64_t test_page_idx = 200;
    void* addr = (void*)(test_page_idx * PAGE_SIZE);

    /* Manually set refcount to max - 1 */
    pmm_ref_counts[test_page_idx] = 65534;

    assert(pmm_get_ref_count(addr) == 65534);

    /* Increment to max */
    pmm_ref_page(addr);
    assert(pmm_get_ref_count(addr) == 65535);

    /* Increment beyond max - should saturate */
    pmm_ref_page(addr);
    assert(pmm_get_ref_count(addr) == 65535);

    /* Verify other pages are unaffected */
    assert(pmm_ref_counts[test_page_idx + 1] == 1);

    printf("PASSED\n");
}

void test_pmm_free_decrement() {
    printf("Running test_pmm_free_decrement... ");

    uint64_t test_page_idx = 300;
    void* addr = (void*)(test_page_idx * PAGE_SIZE);

    /* Set refcount to 2 */
    pmm_ref_counts[test_page_idx] = 2;

    /* Free page (decrement) */
    pmm_free_page(addr);
    assert(pmm_get_ref_count(addr) == 1);
    /* Should still be marked used in bitmap (we didn't set bitmap for this test but let's assume it was set) */

    /* Free again */
    pmm_free_page(addr);
    /* Now refcount should be 0 */
    assert(pmm_get_ref_count(addr) == 0);

    /* Free again (should stay 0 and not underflow) */
    /* pmm_free_page checks if refcount > 0 before decrementing */
    pmm_free_page(addr);
    assert(pmm_get_ref_count(addr) == 0);

    printf("PASSED\n");
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);

    test_pmm_initialization();
    test_pmm_ref_increment();
    test_pmm_ref_overflow_saturation();
    test_pmm_free_decrement();

    /* Cleanup */
    free(host_mem_map);
    free(mock_pmm_bitmap);
    free(mock_pmm_ref_counts);

    printf("\nAll PMM Security tests passed!\n");
    return 0;
}
