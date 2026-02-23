#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Since -Iinclude shadows system headers, we might miss standard declarations */
void exit(int status);

/* Mock types */
#include "../include/types.h"
#include "../include/fs/vfs.h"
#include "../include/drivers/disk.h"

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
    size = (size + 15) & ~15;
    heap_idx += size;
    return ptr;
}

void kfree(void* ptr) {
    (void)ptr;
}

/* Include source under test */
#include "../kernel/drivers/disk/partition.c"

/* Mock Disk */
#define SECTOR_SIZE 512
#define TOTAL_SECTORS 2048
static uint8_t disk_image[TOTAL_SECTORS * SECTOR_SIZE];
static disk_t global_disk;

static uint32_t read_count = 0;
static uint32_t write_count = 0;

int mock_read_sector(disk_t *disk, uint32_t lba, uint8_t *buffer) {
    read_count++;
    if (lba >= TOTAL_SECTORS) return -1;
    memcpy(buffer, &disk_image[lba * SECTOR_SIZE], SECTOR_SIZE);
    return 0;
}

int mock_write_sector(disk_t *disk, uint32_t lba, uint8_t *buffer) {
    write_count++;
    if (lba >= TOTAL_SECTORS) return -1;
    memcpy(&disk_image[lba * SECTOR_SIZE], buffer, SECTOR_SIZE);
    return 0;
}

void setup_disk() {
    memset(disk_image, 0, sizeof(disk_image));
    heap_idx = 0;
    read_count = 0;
    write_count = 0;

    // Create MBR
    mbr_partition_entry_t *entries = (mbr_partition_entry_t*)&disk_image[446];

    // Partition 1 (Active)
    entries[0].boot_indicator = 0x80;
    entries[0].partition_type = 0x83;
    entries[0].start_lba = 1;
    entries[0].sector_count = 100;

    disk_image[510] = 0x55;
    disk_image[511] = 0xAA;

    global_disk.sector_size = SECTOR_SIZE;
    global_disk.sector_count = TOTAL_SECTORS;
    global_disk.read_sector = mock_read_sector;
    global_disk.write_sector = mock_write_sector;

    partition_scan(&global_disk);
}

void run_bench() {
    setup_disk();
    fs_node_t *active = partition_get_active_root();

    printf("Benchmarking sequential write of 10 sectors (5120 bytes)...\n");

    uint32_t size = 10 * SECTOR_SIZE;
    uint8_t *buffer = malloc(size);
    memset(buffer, 0xCC, size);

    read_count = 0;
    write_count = 0;
    uint32_t written = active->write(active, 0, size, buffer);

    printf("Bytes written: %d\n", written);
    printf("Read operations: %d\n", read_count);
    printf("Write operations: %d\n", write_count);

    if (read_count > 0) {
        printf("RESULT: Redundant reads detected: %d\n", read_count);
    } else {
        printf("RESULT: No redundant reads!\n");
    }

    free(buffer);
}

void run_bench_unaligned() {
    setup_disk();
    fs_node_t *active = partition_get_active_root();

    printf("\nBenchmarking unaligned write (offset 100, size 1000)...\n");

    uint32_t size = 1000;
    uint8_t *buffer = malloc(size);
    memset(buffer, 0xDD, size);

    read_count = 0;
    write_count = 0;
    uint32_t written = active->write(active, 100, size, buffer);

    printf("Bytes written: %d\n", written);
    printf("Read operations: %d\n", read_count);
    printf("Write operations: %d\n", write_count);

    // For offset 100, size 1000:
    // Sector 0: offset 100, size 412 (Partial) -> Needs Read-Modify-Write
    // Sector 1: offset 0, size 512 (Full) -> Should NOT need Read-Modify-Write
    // Sector 2: offset 0, size 76 (Partial) -> Needs Read-Modify-Write
    // Total sectors: 3.
    // Redundant reads expected: 1 (for Sector 1).
    // Necessary reads: 2 (Sector 0 and 2).

    free(buffer);
}

int main() {
    run_bench();
    run_bench_unaligned();
    return 0;
}
