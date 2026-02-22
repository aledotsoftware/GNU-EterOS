#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

/* Mock types */
#include "../include/types.h"
#include "../include/fs/vfs.h"
#include "../include/mm.h"
#include "../include/hal.h"
#include "../include/fs/initrd.h"

/* Mock HAL Console */
void hal_console_write(const char* str) {
    // printf("%s", str); // Uncomment for debugging if needed
}

/* Mock Memory Management */
static uint8_t heap[1024 * 1024];
static size_t heap_idx = 0;
static int kmalloc_fail = 0;

void* kmalloc(size_t size) {
    if (kmalloc_fail) return NULL;
    if (heap_idx + size > sizeof(heap)) return NULL;
    void* ptr = &heap[heap_idx];
    heap_idx += size;
    // Align to 16 bytes
    size = (size + 15) & ~15;
    heap_idx += size;
    return ptr;
}

void kfree(void* ptr) {
    (void)ptr;
}

/* Mock other FS inits */
fs_node_t* devfs_init(void) { return (fs_node_t*)kmalloc(sizeof(fs_node_t)); }
fs_node_t* procfs_init(void) { return (fs_node_t*)kmalloc(sizeof(fs_node_t)); }

/* Include string implementation */
#include "../kernel/string.c"

/* Include source under test */
#include "../kernel/fs/initrd.c"

void test_initialise_initrd_null_args() {
    printf("Running test_initialise_initrd_null_args...\n");
    // Should fail if address is 0
    assert(initialise_initrd(0, 100) == NULL);
    // Should fail if size is 0
    assert(initialise_initrd(100, 0) == NULL);
    printf("PASSED\n");
}

void test_initialise_initrd_small_size() {
    printf("Running test_initialise_initrd_small_size...\n");
    // Create a buffer smaller than the header
    uint8_t buffer[sizeof(initrd_header_t) - 1];
    memset(buffer, 0, sizeof(buffer));

    // Should fail because size < sizeof(initrd_header_t)
    assert(initialise_initrd((uint64_t)buffer, sizeof(buffer)) == NULL);
    printf("PASSED\n");
}

void test_initialise_initrd_invalid_magic() {
    printf("Running test_initialise_initrd_invalid_magic...\n");
    uint8_t buffer[sizeof(initrd_header_t) + 10];
    initrd_header_t* header = (initrd_header_t*)buffer;

    // Set invalid magic
    memcpy(header->magic, "BAD!", 4);
    header->count = 0;

    assert(initialise_initrd((uint64_t)buffer, sizeof(buffer)) == NULL);
    printf("PASSED\n");
}

void test_initialise_initrd_headers_overflow() {
    printf("Running test_initialise_initrd_headers_overflow...\n");
    // Buffer size enough for header but not for file headers
    uint8_t buffer[sizeof(initrd_header_t) + 10];
    initrd_header_t* header = (initrd_header_t*)buffer;

    memcpy(header->magic, "INIT", 4);
    // Set count so that headers exceed available size
    // We have 10 bytes left after header
    // Each file header is definitely larger than 1 byte (64+4+4 = 72 bytes)
    header->count = 1;

    // sizeof(initrd_header_t) + 1 * sizeof(initrd_file_header_t) > sizeof(buffer)
    // sizeof(buffer) = sizeof(initrd_header_t) + 10
    // sizeof(initrd_file_header_t) is approx 72
    // So this should fail

    assert(initialise_initrd((uint64_t)buffer, sizeof(buffer)) == NULL);
    printf("PASSED\n");
}

void test_initialise_initrd_kmalloc_fail() {
    printf("Running test_initialise_initrd_kmalloc_fail...\n");
    // Valid minimal initrd
    uint8_t buffer[sizeof(initrd_header_t)];
    initrd_header_t* header = (initrd_header_t*)buffer;
    memcpy(header->magic, "INIT", 4);
    header->count = 0;

    // Force kmalloc to fail
    kmalloc_fail = 1;
    assert(initialise_initrd((uint64_t)buffer, sizeof(buffer)) == NULL);

    // Reset for subsequent tests (though this is the last one here)
    kmalloc_fail = 0;

    printf("PASSED\n");
}

int main() {
    test_initialise_initrd_null_args();
    test_initialise_initrd_small_size();
    test_initialise_initrd_invalid_magic();
    test_initialise_initrd_headers_overflow();
    test_initialise_initrd_kmalloc_fail();
    return 0;
}
