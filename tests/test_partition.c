#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>

/* Since -Iinclude shadows system headers, we might miss standard declarations */
void exit(int status);

#include <stdint.h>

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

int mock_read_sector(disk_t *disk, uint32_t lba, uint8_t *buffer) {
    if (lba >= TOTAL_SECTORS) return -1;
    memcpy(buffer, &disk_image[lba * SECTOR_SIZE], SECTOR_SIZE);
    return 0;
}

int mock_write_sector(disk_t *disk, uint32_t lba, uint8_t *buffer) {
    if (lba >= TOTAL_SECTORS) return -1;
    memcpy(&disk_image[lba * SECTOR_SIZE], buffer, SECTOR_SIZE);
    return 0;
}

void setup_disk() {
    memset(disk_image, 0, sizeof(disk_image));
    heap_idx = 0;

    // Create MBR
    mbr_partition_entry_t *entries = (mbr_partition_entry_t*)&disk_image[446];

    // Partition 1
    entries[0].boot_indicator = 0x00;
    entries[0].partition_type = 0x83;
    entries[0].start_lba = 1;
    entries[0].sector_count = 100;

    // Partition 2 (Passive)
    entries[1].boot_indicator = 0x00;
    entries[1].partition_type = 0x83;
    entries[1].start_lba = 101;
    entries[1].sector_count = 100;

    disk_image[510] = 0x55;
    disk_image[511] = 0xAA;
}

void test_partition_scan() {
    printf("Running test_partition_scan...\n");
    setup_disk();

    global_disk.sector_size = SECTOR_SIZE;
    global_disk.sector_count = TOTAL_SECTORS;
    global_disk.read_sector = mock_read_sector;
    global_disk.write_sector = mock_write_sector;

    partition_scan(&global_disk);

    if (partition_count != 2) {
        printf("FAILED: Expected 2 partitions, got %d\n", partition_count);
        exit(1);
    }

    if (active_partition_index != 0) {
        printf("FAILED: Expected active partition 0, got %d\n", active_partition_index);
        exit(1);
    }

    printf("PASSED\n");
}

void test_ab_logic() {
    printf("Running test_ab_logic...\n");
    fs_node_t *active = partition_get_active_root();
    fs_node_t *passive = partition_get_passive_root();

    if (!active) { printf("FAILED: Active node is NULL\n"); exit(1); }
    if (!passive) { printf("FAILED: Passive node is NULL\n"); exit(1); }

    if (active->impl != 0) { printf("FAILED: Active index mismatch\n"); exit(1); }
    if (passive->impl != 1) { printf("FAILED: Passive index mismatch\n"); exit(1); }

    printf("PASSED\n");
}

void test_io() {
    printf("Running test_io...\n");
    fs_node_t *passive = partition_get_passive_root();

    char write_buf[512];
    memset(write_buf, 0, sizeof(write_buf));
    strlcpy(write_buf, "Test Data Payload", sizeof(write_buf));

    // Call node->write directly since write_fs is in vfs.c which is not linked
    uint32_t written = passive->write(passive, 0, 512, (uint8_t*)write_buf);

    if (written != 512) { printf("FAILED: Write returned %d\n", written); exit(1); }

    // Verify on disk image
    // LBA 101 (passive start)
    if (memcmp(&disk_image[101 * 512], write_buf, 512) != 0) {
        printf("FAILED: Data mismatch on disk\n");
        exit(1);
    }

    char read_buf[512];
    memset(read_buf, 0, 512);
    ssize_t read = passive->read(passive, 0, 512, (uint8_t*)read_buf);

    if (read != 512) { printf("FAILED: Read returned %ld\n", (long)read); exit(1); }

    if (memcmp(read_buf, write_buf, 512) != 0) {
        printf("FAILED: Read data mismatch\n");
        exit(1);
    }

    // Test OOB Write
    // Partition size is 100 sectors * 512 bytes = 51200 bytes.
    // Try to write at offset 51200 (should fail/return 0)
    uint32_t oob_written = passive->write(passive, 51200, 512, (uint8_t*)write_buf);
    if (oob_written != 0) {
        printf("FAILED: OOB Write should return 0, returned %d\n", oob_written);
        exit(1);
    }

    printf("PASSED\n");
}

void test_partition_write_edge_cases() {
    printf("Running test_partition_write_edge_cases...\n");
    fs_node_t *passive = partition_get_passive_root();

    char write_buf[512];
    memset(write_buf, 0xAA, sizeof(write_buf));

    // Null checks
    if (passive->write(NULL, 0, 512, (uint8_t*)write_buf) != 0) {
        printf("FAILED: NULL node should return 0\n"); exit(1);
    }
    if (passive->write(passive, 0, 512, NULL) != 0) {
        printf("FAILED: NULL buffer should return 0\n"); exit(1);
    }

    // Invalid partition index
    fs_node_t invalid_node;
    memcpy(&invalid_node, passive, sizeof(fs_node_t));
    invalid_node.impl = 999;
    if (invalid_node.write(&invalid_node, 0, 512, (uint8_t*)write_buf) != 0) {
        printf("FAILED: Invalid partition index should return 0\n"); exit(1);
    }

    // Sector size 0
    uint32_t original_sector_size = global_disk.sector_size;
    global_disk.sector_size = 0;
    if (passive->write(passive, 0, 512, (uint8_t*)write_buf) != 0) {
        printf("FAILED: Sector size 0 should return 0\n"); exit(1);
    }
    global_disk.sector_size = original_sector_size;

    // The passive partition starts at LBA 101, so sector size is 512.
    // LBA 101 byte offset in disk_image is 101 * 512 = 51712.
    uint32_t passive_start_byte = 101 * 512;

    // Unaligned write: smaller than a sector, not starting on a boundary
    char small_buf[20];
    memset(small_buf, 0xBB, sizeof(small_buf));
    uint32_t written = passive->write(passive, 10, 20, (uint8_t*)small_buf);
    if (written != 20) {
        printf("FAILED: Unaligned write returned %d\n", written); exit(1);
    }
    if (memcmp(&disk_image[passive_start_byte + 10], small_buf, 20) != 0) {
        printf("FAILED: Unaligned write data mismatch\n"); exit(1);
    }

    // Unaligned write: crossing a sector boundary
    char cross_buf[30];
    memset(cross_buf, 0xCC, sizeof(cross_buf));
    written = passive->write(passive, 500, 30, (uint8_t*)cross_buf);
    if (written != 30) {
        printf("FAILED: Cross-boundary write returned %d\n", written); exit(1);
    }
    if (memcmp(&disk_image[passive_start_byte + 500], cross_buf, 30) != 0) {
        printf("FAILED: Cross-boundary write data mismatch\n"); exit(1);
    }

    // Write that gets truncated at the end of the partition
    // Passive partition is 100 sectors = 51200 bytes
    char trunc_buf[20];
    memset(trunc_buf, 0xDD, sizeof(trunc_buf));
    written = passive->write(passive, 51190, 20, (uint8_t*)trunc_buf);
    if (written != 10) {
        printf("FAILED: Truncated write returned %d, expected 10\n", written); exit(1);
    }
    if (memcmp(&disk_image[passive_start_byte + 51190], trunc_buf, 10) != 0) {
         printf("FAILED: Truncated write data mismatch\n"); exit(1);
    }

    printf("PASSED\n");
}

int main() {
    test_partition_scan();
    test_ab_logic();
    test_io();
    test_partition_write_edge_cases();
    printf("All partition tests passed!\n");
    return 0;
}
