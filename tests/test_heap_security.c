#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#define __ETEROS_HOST_TEST__
#include <assert.h>
#define ASSERT(x) do {} while(0)

/* Mock IRQ functions for spinlock support */
uint64_t irq_save(void) { return 0; }
void pmm_mark_region_used(uint64_t addr, size_t count) {}
void irq_restore(uint64_t flags) { (void)flags; }

/* Mock for _kernel_end */
uint8_t _kernel_end = 0;

/* Include headers that heap.c needs */
#include "../include/types.h"
#include "../include/string.h"
#include "../include/mm.h"
#include "../include/serial.h"
#include "../include/pmm.h"
#include "../include/boot.h"

/* Mock dependencies */
void serial_write_string(const char* str) {
    printf("[SERIAL] %s", str);
}

void itoa_s(int64_t value, char* buffer, size_t buffer_size, int base) {
    if (base == 10) snprintf(buffer, buffer_size, "%lld", (long long)value);
    else if (base == 16) snprintf(buffer, buffer_size, "%llx", (long long)value);
    else snprintf(buffer, buffer_size, "%lld", (long long)value);
}

void utoa_hex_s(uint64_t value, char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%llx", (unsigned long long)value);
}

/* We need to implement eteros_ functions because string.h renames them */
void* eteros_memcpy(void* dest, const void* src, size_t n) { return memcpy(dest, src, n); }
void* eteros_memset(void* s, int c, size_t n) {
    return memset(s, c, n);
}
/* heap.c doesn't use other string functions, but let's implement just in case */

/* Now include the file under test */
/* We include the .c file to access static variables and internal structures */
#include "../kernel/mm/heap.c"

/* Test Globals */
#define TEST_HEAP_SIZE (1024 * 1024)
uint8_t test_heap_memory[TEST_HEAP_SIZE];

/* Setup function to initialize heap manually for testing */
void setup_test_heap() {
    mm_initialized = true;
    heap_start = (block_header_t*)test_heap_memory;
    memory_total = TEST_HEAP_SIZE;
    memory_used = 0;

    /* Initialize the first block */
    heap_start->size = memory_total - sizeof(block_header_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    add_to_free_list(heap_start);
    /* We expect magic to be set here in the fix */
#ifdef HEAP_MAGIC
    heap_start->magic = HEAP_MAGIC;
#endif
}

void test_kmalloc_sets_magic() {
    printf("Running test_kmalloc_sets_magic...\n");
    setup_test_heap();

    void* ptr = kmalloc(128);
    assert(ptr != NULL);

    /* Get the block header */
    block_header_t* block = (block_header_t*)((uintptr_t)ptr - sizeof(block_header_t));

#ifdef HEAP_MAGIC
    assert(block->magic == HEAP_MAGIC);
    printf("  Verified magic number is present on allocated block.\n");
#else
    printf("  HEAP_MAGIC not defined yet (expected before fix).\n");
#endif

    /* Check split block if any */
    if (block->next) {
#ifdef HEAP_MAGIC
        assert(block->next->magic == HEAP_MAGIC);
        printf("  Verified magic number is present on split free block.\n");
#endif
    }
}

void test_kfree_checks_magic() {
    printf("Running test_kfree_checks_magic...\n");
    setup_test_heap();

    void* ptr = kmalloc(128);
    assert(ptr != NULL);

    block_header_t* block = (block_header_t*)((uintptr_t)ptr - sizeof(block_header_t));

    /* Case 1: Valid free */
    kfree(ptr);
    /* Should verify coalescing, but here we just check no crash */
    /* And block is now free */
    assert(block->is_free == 1);
    printf("  Valid kfree passed.\n");

    /* Case 2: Invalid magic */
    /* Re-allocate */
    ptr = kmalloc(128);
    block = (block_header_t*)((uintptr_t)ptr - sizeof(block_header_t));

    /* Corrupt magic */
#ifdef HEAP_MAGIC
    block->magic = 0xDEADBEEF;

    /* Redirect stdout to capture error message?
       For simplicity, we assume serial_write_string prints to stdout and we watch it.
       Or we can mock serial_write_string to set a flag.
    */

    kfree(ptr);

    /* Should NOT be free because magic check failed */
    /* Wait, if magic check fails, it returns early. So is_free remains 0. */
    assert(block->is_free == 0);
    printf("  Invalid magic detected (block not freed).\n");

#endif
}

int main() {
    test_kmalloc_sets_magic();
    test_kfree_checks_magic();


    printf("All tests passed!\n");
    return 0;
}
