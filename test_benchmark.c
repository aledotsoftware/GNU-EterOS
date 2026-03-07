#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define __ETEROS_HOST_TEST__
#include "include/types.h"
#include "include/string.h"
#include "include/mm.h"
#include "include/serial.h"
#include "include/pmm.h"
#include "include/lock.h"
#include "include/io.h"

uint64_t irq_save(void) { return 0; }
void irq_restore(uint64_t flags) { (void)flags; }
void pmm_mark_region_used(uint64_t addr, size_t count) {}
uint8_t _kernel_end = 0;
void serial_write_string(const char* str) {}

#include "kernel/string.c"
#include "kernel/mm/heap.c"

#define NUM_ALLOCS 100000

void* ptrs[NUM_ALLOCS];

int main() {
    void* mem = mmap(NULL, 128 * 1024 * 1024, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);

    if (mem == MAP_FAILED) return 1;

    heap_start = (block_header_t*)mem;
    free_list_head = NULL;
    memory_total = 128 * 1024 * 1024;
    heap_start->size = memory_total - sizeof(block_header_t);
    heap_start->is_free = 1;
    heap_start->magic = HEAP_MAGIC;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    add_to_free_list(heap_start);
    memory_used = 0;

    clock_t start = clock();

    // Intense workload to trigger list search
    for (int i=0; i<NUM_ALLOCS; i++) {
        ptrs[i] = kmalloc((i % 128) + 8);
    }

    // Free many blocks
    for (int i=0; i<NUM_ALLOCS; i+=2) {
        kfree(ptrs[i]);
        ptrs[i] = NULL;
    }

    // Ask for blocks that won't fit in the small holes
    // This will force scanning the entire free list to find the big free block at the end
    clock_t search_start = clock();
    for (int i=0; i<200000; i++) {
        void* p = kmalloc(512); // larger than the 8..135 byte holes
        if (p) kfree(p);
    }
    clock_t search_end = clock();

    clock_t end = clock();

    printf("Search time: %f seconds\n", (double)(search_end - search_start) / CLOCKS_PER_SEC);
    printf("Total time: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    return 0;
}
