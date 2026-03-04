#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

/* Mock kmalloc */
void* kmalloc(size_t size) {
    return malloc(size);
}

void kfree(void* ptr) {
    free(ptr);
}

/* Include source under test */
#include "../kernel/fs/bcache.c"

void test_bcache_write_new_entry(void) {
    printf("Running test_bcache_write_new_entry...\n");
    bcache_init();

    uint32_t vol_id = 1;
    uint32_t sector = 100;
    uint8_t write_buf[512];
    memset(write_buf, 0xAA, 512);

    bcache_write(vol_id, sector, write_buf);

    uint8_t read_buf[512];
    memset(read_buf, 0, 512);

    int res = bcache_read(vol_id, sector, read_buf);
    if (res != 0) {
        printf("FAILED: bcache_read returned %d\n", res);
        exit(1);
    }

    if (memcmp(write_buf, read_buf, 512) != 0) {
        printf("FAILED: bcache_read returned incorrect data\n");
        exit(1);
    }

    // Verify properties
    int found = 0;
    for (int i = 0; i < BCACHE_SIZE; i++) {
        if (bcache[i].valid && bcache[i].volume_id == vol_id && bcache[i].sector == sector) {
            found = 1;
            break;
        }
    }
    if (!found) {
        printf("FAILED: Entry not found in cache directly\n");
        exit(1);
    }

    printf("PASSED\n");
}

void test_bcache_write_update_existing(void) {
    printf("Running test_bcache_write_update_existing...\n");
    bcache_init();

    uint32_t vol_id = 2;
    uint32_t sector = 200;
    uint8_t write_buf[512];

    // Write initially
    memset(write_buf, 0xBB, 512);
    bcache_write(vol_id, sector, write_buf);

    // Write again with different data
    memset(write_buf, 0xCC, 512);
    bcache_write(vol_id, sector, write_buf);

    uint8_t read_buf[512];
    memset(read_buf, 0, 512);

    int res = bcache_read(vol_id, sector, read_buf);
    if (res != 0) {
        printf("FAILED: bcache_read returned %d\n", res);
        exit(1);
    }

    if (memcmp(write_buf, read_buf, 512) != 0) {
        printf("FAILED: bcache_read returned incorrect updated data\n");
        exit(1);
    }

    // Verify it didn't create a new entry (by counting valid entries)
    int valid_count = 0;
    for (int i = 0; i < BCACHE_SIZE; i++) {
        if (bcache[i].valid) valid_count++;
    }
    if (valid_count != 1) {
        printf("FAILED: bcache updated created new entry instead of updating (count = %d)\n", valid_count);
        exit(1);
    }

    printf("PASSED\n");
}

void test_bcache_write_lru_eviction(void) {
    printf("Running test_bcache_write_lru_eviction...\n");
    bcache_init();

    uint8_t write_buf[512];

    // Fill the cache
    for (int i = 0; i < BCACHE_SIZE; i++) {
        memset(write_buf, i, 512);
        bcache_write(1, i, write_buf);
    }

    // Access entry 0 to make it recently used
    uint8_t read_buf[512];
    bcache_read(1, 0, read_buf);

    // Access entry 1 to make it recently used
    bcache_read(1, 1, read_buf);

    // Now write a new entry, it should evict the oldest (entry 2, since 0 and 1 were just accessed)
    memset(write_buf, 0xFF, 512);
    bcache_write(1, BCACHE_SIZE, write_buf); // Sector BCACHE_SIZE is new

    // Check that entry 2 is gone (was the oldest untouched)
    if (bcache_read(1, 2, read_buf) == 0) {
        printf("FAILED: bcache did not evict LRU entry (entry 2 still found)\n");
        exit(1);
    }

    // Check that new entry is present
    if (bcache_read(1, BCACHE_SIZE, read_buf) != 0) {
        printf("FAILED: bcache new entry not found after eviction\n");
        exit(1);
    }

    // Check that entry 0 and 1 are still present
    if (bcache_read(1, 0, read_buf) != 0 || bcache_read(1, 1, read_buf) != 0) {
        printf("FAILED: bcache evicted recently used entries\n");
        exit(1);
    }

    printf("PASSED\n");
}

void test_bcache_write_null_cache(void) {
    printf("Running test_bcache_write_null_cache...\n");

    // Make sure cache is NULL
    bcache = NULL;

    uint8_t write_buf[512];
    memset(write_buf, 0xDD, 512);

    // Should not crash
    bcache_write(1, 100, write_buf);

    printf("PASSED\n");
}


int main(void) {
    test_bcache_write_new_entry();
    test_bcache_write_update_existing();
    test_bcache_write_lru_eviction();
    test_bcache_write_null_cache();
    printf("All bcache tests passed!\n");
    return 0;
}
