#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif
#include <assert.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <stdint.h>

/* Mock types */
#include "../include/types.h"
#include "../include/fs/vfs.h"
#include "../include/mm.h"
#include "../include/hal.h"
#include "../include/klog.h"

/* Mock HAL Console */
void hal_console_write(const char* str) {
    // printf("[MOCK HAL] %s", str); // Uncomment for debug
}
void serial_write_string(const char* str) {
    (void)str;
}

/* Mock Memory Management */
static uint8_t heap[8 * 1024 * 1024]; // 8MB Heap
static size_t heap_idx = 0;

void* kmalloc(size_t size) {
    if (heap_idx + size > sizeof(heap)) return NULL;
    void* ptr = &heap[heap_idx];
    heap_idx += size;
    size = (size + 15) & ~15; // Align
    return ptr;
}

void kfree(void* ptr) {
    (void)ptr;
}

/* Include source under test */

#include "../kernel/fs/jfs.c"

/* Tests */

/* Helper to reset JFS state between tests */
void reset_jfs_state() {
    heap_idx = 0;
    jfs_disk_buffer = NULL;
    sb = NULL;
    current_tx_id = 1;
    jfs_next_free_block = 0;
}

void test_jfs_init() {
    printf("Running test_jfs_init...\n");
    reset_jfs_state(); // Reset heap and static vars

    fs_node_t* fs = jfs_init();
    assert(fs != NULL);
    assert(strcmp(fs->name, "jfs_root") == 0);
    assert(fs->flags == FS_DIRECTORY);

    // Verify Superblock
    assert(sb != NULL);
    assert(sb->magic == JFS_MAGIC);
    assert(sb->block_size == 512);

    printf("PASSED\n");
}

void test_jfs_create_file() {
    printf("Running test_jfs_create_file...\n");
    reset_jfs_state();
    fs_node_t* root = jfs_init();

    // Create file
    int res = root->create(root, "test.txt", 0);
    assert(res == 0);

    // Find file
    fs_node_t* file = root->finddir(root, "test.txt");
    assert(file != NULL);
    assert(strcmp(file->name, "test.txt") == 0);

    // Verify Journal for creation (directory update)
    uint32_t j_start = sb->journal_start;

    // Inspect the journal area.
    // jfs_journal_write writes:
    // Block j_start: TX_BEGIN
    // Block j_start+1: TX_DATA
    // Block j_start+2: The data
    // Block j_start+3: TX_COMMIT

    // Check TX_COMMIT
    jfs_journal_header_t* commit_hdr = (jfs_journal_header_t*)(jfs_disk_buffer + ((j_start + 3) * 512));
    assert(commit_hdr->type == JFS_TX_COMMIT);

    // Check TX_BEGIN
    jfs_journal_header_t* begin_hdr = (jfs_journal_header_t*)(jfs_disk_buffer + (j_start * 512));
    assert(begin_hdr->type == JFS_TX_BEGIN);
    assert(begin_hdr->tx_id == commit_hdr->tx_id);

    // Check TX_DATA
    jfs_journal_header_t* data_hdr = (jfs_journal_header_t*)(jfs_disk_buffer + ((j_start + 1) * 512));
    assert(data_hdr->type == JFS_TX_DATA);

    // Verify target block matches root dir block
    // Root dir block is sb->data_start (100)
    assert(data_hdr->target_block == sb->data_start);

    printf("PASSED\n");
}

void test_jfs_write_read() {
    printf("Running test_jfs_write_read...\n");
    reset_jfs_state();
    fs_node_t* root = jfs_init();

    // Create file
    root->create(root, "data.txt", 0);
    fs_node_t* file = root->finddir(root, "data.txt");
    assert(file != NULL);

    const char* data = "Hello, Journaling Filesystem!";
    uint32_t len = strlen(data);

    // Write
    uint32_t written = file->write(file, 0, len, (uint8_t*)data);
    assert(written == len);

    // Read
    char buffer[100];
    memset(buffer, 0, sizeof(buffer));
    uint32_t read = file->read(file, 0, sizeof(buffer), (uint8_t*)buffer);
    assert(read == len);
    assert(strcmp(buffer, data) == 0);

    // Verify Journal for write
    uint32_t j_start = sb->journal_start;

    // Inspect the journal area.
    jfs_journal_header_t* commit_hdr = (jfs_journal_header_t*)(jfs_disk_buffer + ((j_start + 3) * 512));
    assert(commit_hdr->type == JFS_TX_COMMIT);

    // Verify data in journal
    uint8_t* journal_data = jfs_disk_buffer + ((j_start + 2) * 512);
    assert(memcmp(journal_data, data, len) == 0);

    // Verify file still exists in directory (check for corruption)
    // If jfs_write overwrote the root directory block (100), this will fail.
    fs_node_t* file2 = root->finddir(root, "data.txt");
    if (file2 == NULL) {
        printf("CRITICAL: Root directory corrupted by file write!\n");
        assert(file2 != NULL);
    }

    printf("PASSED\n");
}

void test_jfs_directory() {
    printf("Running test_jfs_directory...\n");
    reset_jfs_state();
    fs_node_t* root = jfs_init();

    root->create(root, "file1.txt", 0);
    root->create(root, "file2.txt", 0);

    struct dirent entry;

    // Readdir index 0
    int res = root->readdir(root, 0, &entry);
    assert(res == 0);
    assert(strcmp(entry.name, "file1.txt") == 0);

    // Readdir index 1
    res = root->readdir(root, 1, &entry);
    assert(res == 0);
    assert(strcmp(entry.name, "file2.txt") == 0);

    // Readdir index 2 (EOF)
    res = root->readdir(root, 2, &entry);
    assert(res != 0);

    printf("PASSED\n");
}

int main() {
    test_jfs_init();
    test_jfs_create_file();
    test_jfs_write_read();
    test_jfs_directory();
    printf("All JFS tests passed!\n");
    return 0;
}
