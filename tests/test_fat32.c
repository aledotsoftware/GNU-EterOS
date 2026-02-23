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
#include "../include/fs/fat32.h"
#include "../include/fs/vfs.h"
#include "../include/mm.h"
#include "../include/hal.h"

/* Mock HAL Console */
void hal_console_write(const char* str) {
    printf("%s", str);
}

/* Mock Memory Management */
static uint8_t heap[1024 * 1024];
static size_t heap_idx = 0;

void* kmalloc(size_t size) {
    if (heap_idx + size > sizeof(heap)) return NULL;
    void* ptr = &heap[heap_idx];
    heap_idx += size;
    /* Align to 16 bytes for safety */
    size = (size + 15) & ~15;
    heap_idx += size;
    return ptr;
}

void kfree(void* ptr) {
    (void)ptr;
}

/* Mock VFS globals if needed by fat32.c (it includes vfs.h) */
fs_node_t *fs_root = NULL;

/* Include source under test */
#include "../kernel/fs/fat32.c"

/* Mock Disk */
#define SECTOR_SIZE 512
#define TOTAL_SECTORS 1024
static uint8_t disk_image[TOTAL_SECTORS * SECTOR_SIZE];

int mock_read_sector(uint32_t sector, uint8_t* buffer) {
    if (sector >= TOTAL_SECTORS) return -1;
    memcpy(buffer, &disk_image[sector * SECTOR_SIZE], SECTOR_SIZE);
    return 0;
}

int mock_write_sector(uint32_t sector, const uint8_t* buffer) {
    if (sector >= TOTAL_SECTORS) return -1;
    memcpy(&disk_image[sector * SECTOR_SIZE], buffer, SECTOR_SIZE);
    return 0;
}

/* Helpers to setup disk */
void setup_disk() {
    memset(disk_image, 0, sizeof(disk_image));
    heap_idx = 0; // Reset heap for each test

    // BPB
    fat32_boot_sector_t* bpb = (fat32_boot_sector_t*)&disk_image[0];
    bpb->jmp_boot[0] = 0xEB; bpb->jmp_boot[1] = 0x3C; bpb->jmp_boot[2] = 0x90;
    bpb->boot_sector_signature = FAT32_BOOT_SIGNATURE;
    bpb->bytes_per_sector = SECTOR_SIZE;
    bpb->sectors_per_cluster = 1;
    bpb->reserved_sectors = 32;
    bpb->num_fats = 2;
    bpb->fat_size_32 = 1;
    bpb->root_cluster = 2;
    bpb->boot_signature = 0x29;

    // FAT (Sector 32)
    uint32_t* fat = (uint32_t*)&disk_image[32 * SECTOR_SIZE];
    fat[0] = 0x0FFFFFF8;
    fat[1] = 0x0FFFFFFF;
    fat[2] = 0x0FFFFFFF; // Cluster 2 (Root) - End of Chain
    fat[3] = 0x0FFFFFFF; // Cluster 3 (File) - End of Chain

    // Root Directory (Sector 34 - Cluster 2)
    fat32_dir_entry_t* root = (fat32_dir_entry_t*)&disk_image[34 * SECTOR_SIZE];

    // Entry: TEST.TXT
    memcpy(root[0].name, "TEST    TXT", 11);
    root[0].attr = 0; // Normal file
    root[0].fst_clus_hi = 0;
    root[0].fst_clus_lo = 3;
    root[0].file_size = 13; // "Hello, FAT32!"

    // File Content (Sector 35 - Cluster 3)
    char* file_data = (char*)&disk_image[35 * SECTOR_SIZE];
    strlcpy(file_data, "Hello, FAT32!", SECTOR_SIZE);
}

