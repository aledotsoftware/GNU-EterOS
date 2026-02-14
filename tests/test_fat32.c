#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
/* Since -Iinclude shadows system headers, we might miss standard declarations */
void exit(int status);
char *strcpy(char *dest, const char *src);
int strcmp(const char *s1, const char *s2);
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);

#include <stdint.h>
#include <assert.h>

/* Mock types */
#include "../include/types.h"
#include "../include/fs/fat32.h"
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

/* Helpers to setup disk */
void setup_disk() {
    memset(disk_image, 0, sizeof(disk_image));
    heap_idx = 0; // Reset heap for each test

    // BPB
    fat32_boot_sector_t* bpb = (fat32_boot_sector_t*)&disk_image[0];
    bpb->jmp_boot[0] = 0xEB; bpb->jmp_boot[1] = 0x3C; bpb->jmp_boot[2] = 0x90;
    bpb->boot_sector_signature = 0xAA55;
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
    strcpy(file_data, "Hello, FAT32!");
}

void test_fat32_read() {
    printf("Running test_fat32_read...\n");
    setup_disk();

    fat32_volume_t vol;
    int res = fat32_init(&vol, mock_read_sector, 0);
    if (res != 0) {
        printf("FAILED: fat32_init returned error %d\n", res);
        exit(1);
    }

    char buffer[100];
    memset(buffer, 0, sizeof(buffer));

    int bytes = fat32_read_file(&vol, "TEST.TXT", buffer, sizeof(buffer));

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
    fat32_init(&vol, mock_read_sector, 0);

    char buffer[100];
    int res = fat32_read_file(&vol, "NONEXIST.ENT", buffer, sizeof(buffer));

    if (res >= 0) {
        printf("FAILED: Expected error for non-existent file, got %d\n", res);
        exit(1);
    }
    printf("PASSED\n");
}

void test_fat32_list() {
    printf("Running test_fat32_list...\n");
    setup_disk();
    fat32_volume_t vol;
    fat32_init(&vol, mock_read_sector, 0);
    fat32_list_directory(&vol);
    printf("PASSED (Check output manually if needed)\n");
}

int main() {
    test_fat32_read();
    test_fat32_not_found();
    test_fat32_list();
    printf("All FAT32 tests passed!\n");
    return 0;
}
