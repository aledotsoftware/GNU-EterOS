#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>

/* Capture standard library functions before they are renamed */
static void* (*std_memset)(void*, int, size_t) = memset;
static void* (*std_memcpy)(void*, const void*, size_t) = memcpy;

/* Undefine PAGE_SIZE if defined by system headers to avoid conflict with pmm.h */
#ifdef PAGE_SIZE
#undef PAGE_SIZE
#endif

/* Mock IRQ functions for spinlock support */
uint64_t irq_save(void) { return 0; }
void irq_restore(uint64_t flags) { (void)flags; }

/* Mock kernel end symbol */
uint8_t _kernel_end = 0;

/* Mock serial functions */
void serial_write_string(const char* str) {
    /* Uncomment for debug output */
    /* printf("[SERIAL] %s", str); */
    (void)str;
}

/* Include kernel headers for types and macros */
#include "../include/string.h"
#include "../include/pmm.h"

/* Implement string functions used by heap.c */
void* eteros_memset(void* dest, int c, size_t n) {
    return std_memset(dest, c, n);
}

void* eteros_memcpy(void* dest, const void* src, size_t n) {
    return std_memcpy(dest, src, n);
}

void itoa_s(int64_t value, char* buffer, size_t buffer_size, int base) {
    if (base == 10) snprintf(buffer, buffer_size, "%lld", (long long)value);
    else if (base == 16) snprintf(buffer, buffer_size, "%llx", (long long)value);
    else snprintf(buffer, buffer_size, "?");
}

void utoa_hex_s(uint64_t value, char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "0x%llx", (unsigned long long)value);
}

/* Mock pmm functions */
void pmm_mark_region_used(uint64_t base, size_t size) {
    (void)base;
    (void)size;
}

/* Include the file under test directly to access static variables */
#include "../kernel/mm/heap.c"

/* Helper to reset heap state */
#define HEAP_SIZE (1024 * 1024)
/* Align heap memory to 16 bytes to ensure block_header_t access is safe */
static uint8_t heap_memory[HEAP_SIZE] __attribute__((aligned(16)));

void reset_heap() {
    // Clear memory
    memset(heap_memory, 0, HEAP_SIZE);

    // Reset global state
    heap_start = (block_header_t*)heap_memory;
    last_alloc = heap_start; /* Reset Next Fit cursor */
    memory_total = HEAP_SIZE;
    memory_used = 0;

    // Initialize first block
    heap_start->size = memory_total - sizeof(block_header_t);
    heap_start->is_free = 1;
    heap_start->magic = HEAP_MAGIC;
    heap_start->next = NULL;
    heap_start->prev = NULL;
}

void test_kmalloc_overflow() {
    printf("Running test_kmalloc_overflow... ");
    reset_heap();

    /* Try to allocate SIZE_MAX (or close to it) */
    void* p = kmalloc(SIZE_MAX);
    assert(p == NULL);

    /* Test slightly less than SIZE_MAX to catch off-by-one in align */
    void* p2 = kmalloc(SIZE_MAX - 7);
    assert(p2 == NULL);

    /* Test allocation larger than total memory */
    void* p3 = kmalloc(memory_total + 1);
    assert(p3 == NULL);

    printf("PASSED\n");
}

void test_kmalloc_basic() {
    printf("Running test_kmalloc_basic... ");
    reset_heap();

    void* p1 = kmalloc(100);
    assert(p1 != NULL);

    // Check alignment
    assert((uintptr_t)p1 % HEAP_ALIGNMENT == 0);

    // Check block header
    block_header_t* header = (block_header_t*)((uintptr_t)p1 - sizeof(block_header_t));
    assert(header->is_free == 0);
    assert(header->size >= 100);

    void* p2 = kmalloc(200);
    assert(p2 != NULL);
    assert(p2 > p1); // Should be after p1

    kfree(p1);
    kfree(p2);

    // Should be fully coalesced back
    assert(heap_start->is_free == 1);
    assert(heap_start->size == memory_total - sizeof(block_header_t));
    assert(memory_used == 0);
    printf("PASSED\n");
}