void test_fat32_read() {
    printf("Running test_fat32_read...\n");
    setup_disk();

    fat32_volume_t vol;
    int res = fat32_init(&vol, mock_read_sector, mock_write_sector, 0);
    if (res != 0) {
        printf("FAILED: fat32_init returned error %d\n", res);
        exit(1);
    }

    fs_node_t* root = fat32_mount(&vol);
    if (!root) {
        printf("FAILED: fat32_mount returned NULL\n");
        exit(1);
    }

    fs_node_t* file = fat32_finddir_fs(root, "TEST.TXT");
    if (!file) {
        printf("FAILED: TEST.TXT not found\n");
        exit(1);
    }

    char buffer[100];
    memset(buffer, 0, sizeof(buffer));

    uint32_t bytes = fat32_read_fs(file, 0, sizeof(buffer), (uint8_t*)buffer);
    kfree(file);
    kfree(root);

    if (bytes != 13) {
        printf("FAILED: Expected 13 bytes, got %d\n", bytes);
        exit(1);
    }

    if (strcmp(buffer, "Hello, FAT32!") != 0) {
        printf("FAILED: Content mismatch: '%s'\n", buffer);
        exit(1);
    }

    printf("PASSED\n");
}

void test_fat32_not_found() {
    printf("Running test_fat32_not_found...\n");
    setup_disk();

    fat32_volume_t vol;
    fat32_init(&vol, mock_read_sector, mock_write_sector, 0);

    fs_node_t* root = fat32_mount(&vol);
    if (!root) {
        printf("FAILED: fat32_mount returned NULL\n");
        exit(1);
    }

    fs_node_t* file = fat32_finddir_fs(root, "NONEXIST.ENT");
    if (file) {
        printf("FAILED: Found non-existent file\n");
        kfree(file);
        kfree(root);
        exit(1);
    }
    kfree(root);
    printf("PASSED\n");
}

void test_fat32_list() {
    printf("Running test_fat32_list...\n");
    setup_disk();
    fat32_volume_t vol;
    fat32_init(&vol, mock_read_sector, mock_write_sector, 0);

    fs_node_t* root = fat32_mount(&vol);
    if (!root) {
        printf("FAILED: fat32_mount returned NULL\n");
        exit(1);
    }

    printf("  [FAT32] Listing Root Directory:\n");
    struct dirent entry;
    uint32_t idx = 0;
    while (fat32_readdir_fs(root, idx, &entry) == 0) {
        printf("    - %s\n", entry.name);
        idx++;
    }
    kfree(root);
    printf("PASSED (Check output manually if needed)\n");
}

void test_fat32_create_write_read() {
    printf("Running test_fat32_create_write_read...\n");
    setup_disk();
    fat32_volume_t vol;
    fat32_init(&vol, mock_read_sector, mock_write_sector, 0);

    fs_node_t* root = fat32_mount(&vol);
    if (!root) {
        printf("FAILED: fat32_mount returned NULL\n");
        exit(1);
    }

    // Create file
    int res = fat32_create_fs(root, "NEW.TXT", 0);
    if (res != 0) {
        printf("FAILED: fat32_create_fs returned %d\n", res);
        exit(1);
    }

    // Write file
    const char* data = "This is a new file content.";
    fs_node_t* file = fat32_finddir_fs(root, "NEW.TXT");
    if (!file) {
        printf("FAILED: Could not find NEW.TXT\n");
        exit(1);
    }

    uint32_t bytes = fat32_write_fs(file, 0, strlen(data), (uint8_t*)data);
    if (bytes != strlen(data)) {
        printf("FAILED: fat32_write_fs returned %d (expected %ld)\n", bytes, strlen(data));
        exit(1);
    }
    kfree(file);

    // Read back
    char buffer[100];
    memset(buffer, 0, sizeof(buffer));

    file = fat32_finddir_fs(root, "NEW.TXT");
    if (!file) {
        printf("FAILED: Could not find NEW.TXT for reading\n");
        exit(1);
    }

    bytes = fat32_read_fs(file, 0, sizeof(buffer), (uint8_t*)buffer);
    if (bytes != strlen(data)) {
        printf("FAILED: fat32_read_fs returned %d\n", bytes);
        exit(1);
    }
    kfree(file);
    kfree(root);

    if (strcmp(buffer, data) != 0) {
        printf("FAILED: Read content mismatch: '%s'\n", buffer);
        exit(1);
    }

    printf("PASSED\n");
}

