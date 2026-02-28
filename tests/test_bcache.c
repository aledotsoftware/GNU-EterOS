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

/* Mock memory allocation for bcache */
static uint8_t heap[1024 * 1024];
static size_t heap_idx = 0;

void* kmalloc(size_t size) {
    if (heap_idx + size > sizeof(heap)) return NULL;
    void* ptr = &heap[heap_idx];
    heap_idx += size;
    return ptr;
}

void kfree(void* ptr) {
    // Basic mock, don't need real free for this test
}

void test_bcache_invalidate() {
    printf("Running test_bcache_invalidate...\n");

    bcache_init();

    uint32_t vol_id = 1;
    uint32_t sector = 100;
    uint8_t write_buf[512];
    uint8_t read_buf[512];

    // Initialize buffer with some data
    for (int i = 0; i < 512; i++) {
        write_buf[i] = i % 256;
    }

    // 1. Write to cache
    bcache_write(vol_id, sector, write_buf);

    // 2. Verify it can be read
    int ret = bcache_read(vol_id, sector, read_buf);
    assert(ret == 0);
    assert(memcmp(write_buf, read_buf, 512) == 0);
    printf("  [PASS] Data written and verified in cache.\n");

    // 3. Invalidate it
    bcache_invalidate(vol_id, sector);

    // 4. Verify it's gone
    ret = bcache_read(vol_id, sector, read_buf);
    assert(ret == -1);
    printf("  [PASS] Data successfully invalidated.\n");

    // 5. Check edge cases (invalidating non-existent sector)
    bcache_invalidate(vol_id, sector + 1); // Should not crash
    printf("  [PASS] Invalidating non-existent sector doesn't crash.\n");

    // 6. Check cache can be written again after invalidate
    bcache_write(vol_id, sector, write_buf);
    ret = bcache_read(vol_id, sector, read_buf);
    assert(ret == 0);
    assert(memcmp(write_buf, read_buf, 512) == 0);
    printf("  [PASS] Sector can be rewritten after invalidation.\n");

    // 7. Test invalidate_all
    bcache_write(vol_id, sector + 1, write_buf);
    bcache_invalidate_all();
    ret = bcache_read(vol_id, sector, read_buf);
    assert(ret == -1);
    ret = bcache_read(vol_id, sector + 1, read_buf);
    assert(ret == -1);
    printf("  [PASS] Invalidate all works.\n");
}

int main() {
    test_bcache_invalidate();
    printf("All bcache_invalidate tests passed!\n");
    return 0;
}
