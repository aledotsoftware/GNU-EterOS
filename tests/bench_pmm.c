#define __ETEROS_HOST_TEST__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

/* Mock spinlocks to be no-ops for single-threaded benchmark */
#define ETEROS_LOCK_H
typedef int spinlock_t;
static inline void spin_lock(spinlock_t *lock) { (void)lock; }
static inline void spin_unlock(spinlock_t *lock) { (void)lock; }
static inline int spin_trylock(spinlock_t *lock) { (void)lock; return 1; }

/* Mock kernel end */
uint8_t _kernel_end = 0;

/* Implement eteros_* string functions */
void* eteros_memset(void* dest, int c, size_t n) { return memset(dest, c, n); }
void* eteros_memcpy(void* dest, const void* src, size_t n) { return memcpy(dest, src, n); }

void itoa_s(int64_t value, char* buffer, size_t buffer_size, int base) {
    if (base == 10) snprintf(buffer, buffer_size, "%lld", (long long)value);
    else if (base == 16) snprintf(buffer, buffer_size, "%llx", (long long)value);
}
void utoa_hex_s(uint64_t value, char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%llx", (unsigned long long)value);
}

/* Include the file to test */
#include "../kernel/mm/pmm.c"

/* Mock other dependencies - definitions match headers included by pmm.c */
void serial_write_string(const char* str) { (void)str; }
void terminal_write_string(const char* str) { (void)str; }
void terminal_write_colored(const char* str, vga_color_t fg, vga_color_t bg) { (void)str; (void)fg; (void)bg; }
void bcache_invalidate_all(void) {}

#define TOTAL_RAM_MB 1024
#define TOTAL_PAGES ((TOTAL_RAM_MB * 1024 * 1024) / 4096)
#define BITMAP_SIZE (TOTAL_PAGES / 8)

uint8_t* test_bitmap;
uint8_t* test_refcounts;

void setup_test_state() {
    test_bitmap = (uint8_t*)malloc(BITMAP_SIZE + 64);
    test_refcounts = (uint8_t*)malloc(TOTAL_PAGES);

    if (!test_bitmap || !test_refcounts) {
        perror("malloc failed");
        exit(1);
    }

    pmm_bitmap = test_bitmap;
    pmm_ref_counts = test_refcounts;
    total_pages = TOTAL_PAGES;
    pmm_bitmap_size = BITMAP_SIZE;

    memset(test_bitmap, 0, BITMAP_SIZE);
    memset(test_refcounts, 0, TOTAL_PAGES);

    last_free_idx = 0;
    used_ram = 0;

    /* Mark first 1MB used */
    for(int i=0; i<256; i++) {
        bitmap_set(i);
        test_refcounts[i] = 1;
        used_ram += PAGE_SIZE;
    }
}

void teardown_test_state() {
    free(test_bitmap);
    free(test_refcounts);
}

void* allocated_ptrs[TOTAL_PAGES];

int main() {
    setup_test_state();

    printf("Setup: Filling memory...\n");
    int count = TOTAL_PAGES - 256; /* Available pages */
    for (int i = 0; i < count; i++) {
        allocated_ptrs[i] = pmm_alloc_page();
    }

    printf("Setup: Creating fragmentation...\n");
    /* Free every 67th page to create sparse holes */
    int holes = 0;
    for (int i = 0; i < count; i+=67) {
        if (allocated_ptrs[i]) {
            pmm_free_page(allocated_ptrs[i]);
            allocated_ptrs[i] = NULL;
            holes++;
        }
    }
    printf("Created %d holes.\n", holes);

    /* Ensure last_free_idx is misaligned to force scan loop */
    /* Alloc 1 page, if current last_free_idx is aligned? */
    /* We can't easily check internal state, but 67 is prime, so indices should vary. */

    printf("Running benchmark (1000 iterations)...\n");
    clock_t start = clock();

    int iterations = 1000;
    long total_allocs = 0;

    for (int iter = 0; iter < iterations; iter++) {
        /* Fill holes */
        for (int i = 0; i < count; i+=67) {
            if (allocated_ptrs[i] == NULL) {
                allocated_ptrs[i] = pmm_alloc_page();
                if (allocated_ptrs[i]) total_allocs++;
            }
        }

        /* Create holes again (skip last iteration to leave memory full?)
           No, we want to measure allocs, so we must free to repeat.
           But we only measure the WHOLE process (Alloc+Free).
           We can't easily isolate Alloc without more code.
           But optimization affects Alloc only. Free is O(1).
           So if total time improves, it's due to Alloc. */

        if (iter < iterations - 1) {
             for (int i = 0; i < count; i+=67) {
                if (allocated_ptrs[i]) {
                    pmm_free_page(allocated_ptrs[i]);
                    allocated_ptrs[i] = NULL;
                }
            }
        }
    }

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Performed %ld allocations (and frees) in %f seconds\n", total_allocs, time_spent);
    printf("Average per alloc+free cycle: %.9f s\n", time_spent / total_allocs);

    teardown_test_state();
    return 0;
}