void test_fat32_delete() {
    printf("Running test_fat32_delete...\n");
    setup_disk();
    fat32_volume_t vol;
    fat32_init(&vol, mock_read_sector, mock_write_sector, 0);

    fs_node_t* root = fat32_mount(&vol);
    if (!root) {
        printf("FAILED: fat32_mount returned NULL\n");
        exit(1);
    }

    // Delete existing file TEST.TXT
    int res = fat32_unlink_fs(root, "TEST.TXT");
    if (res != 0) {
        printf("FAILED: fat32_unlink_fs returned %d\n", res);
        exit(1);
    }

    // Try to find it
    fs_node_t* file = fat32_finddir_fs(root, "TEST.TXT");
    if (file) {
        printf("FAILED: Should not be able to find deleted file\n");
        kfree(file);
        exit(1);
    }
    kfree(root);

    printf("PASSED\n");
}

void test_fat32_mkdir() {
    printf("Running test_fat32_mkdir...\n");
    setup_disk();
    fat32_volume_t vol;
    fat32_init(&vol, mock_read_sector, mock_write_sector, 0);

    fs_node_t* root = fat32_mount(&vol);
    if (!root) {
        printf("FAILED: fat32_mount returned NULL\n");
        exit(1);
    }

    // Create directory
    int res = fat32_mkdir_fs(root, "SUBDIR", 0);
    if (res != 0) {
        printf("FAILED: fat32_mkdir_fs returned %d\n", res);
        exit(1);
    }

    // Verify it exists in directory listing (by finding it)
    fs_node_t* dir = fat32_finddir_fs(root, "SUBDIR");
    if (!dir) {
        printf("FAILED: Could not find created directory\n");
        exit(1);
    }

    if (!(dir->flags & FS_DIRECTORY)) {
        printf("FAILED: Created entry is not a directory\n");
        exit(1);
    }
    kfree(dir);
    kfree(root);

    printf("PASSED\n");
}

void test_fat32_vfs() {
    printf("Running test_fat32_vfs...\n");
    setup_disk();
    fat32_volume_t vol;
    fat32_init(&vol, mock_read_sector, mock_write_sector, 0);

    // Mount
    fs_node_t* root = fat32_mount(&vol);
    if (!root) {
        printf("FAILED: fat32_mount returned NULL\n");
        exit(1);
    }

    // Lookup existing file
    fs_node_t* file = fat32_finddir_fs(root, "TEST.TXT");
    if (!file) {
        printf("FAILED: fat32_finddir_fs could not find TEST.TXT\n");
        exit(1);
    }

    // Read FS
    char buffer[100];
    memset(buffer, 0, sizeof(buffer));
    uint32_t bytes = fat32_read_fs(file, 0, 100, (uint8_t*)buffer);
    if (bytes != 13) {
        printf("FAILED: fat32_read_fs returned %d\n", bytes);
        exit(1);
    }
    if (strcmp(buffer, "Hello, FAT32!") != 0) {
        printf("FAILED: Content mismatch\n");
        exit(1);
    }
    kfree(file);

    // Create via VFS
    int res = fat32_create_fs(root, "VFS.TXT", 0);
    if (res != 0) {
        printf("FAILED: fat32_create_fs returned %d\n", res);
        exit(1);
    }

    // Find and Write via VFS
    file = fat32_finddir_fs(root, "VFS.TXT");
    if (!file) {
        printf("FAILED: Could not find VFS.TXT\n");
        exit(1);
    }

    const char* data = "VFS Write Test";
    bytes = fat32_write_fs(file, 0, strlen(data), (uint8_t*)data);
    if (bytes != strlen(data)) {
        printf("FAILED: fat32_write_fs returned %d\n", bytes);
        exit(1);
    }

    // Check file size in node updated?
    if (file->length != strlen(data)) {
        printf("FAILED: Node length not updated (%d)\n", file->length);
        exit(1);
    }
    kfree(file);

    // Verify Read back
    file = fat32_finddir_fs(root, "VFS.TXT");
    memset(buffer, 0, sizeof(buffer));
    fat32_read_fs(file, 0, 100, (uint8_t*)buffer);
    if (strcmp(buffer, data) != 0) {
        printf("FAILED: Read back mismatch: %s\n", buffer);
        exit(1);
    }
    kfree(file);
    kfree(root);

    printf("PASSED\n");
}

int main() {
    test_fat32_read();
    test_fat32_not_found();
    test_fat32_list();
    test_fat32_create_write_read();
    test_fat32_delete();
    test_fat32_mkdir();
    test_fat32_vfs();
    printf("All FAT32 tests passed!\n");
    return 0;
}
