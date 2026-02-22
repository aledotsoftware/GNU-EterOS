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
#include "../include/fs/jfs.h"
#include "../include/fs/vfs.h"
#include "../include/mm.h"
#include "../include/hal.h"

/* Mock HAL Console */
void hal_console_write(const char* str) {
    printf("%s", str);
}

/* Mock Memory Management */
void* kmalloc(size_t size) {
    return calloc(1, size); // Use calloc to zero memory like kernel often expects (though kmalloc usually doesn't, but let's be safe or just malloc)
    // Actually kmalloc in kernel doesn't zero, but for tests it might be cleaner.
    // Let's use malloc and memset if needed, or just calloc.
    // jfs_init does memset(jfs_disk_buffer, 0, jfs_disk_size), so malloc is fine.
}

void kfree(void* ptr) {
    free(ptr);
}

/* Mock VFS globals if needed */
fs_node_t *fs_root = NULL;

/* Include source under test */
// We need to include jfs.c to test static functions and internal state
#include "../kernel/fs/jfs.c"

void test_jfs_init() {
    printf("Running test_jfs_init...\n");
    fs_node_t* root = jfs_init();
    if (!root) {
        printf("FAILED: jfs_init returned NULL\n");
        exit(1);
    }

    // Verify superblock
    if (sb->magic != JFS_MAGIC) {
        printf("FAILED: Invalid magic 0x%x\n", sb->magic);
        exit(1);
    }

    printf("Superblock: Magic=0x%x, BlockSize=%d, TotalBlocks=%d\n", sb->magic, sb->block_size, sb->total_blocks);

    kfree(root);
    // jfs_disk_buffer is global static in jfs.c, we should probably free it if we can access it,
    // but it is static. Since we are running as a process, it's fine.
    // However, for multiple tests in one run, we might want to reset it.
    // But jfs_init allocates a new buffer every time, leaking the old one.
    // Since this is a test, it's acceptable, but ideally we would free it.
    // We can access jfs_disk_buffer since we included the .c file.
    if (jfs_disk_buffer) {
        free(jfs_disk_buffer);
        jfs_disk_buffer = NULL;
    }
    printf("PASSED\n");
}

void test_jfs_create_write() {
    printf("Running test_jfs_create_write...\n");
    fs_node_t* root = jfs_init();

    // Create a file
    int res = root->create(root, "file1", 0);
    if (res != 0) {
        printf("FAILED: create returned %d\n", res);
        exit(1);
    }

    // Find the file
    fs_node_t* file = root->finddir(root, "file1");
    if (!file) {
        printf("FAILED: finddir returned NULL\n");
        exit(1);
    }

    // Write to file
    char data[1024];
    memset(data, 'A', sizeof(data));
    uint32_t written = file->write(file, 0, sizeof(data), (uint8_t*)data);
    if (written != sizeof(data)) {
        printf("FAILED: write returned %d\n", written);
        exit(1);
    }

    // Verify file size
    jfs_inode_t* inode = get_inode(file->inode);
    if (inode->size != sizeof(data)) {
        printf("FAILED: inode size is %d, expected %lu\n", inode->size, sizeof(data));
        exit(1);
    }

    // Verify blocks allocated
    // 1024 bytes = 2 blocks (512 each).
    // inode->blocks[0] and inode->blocks[1] should be set.
    if (inode->blocks[0] == 0 || inode->blocks[1] == 0) {
        printf("FAILED: Blocks not allocated: %d, %d\n", inode->blocks[0], inode->blocks[1]);
        exit(1);
    }

    printf("Allocated blocks: %d, %d\n", inode->blocks[0], inode->blocks[1]);

    kfree(file);
    kfree(root);
    if (jfs_disk_buffer) {
        free(jfs_disk_buffer);
        jfs_disk_buffer = NULL;
    }
    printf("PASSED\n");
}

int main() {
    test_jfs_init();
    test_jfs_create_write();
    return 0;
}
