#include <assert.h>
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

/* Mock disk partition and bcache */
#include "../include/fs/bcache.h"
#include "../include/drivers/disk.h"

static uint8_t mock_disk[4 * 1024 * 1024]; // 4MB Disk

int bcache_read(uint32_t volume_id, uint32_t sector, uint8_t *buffer) {
    memcpy(buffer, mock_disk + (sector * 512), 512);
    return 0;
}
void bcache_write(uint32_t volume_id, uint32_t sector, const uint8_t *buffer) {
    memcpy(mock_disk + (sector * 512), buffer, 512);
}

static fs_node_t mock_part_node;
static ssize_t mock_part_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    memcpy(buffer, mock_disk + offset, size);
    return size;
}
static uint32_t mock_part_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    memcpy(mock_disk + offset, buffer, size);
    return size;
}
fs_node_t* partition_get_active_root(void) {
    mock_part_node.read = mock_part_read;
    mock_part_node.write = mock_part_write;
    mock_part_node.length = sizeof(mock_disk);
    return &mock_part_node;
}
uint8_t nvram_get_boot_partition(void) { return 0; }

/* Include source under test */

#include "../kernel/fs/jfs.c"

/* Tests */

/* Helper to reset JFS state between tests */
void reset_jfs_state() {
    heap_idx = 0;
    memset(mock_disk, 0, sizeof(mock_disk));
    jfs_part_node = NULL;
    sb = &jfs_sb_cache;
    current_tx_id = 1;
    jfs_next_free_block = 0;
}

void test_jfs_init() {
    printf("Running test_jfs_init...\n");
    reset_jfs_state(); // Reset heap and static vars

    fs_node_t* fs = jfs_init();
    ASSERT(fs != NULL);
    ASSERT(strcmp(fs->name, "jfs_root") == 0);
    ASSERT(fs->flags == FS_DIRECTORY);

    // Verify Superblock
    ASSERT(sb != NULL);
    ASSERT(sb->magic == JFS_MAGIC);
    ASSERT(sb->block_size == 512);

    printf("PASSED\n");
}

void test_jfs_create_file() {
    printf("Running test_jfs_create_file...\n");
    reset_jfs_state();
    fs_node_t* root = jfs_init();

    // Create file
    int res = root->create(root, "test.txt", 0);
    ASSERT(res == 0);

    // Find file
    fs_node_t* file = root->finddir(root, "test.txt");
    ASSERT(file != NULL);
    ASSERT(strcmp(file->name, "test.txt") == 0);

    // Verify Journal for creation (directory update)
    uint32_t j_start = sb->journal_start;

    // Inspect the journal area.
    // jfs_journal_write writes:
    // Block j_start: TX_BEGIN
    // Block j_start+1: TX_DATA
    // Block j_start+2: The data
    // Block j_start+3: TX_COMMIT

    // Check TX_COMMIT
    jfs_journal_header_t* commit_hdr = (jfs_journal_header_t*)(mock_disk + ((j_start + 3) * 512));
    ASSERT(commit_hdr->type == JFS_TX_COMMIT);

    // Check TX_BEGIN
    jfs_journal_header_t* begin_hdr = (jfs_journal_header_t*)(mock_disk + (j_start * 512));
    ASSERT(begin_hdr->type == JFS_TX_BEGIN);
    ASSERT(begin_hdr->tx_id == commit_hdr->tx_id);

    // Check TX_DATA
    jfs_journal_header_t* data_hdr = (jfs_journal_header_t*)(mock_disk + ((j_start + 1) * 512));
    ASSERT(data_hdr->type == JFS_TX_DATA);

    // Verify target block matches root dir block
    // Root dir block is sb->data_start (100)
    ASSERT(data_hdr->target_block == sb->data_start);

    printf("PASSED\n");
}

void test_jfs_write_read() {
    printf("Running test_jfs_write_read...\n");
    reset_jfs_state();
    fs_node_t* root = jfs_init();

    // Create file
    root->create(root, "data.txt", 0);
    fs_node_t* file = root->finddir(root, "data.txt");
    ASSERT(file != NULL);

    const char* data = "Hello, Journaling Filesystem!";
    uint32_t len = strlen(data);

    // Write
    uint32_t written = file->write(file, 0, len, (uint8_t*)data);
    ASSERT(written == len);

    // Read
    char buffer[100];
    memset(buffer, 0, sizeof(buffer));
    uint32_t read = file->read(file, 0, sizeof(buffer), (uint8_t*)buffer);
    ASSERT(read == len);
    ASSERT(strcmp(buffer, data) == 0);

    // Verify Journal for write
    uint32_t j_start = sb->journal_start;

    // Inspect the journal area.
    jfs_journal_header_t* commit_hdr = (jfs_journal_header_t*)(mock_disk + ((j_start + 3) * 512));
    ASSERT(commit_hdr->type == JFS_TX_COMMIT);

    // Verify data in journal
    uint8_t* journal_data = mock_disk + ((j_start + 2) * 512);
    ASSERT(memcmp(journal_data, data, len) == 0);

    // Verify file still exists in directory (check for corruption)
    // If jfs_write overwrote the root directory block (100), this will fail.
    fs_node_t* file2 = root->finddir(root, "data.txt");
    if (file2 == NULL) {
        printf("CRITICAL: Root directory corrupted by file write!\n");
        ASSERT(file2 != NULL);
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
    ASSERT(res == 0);
    ASSERT(strcmp(entry.name, "file1.txt") == 0);

    // Readdir index 1
    res = root->readdir(root, 1, &entry);
    ASSERT(res == 0);
    ASSERT(strcmp(entry.name, "file2.txt") == 0);

    // Readdir index 2 (EOF)
    res = root->readdir(root, 2, &entry);
    ASSERT(res != 0);

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
