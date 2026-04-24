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
    // printf("%s", str); // Silence console output
}

/* Mock Memory Management */
static uint8_t heap[1024 * 1024];
static size_t heap_idx = 0;

void* kmalloc(size_t size) {
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
/* We need to include string.c to get strlcpy, strlen etc. */
/* But since we are testing initrd.c which uses them, we must include them. */
#include "../kernel/string.c"

/* Include source under test */
/* We include the C file directly to test static functions and avoid linking issues */
#include "../kernel/fs/initrd.c"

void test_initrd_readdir_leak() {
    printf("Running test_initrd_readdir_leak...\n");
    heap_idx = 0;

    // Create a fake initrd image
    // [Header] [FileHeader 1] [FileHeader 2] ...
    static uint8_t image[4096];
    memset(image, 0, sizeof(image));

    initrd_header_t* header = (initrd_header_t*)image;
    memcpy(header->magic, "INIT", 4);
    header->count = 2; // Two files

    // File 1: "A" * 64 (No null terminator)
    initrd_file_header_t* file1 = (initrd_file_header_t*)(image + sizeof(initrd_header_t));
    memset(file1->name, 'A', 64); // Fill completely with 'A'

    // Set size/offset to non-null bytes to allow leak to continue
    // 0x42 = 'B'
    file1->size = 0x42424242;
    file1->offset = 0x42424242;

    // File 2: "SECRET_LEAK"
    initrd_file_header_t* file2 = (initrd_file_header_t*)(image + sizeof(initrd_header_t) + sizeof(initrd_file_header_t));
    const char* secret = "SECRET_LEAK";
    // We don't care about file2 structure much, just that it follows file1 immediately in memory
    // But since file_headers is array of structs, it follows immediately.
    memcpy(file2->name, secret, strlen(secret));
    // Let's ensure file2 name is null terminated eventually so strlen stops
    file2->size = 0;
    file2->offset = 0;

    // Initialize
    fs_node_t* root = initialise_initrd((uint64_t)image, sizeof(image));
    if (!root) {
        printf("initrd failed\n");
        exit(1);
    }

    // Call readdir for File 1 (index 0, since virtual dirs are now dynamic and start empty)
    // index 0 -> file_headers[0] -> file1
    struct dirent entry;
    memset(&entry, 0, sizeof(entry));

    // readdir_fs calls initrd_readdir
    // We call initrd_readdir directly to avoid linking vfs.c
    // int ret = readdir_fs(root, 0, &entry);
    int ret = initrd_readdir(root, 0, &entry);

    // If successful (0), check name
    if (ret != 0) {
        printf("Error: readdir failed with %d\n", ret);
        exit(1);
    }

    size_t len = strlen(entry.name);
    printf("Read name length: %zu\n", len);
    printf("Content: %s\n", entry.name);

    if (len > 64) {
        printf("VULNERABILITY CONFIRMED: Name length %zu > 64. Leak detected!\n", len);
        exit(1); // Fail (Vulnerability exists)
    } else {
        printf("Name length %zu. Safe.\n", len);
    }
}

int main() {
    test_initrd_readdir_leak();
    return 0;
}
