#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

typedef __builtin_va_list __gnuc_va_list;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <sys/time.h>

/* Mock types */
#include "../include/types.h"
#include "../include/fs/fat32.h"
#include "../include/fs/vfs.h"
#include "../include/fs/bcache.h"
#include "../include/mm.h"
#include "../include/hal.h"

/* Mock HAL Console */
void hal_console_write(const char* str) {
    // printf("%s", str); // Silence console output for benchmark
}

/* Mock Memory Management */
static uint8_t heap[32 * 1024 * 1024]; // Increase heap for larger test
static size_t heap_idx = 0;

void* kmalloc(size_t size) {
    if (heap_idx + size > sizeof(heap)) {
        printf("Heap exhausted!\n");
        return NULL;
    }
    void* ptr = &heap[heap_idx];
    heap_idx += size;
    size = (size + 15) & ~15;
    heap_idx += size;
    return ptr;
}

void kfree(void* ptr) {
    (void)ptr;
}

fs_node_t *fs_root = NULL;

#include "../kernel/fs/fat32.c"

/* Mock Disk - Larger for benchmarking */
#define SECTOR_SIZE 512
// Enough sectors for a reasonable FAT size
// 65536 clusters * 1 sector/cluster = 65536 sectors
// + FAT + Reserved... let's say 70000 sectors.
#define TOTAL_SECTORS 70000
static uint8_t* disk_image;

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

void setup_disk() {
    disk_image = malloc(TOTAL_SECTORS * SECTOR_SIZE);
    if (!disk_image) {
        printf("Failed to allocate disk image\n");
        exit(1);
    }
    memset(disk_image, 0, TOTAL_SECTORS * SECTOR_SIZE);
    heap_idx = 0;

    // BPB
    fat32_boot_sector_t* bpb = (fat32_boot_sector_t*)&disk_image[0];
    bpb->jmp_boot[0] = 0xEB; bpb->jmp_boot[1] = 0x3C; bpb->jmp_boot[2] = 0x90;
    bpb->boot_sector_signature = 0xAA55;
    bpb->bytes_per_sector = SECTOR_SIZE;
    bpb->sectors_per_cluster = 1;
    bpb->reserved_sectors = 32;
    bpb->num_fats = 2;

    // FAT size
    // 65536 entries * 4 bytes = 262144 bytes
    // 262144 / 512 = 512 sectors
    bpb->fat_size_32 = 512;

    bpb->root_cluster = 2;
    bpb->boot_signature = 0x29;

    // Initialize FAT
    uint32_t fat_offset_sectors = bpb->reserved_sectors;
    uint32_t* fat = (uint32_t*)&disk_image[fat_offset_sectors * SECTOR_SIZE];
    fat[0] = 0x0FFFFFF8;
    fat[1] = 0x0FFFFFFF;
    fat[2] = 0x0FFFFFFF; // Cluster 2 (Root)
}

void teardown_disk() {
    free(disk_image);
}

void bench_allocation() {
    printf("Running FAT32 Allocation Benchmark...\n");
    setup_disk();

    fat32_volume_t vol;
    if (fat32_init(&vol, mock_read_sector, mock_write_sector, 0) != 0) {
        printf("Init failed\n");
        exit(1);
    }

    // Allocate many clusters
    int num_allocations = 5000;
    uint32_t cluster;

    struct timeval start, end;
    gettimeofday(&start, NULL);

    for (int i = 0; i < num_allocations; i++) {
        if (fat32_alloc_cluster(&vol, &cluster) != 0) {
            printf("Allocation failed at %d\n", i);
            break;
        }
    }
    gettimeofday(&end, NULL);

    double time_taken = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Time taken for %d allocations: %f seconds\n", num_allocations, time_taken);
    printf("Average time per allocation: %f ms\n", (time_taken * 1000) / num_allocations);

    teardown_disk();
}

int main() {
    bench_allocation();
    return 0;
}
