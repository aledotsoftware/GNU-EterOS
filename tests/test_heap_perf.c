#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <time.h>

#define CLOCKS_PER_SEC 1000000

#include "../include/types.h"
#include "../include/string.h"
#include "../include/mm.h"
#include "../include/serial.h"
#include "../include/pmm.h"
#include "../include/lock.h"
#include "../include/io.h"

uint64_t irq_save(void) { return 0; }
void irq_restore(uint64_t flags) { (void)flags; }
void pmm_mark_region_used(uint64_t addr, size_t count) {}
uint8_t _kernel_end = 0;
void serial_write_string(const char* str) {}

#include "../kernel/string.c"
#include "../kernel/mm/heap.c"

int main() {
    void* mem = mmap(NULL, 128 * 1024 * 1024, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);

    if (mem == MAP_FAILED) return 1;

    heap_start = (block_header_t*)mem;
    for(int i=0; i<NUM_BUCKETS; i++) free_buckets[i] = NULL;
    memory_total = 128 * 1024 * 1024;
    heap_start->size = memory_total - sizeof(block_header_t);
    heap_start->is_free = 1;
    heap_start->magic = HEAP_MAGIC;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    add_to_free_list(heap_start);
    mm_initialized = true;
    memory_used = 0;

    long start = 0;

    // Intense workload to trigger list search
    #define NUM_ALLOCS 500000
    void* ptrs[NUM_ALLOCS];
    for (int i=0; i<NUM_ALLOCS; i++) {
        ptrs[i] = kmalloc(32);
    }

    // Free many blocks
    for (int i=0; i<NUM_ALLOCS; i+=2) {
        kfree(ptrs[i]);
    }

    // Measure the issue: Linear search in kmalloc
    // Because we are now using segregated free lists, allocating 256 bytes shouldn't scan 32 bytes holes at all!
    void* p[100];
    long search_start = 0;
    for (int j=0; j<500000; j++) {
        for (int i=0; i<100; i++) {
            p[i] = kmalloc(256);
        }
        for (int i=0; i<100; i++) {
            if (p[i]) kfree(p[i]);
        }
    }
    long search_end = 0;

    long end = 0;

    printf("Search time: %f seconds\n", (double)(search_end - search_start) / CLOCKS_PER_SEC);
    printf("Total time: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    return 0;
}
