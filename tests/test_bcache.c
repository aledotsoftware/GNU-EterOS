#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

/* Fix for conflict between kernel headers and system headers during test */
typedef __builtin_va_list __gnuc_va_list;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

/* Mock types */
#include "../include/types.h"
#include "../include/fs/bcache.h"
#include "../include/mm.h"
#include "../include/lock.h"

/* Mock HAL Console */
void hal_console_write(const char* str) {
    printf("%s", str);
}

/* Mock Memory Management */
static uint8_t heap[1024 * 1024]; // 1MB heap
static size_t heap_idx = 0;

void* kmalloc(size_t size) {
    // Align to 16 bytes
    size = (size + 15) & ~15;
    if (heap_idx + size > sizeof(heap)) return NULL;
    void* ptr = &heap[heap_idx];
    heap_idx += size;
    return ptr;
}

void kfree(void* ptr) {
    (void)ptr;
}

/* Reset state before each test */
void setup() {
    heap_idx = 0;
    memset(heap, 0, sizeof(heap));
    // Calling bcache_init will re-allocate the bcache array from the "fresh" heap
    // and reset counters/locks.
    bcache_init();
}

void test_bcache_basic() {
    printf("Running test_bcache_basic...\n");
    setup();

    uint32_t vol_id = 1;
    uint32_t sector = 100;
    uint8_t write_buf[512];
    uint8_t read_buf[512];

    // Initialize data
    for (int i = 0; i < 512; i++) write_buf[i] = (uint8_t)i;
    memset(read_buf, 0, 512);

    // 1. Read miss
    int res = bcache_read(vol_id, sector, read_buf);
    if (res != -1) {
        printf("FAILED: Expected read miss (-1), got %d\n", res);
        exit(1);
    }

    // 2. Write to cache
    bcache_write(vol_id, sector, write_buf);

    // 3. Read hit
    res = bcache_read(vol_id, sector, read_buf);
    if (res != 0) {
        printf("FAILED: Expected read hit (0), got %d\n", res);
        exit(1);
    }

    // 4. Verify data
    if (memcmp(write_buf, read_buf, 512) != 0) {
        printf("FAILED: Data mismatch\n");
        exit(1);
    }

    printf("PASSED\n");
}

void test_bcache_update() {
    printf("Running test_bcache_update...\n");
    setup();

    uint32_t vol_id = 1;
    uint32_t sector = 50;
    uint8_t buf1[512];
    uint8_t buf2[512];
    uint8_t read_buf[512];

    memset(buf1, 0xAA, 512);
    memset(buf2, 0xBB, 512);

    // Write initial
    bcache_write(vol_id, sector, buf1);

    // Read and verify
    bcache_read(vol_id, sector, read_buf);
    if (memcmp(read_buf, buf1, 512) != 0) {
        printf("FAILED: Initial write mismatch\n");
        exit(1);
    }

    // Update
    bcache_write(vol_id, sector, buf2);

    // Read and verify update
    bcache_read(vol_id, sector, read_buf);
    if (memcmp(read_buf, buf2, 512) != 0) {
        printf("FAILED: Update write mismatch\n");
        exit(1);
    }

    printf("PASSED\n");
}

void test_bcache_eviction() {
    printf("Running test_bcache_eviction...\n");
    setup();

    // cache size is 128
    int CACHE_SIZE = 128;
    uint32_t vol_id = 1;
    uint8_t buf[512];
    memset(buf, 0, 512);

    // Fill cache: 0 to 127
    for (int i = 0; i < CACHE_SIZE; i++) {
        bcache_write(vol_id, i, buf);
    }

    // Verify all are present
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (bcache_read(vol_id, i, buf) != 0) {
            printf("FAILED: Sector %d missing after fill\n", i);
            exit(1);
        }
    }

    // Access sequence to control LRU:
    // We just read 0..127. So 0 was accessed first, 127 last.
    // 0 is the least recently used among them (assuming access time increased monotonically).
    // Let's make sure 127 is the LRU candidate by accessing 0..126 again.

    for (int i = 0; i < CACHE_SIZE - 1; i++) { // 0 to 126
        bcache_read(vol_id, i, buf);
    }

    // Now 127 has the oldest access time.
    // Write 128 (new sector). It should evict 127.
    bcache_write(vol_id, 128, buf);

    // 127 should be gone.
    if (bcache_read(vol_id, 127, buf) == 0) {
        printf("FAILED: Sector 127 should have been evicted\n");
        exit(1);
    }

    // 128 should be present
    if (bcache_read(vol_id, 128, buf) != 0) {
        printf("FAILED: Sector 128 should be present\n");
        exit(1);
    }

    // 0 should still be present
    if (bcache_read(vol_id, 0, buf) != 0) {
        printf("FAILED: Sector 0 should be present\n");
        exit(1);
    }

    printf("PASSED\n");
}

void test_bcache_invalidate() {
    printf("Running test_bcache_invalidate...\n");
    setup();

    uint32_t vol_id = 1;
    uint32_t sector = 10;
    uint8_t buf[512];

    bcache_write(vol_id, sector, buf);
    if (bcache_read(vol_id, sector, buf) != 0) {
        printf("FAILED: Write failed\n");
        exit(1);
    }

    bcache_invalidate(vol_id, sector);

    if (bcache_read(vol_id, sector, buf) == 0) {
        printf("FAILED: Sector should be invalidated\n");
        exit(1);
    }

    printf("PASSED\n");
}

void test_bcache_invalidate_all() {
    printf("Running test_bcache_invalidate_all...\n");
    setup();

    uint32_t vol_id = 1;
    uint8_t buf[512];

    for (int i = 0; i < 10; i++) {
        bcache_write(vol_id, i, buf);
    }

    bcache_invalidate_all();

    for (int i = 0; i < 10; i++) {
        if (bcache_read(vol_id, i, buf) == 0) {
            printf("FAILED: Sector %d should be invalidated\n", i);
            exit(1);
        }
    }

    printf("PASSED\n");
}

int main() {
    test_bcache_basic();
    test_bcache_update();
    test_bcache_eviction();
    test_bcache_invalidate();
    test_bcache_invalidate_all();
    printf("All bcache tests passed!\n");
    return 0;
}
