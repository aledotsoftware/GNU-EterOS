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
    fs_node_t* root = initialise_initrd((uint64_t)image, sizeof(image));
    assert(root != NULL);

    // Find the file
    fs_node_t* file = initrd_finddir(root, "exploit.txt");
    assert(file != NULL);

    // Try to read
    uint8_t buffer[100];
    memset(buffer, 0xAA, sizeof(buffer));

    uint32_t read_size = initrd_read(file, 0, 100, buffer);

    if (read_size != 0) {
        printf("VULNERABILITY CONFIRMED: Read %d bytes from out-of-bounds offset!\n", read_size);
        exit(1);
    }

    printf("PASSED: Read correctly blocked.\n");
}

void test_initrd_name_overflow() {
    printf("Running test_initrd_name_overflow...\n");
    heap_idx = 0;

    static uint8_t image[4096];
    memset(image, 0, sizeof(image));

    initrd_header_t* header = (initrd_header_t*)image;
    memcpy(header->magic, "INIT", 4);
    header->count = 1;

    initrd_file_header_t* file_header = (initrd_file_header_t*)(image + sizeof(initrd_header_t));

    // Fill name with 64 'A's (no null terminator)
    memset(file_header->name, 'A', 64);

    // Fill adjacent memory (size/offset fields) with 'B's to simulate overflow
    // size (4 bytes) + offset (4 bytes) = 8 bytes
    memset(&file_header->size, 'B', 4);
    memset(&file_header->offset, 'B', 4);

    // Initialize
    fs_node_t* root = initialise_initrd((uint64_t)image, sizeof(image));
    assert(root != NULL);

    // 1. Test Leakage via Finddir (strcmp overflow)
    // We search for "A"*64 + "B"*8.
    // If initrd_finddir uses strcmp, it will read past the 64 'A's in header->name
    // and match against the 'B's in size/offset fields.
    char malicious_name[100];
    memset(malicious_name, 'A', 64);
    memset(malicious_name + 64, 'B', 8);
    malicious_name[72] = '\0';

    fs_node_t* file = initrd_finddir(root, malicious_name);

    if (file != NULL) {
        printf("VULNERABILITY CONFIRMED: Found file using name extended with adjacent memory!\n");
        printf("  Found name: %s\n", file->name);

        // Also check if fnode->name contains leaked data
        if (strlen(file->name) > 64) {
             printf("  fnode->name has leaked data (len=%zu)\n", strlen(file->name));
        }
        exit(1);
    }

    // 2. Test Leakage via Readdir (strlcpy overflow)
    // Even if finddir is fixed, readdir might leak into dirent->name
    struct dirent entry;
    // Index 0,1,2 are virtual (dev, proc, sys). Index 3 is our file.
    if (initrd_readdir(root, 3, &entry) == 0) {
        if (strlen(entry.name) > 64) {
            printf("VULNERABILITY CONFIRMED: Readdir leaked data into dirent->name (len=%zu)!\n", strlen(entry.name));
            exit(1);
        }
    }

    printf("PASSED: Did not find file with over-read name and no leakage in readdir.\n");
}

int main() {
    test_initrd_oob_read();
    test_initrd_name_overflow();
    return 0;
}
