#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

typedef __builtin_va_list __gnuc_va_list;

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
    // printf("%s", str); // Silence console output for cleaner test run, or keep it for debug
}

/* Mock Memory Management */
static uint8_t heap[1024 * 1024];
static size_t heap_idx = 0;

void* kmalloc(size_t size) {
    if (heap_idx + size > sizeof(heap)) return NULL;
    void* ptr = &heap[heap_idx];
    heap_idx += size;
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

void test_initrd_oob_read() {
    printf("Running test_initrd_oob_read...\n");
    heap_idx = 0;

    // Create a fake initrd image
    // [Header] [FileHeaders...] [Data...]
    // We make it small so we can easily point outside it
    static uint8_t image[4096];
    memset(image, 0, sizeof(image));

    initrd_header_t* header = (initrd_header_t*)image;
    memcpy(header->magic, "INIT", 4);
    header->count = 1;

    initrd_file_header_t* file_header = (initrd_file_header_t*)(image + sizeof(initrd_header_t));
    strlcpy(file_header->name, "exploit.txt", 64);
    file_header->size = 100;
    // Malicious offset! Points to 10000 bytes past start
    file_header->offset = 10000;

    // Initialize
    // Pass size=4096
    fs_node_t* root = initialise_initrd((uint64_t)image, sizeof(image));
    assert(root != NULL);

    // Find the file
    fs_node_t* file = initrd_finddir(root, "exploit.txt");
    assert(file != NULL);

    // Try to read
    uint8_t buffer[100];
    memset(buffer, 0xAA, sizeof(buffer));

    // In vulnerable code:
    // header.offset = 10000
    // offset = 0
    // size = 100
    // header.size = 100
    // Checks:
    // offset(0) > header.size(100) -> false
    // offset(0) + size(100) > header.size(100) -> false
    // memcpy(buffer, start + 10000, 100)
    // This will perform a read from image + 10000.

    // We expect this to return 0 after the fix.
    uint32_t read_size = initrd_read(file, 0, 100, buffer);

    if (read_size != 0) {
        printf("VULNERABILITY CONFIRMED: Read %d bytes from out-of-bounds offset!\n", read_size);
        // We exit with 0 to signal that the reproduction was successful (i.e., the vulnerability exists)
        // Wait, usually tests should fail if vulnerability exists.
        // But for reproduction step, I want to confirm failure.
        // Let's make it fail (exit 1) if vulnerability exists so I can see it clearly.
        exit(1);
    }

    printf("PASSED: Read correctly blocked.\n");
}

int main() {
    test_initrd_oob_read();
    return 0;
}