void test_kmalloc_zero() {
    printf("Running test_kmalloc_zero... ");
    reset_heap();

    void* p = kmalloc(0);
    assert(p == NULL);
    printf("PASSED\n");
}

void test_kfree_null() {
    printf("Running test_kfree_null... ");
    reset_heap();

    // Should not crash
    kfree(NULL);
    printf("PASSED\n");
}

void test_coalescing() {
    printf("Running test_coalescing... ");
    reset_heap();

    // Allocate 3 blocks
    void* p1 = kmalloc(100);
    void* p2 = kmalloc(100);
    void* p3 = kmalloc(100);

    assert(p1 && p2 && p3);

    // Free p2 (middle)
    kfree(p2);

    block_header_t* h1 = (block_header_t*)((uintptr_t)p1 - sizeof(block_header_t));
    block_header_t* h2 = (block_header_t*)((uintptr_t)p2 - sizeof(block_header_t));
    block_header_t* h3 = (block_header_t*)((uintptr_t)p3 - sizeof(block_header_t));

    // Verify p2 is free
    assert(h2->is_free == 1);

    // Free p1 (left) -> should merge with p2
    kfree(p1);

    assert(h1->is_free == 1);

    // Check linkage
    // After merging h1 and h2, h1->next should be h3
    assert(h1->next == h3);

    // Free p3 (right) -> should merge with h1 (which now includes p2)
    kfree(p3);

    assert(h1->size == memory_total - sizeof(block_header_t));
    assert(h1->next == NULL);
    printf("PASSED\n");
}

void test_out_of_memory() {
    printf("Running test_out_of_memory... ");
    reset_heap();

    // Alloc almost everything
    size_t alloc_size = memory_total - sizeof(block_header_t);
    void* p = kmalloc(alloc_size);
    if (!p) {
        // If allocation fails, it might be due to alignment padding or header size mismatch assumptions.
        // But with exact calculation it should fit if no hidden overhead.
        // Let's relax check if needed, but for now strict.
    }
    assert(p != NULL);

    // Try alloc more
    void* p2 = kmalloc(100);
    assert(p2 == NULL);

    kfree(p);
    printf("PASSED\n");
}

void test_double_free() {
    printf("Running test_double_free... ");
    reset_heap();

    void* p = kmalloc(100);
    kfree(p);

    // Double free - should warning but not crash or corrupt
    kfree(p);

    block_header_t* h = (block_header_t*)((uintptr_t)p - sizeof(block_header_t));
    assert(h->is_free == 1);
    printf("PASSED\n");
}

void test_kcalloc() {
    printf("Running test_kcalloc... ");
    reset_heap();

    // Allocate array of ints
    int* arr = (int*)kcalloc(10, sizeof(int));
    assert(arr != NULL);

    // Verify zeroed
    for (int i = 0; i < 10; i++) {
        assert(arr[i] == 0);
    }

    kfree(arr);

    // Test overflow
    void* p = kcalloc(SIZE_MAX / 2 + 1, 4);
    assert(p == NULL);
    printf("PASSED\n");
}

void test_performance() {
    printf("Running test_performance... ");
    reset_heap();

    // Config
    const int N = 2000;
    void* ptrs[N];

    clock_t start = clock();

    // 1. Allocate many small blocks
    for(int i=0; i<N; i++) {
        ptrs[i] = kmalloc(64);
        assert(ptrs[i] != NULL);
    }

    // 2. Free every second block to fragment
    for(int i=0; i<N; i+=2) {
        kfree(ptrs[i]);
        ptrs[i] = NULL;
    }

    // 3. Allocate again (should fill holes efficiently with Next Fit)
    for(int i=0; i<N; i+=2) {
        ptrs[i] = kmalloc(64);
        assert(ptrs[i] != NULL);
    }

    // 4. Cleanup
    for(int i=0; i<N; i++) {
        if(ptrs[i]) kfree(ptrs[i]);
    }

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Done in %f seconds. PASSED\n", time_spent);
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    test_kmalloc_basic();
    test_kmalloc_zero();
    test_kfree_null();
    test_coalescing();
    test_out_of_memory();
    test_double_free();
    test_kcalloc();
    test_kmalloc_overflow();
    test_performance();

    printf("\nAll heap tests passed!\n");
    return 0;
}
