#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>

/* Define Host Test Mode */
#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

/* Mock IRQ functions for spinlock support */
uint64_t irq_save(void) { return 0; }
void irq_restore(uint64_t flags) { (void)flags; }

/* Mock Kernel Symbols */
uint8_t _kernel_end = 0;

/* Mock Serial Output */
void serial_write_string(const char* str) {
    /* Enable debug output to trace crash */
    /* printf("%s", str); */
    (void)str;
}

/* Include Headers */
#include "../kernel/string.c"
#include "../kernel/mm/heap.c"

/* Benchmark Configuration */
#define NUM_ALLOCS 20000
#define ALLOC_SIZE 256

void* ptrs[NUM_ALLOCS];

/* Helper to setup heap */
void setup_heap() {
    /* Allocate 32MB of memory in the lower 32-bits */
    size_t heap_size = 32 * 1024 * 1024;
    void* mem = mmap(NULL, heap_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);

    if (mem == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    /* Create a mock boot_info with memory map */
    struct memory_map* map = malloc(sizeof(struct memory_map) + sizeof(struct e820_entry));
    map->entry_count = 1;
    map->entries[0].base_addr = (uint64_t)(uintptr_t)mem;
    map->entries[0].length = heap_size;
    map->entries[0].type = E820_USABLE;

    boot_info_t boot_info;
    memset(&boot_info, 0, sizeof(boot_info));
    boot_info.mem_map_addr = (uint64_t)(uintptr_t)map;

    mm_init(&boot_info);

    free(map);
}

int main() {
    setup_heap();

    /* Verify allocation works */
    void* p = kmalloc(128);
    if (!p) {
        printf("kmalloc(128) returned NULL\n");
        exit(1);
    }
    kfree(p);

    /* Benchmark */
    clock_t start, end;
    double cpu_time_used;

    /* 1. Allocation Phase */
    for (int i = 0; i < NUM_ALLOCS; i++) {
        ptrs[i] = kmalloc(ALLOC_SIZE);
        if (!ptrs[i]) {
            fprintf(stderr, "Allocation failed at %d\n", i);
            exit(1);
        }
    }

    /* 2. Free Phase (Measured) */
    start = clock();

    /* Free in order */
    for (int i = 0; i < NUM_ALLOCS; i++) {
        kfree(ptrs[i]);
    }

    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    printf("Time taken for %d frees: %f seconds\n", NUM_ALLOCS, cpu_time_used);

    /* Verify Heap is Coalesced */
    if (heap_start->is_free && heap_start->next == NULL) {
        printf("Heap verification passed: Single free block.\n");
    } else {
        printf("Heap verification FAILED: Not fully coalesced.\n");
        block_header_t* curr = heap_start;
        int blocks = 0;
        int free_blocks = 0;
        while(curr) {
            blocks++;
            if (curr->is_free) free_blocks++;
            curr = curr->next;
        }
        printf("Total blocks: %d (Free: %d)\n", blocks, free_blocks);
        exit(1);
    }

    return 0;
}
