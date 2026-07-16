#include <drivers/disk.h>
#include <mm.h>
#include <string.h>
#include <hal.h>
#include <nvram.h>

#define MBR_SIGNATURE 0xAA55
#define MBR_OFFSET_PARTITION_TABLE 446

typedef struct {
    uint8_t boot_indicator; // 0x80 = Active/Bootable
    uint8_t start_head;
    uint8_t start_sector;
    uint8_t start_cylinder;
    uint8_t partition_type;
    uint8_t end_head;
    uint8_t end_sector;
    uint8_t end_cylinder;
    uint32_t start_lba;
    uint32_t sector_count;
} __attribute__((packed)) mbr_partition_entry_t;

typedef struct {
    disk_t *disk;
    uint32_t start_lba;
    uint32_t sector_count;
    uint8_t type;
    uint8_t index;
    uint8_t is_active;
} partition_t;

static partition_t partitions[4]; // Support up to 4 primary partitions
static int partition_count = 0;
static int active_partition_index = -1;

static ssize_t partition_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (!node || !buffer) return 0;

    int p_index = node->impl;
    if (p_index < 0 || p_index >= partition_count) return 0;

    partition_t *part = &partitions[p_index];
    disk_t *disk = part->disk;

    uint32_t sector_size = disk->sector_size;
    if (sector_size == 0) return 0;

    uint32_t start_sector = offset / sector_size;
    uint32_t end_sector = (offset + size - 1) / sector_size;
    uint32_t sector_offset = offset % sector_size;

    uint32_t total_sectors_to_read = end_sector - start_sector + 1;

    if (start_sector >= part->sector_count) return 0;
    if (start_sector + total_sectors_to_read > part->sector_count) {
        total_sectors_to_read = part->sector_count - start_sector;
        size = (total_sectors_to_read * sector_size) - sector_offset; // Truncate size
    }

    uint8_t *sect_buf = kmalloc(sector_size);
    if (!sect_buf) return 0;

    uint32_t bytes_read = 0;

    for (uint32_t i = 0; i < total_sectors_to_read; i++) {
        uint32_t current_lba = part->start_lba + start_sector + i;
        if (disk->read_sector(disk, current_lba, sect_buf) != 0) {
            break;
        }

        uint32_t chunk_start = 0;
        if (i == 0) chunk_start = sector_offset;

        uint32_t chunk_end = sector_size;
        if (i == total_sectors_to_read - 1) {
             uint32_t remaining = size - bytes_read;
             if (chunk_start + remaining < sector_size) {
                 chunk_end = chunk_start + remaining;
             }
        }

        uint32_t chunk_size = chunk_end - chunk_start;
        memcpy(buffer + bytes_read, sect_buf + chunk_start, chunk_size);
        bytes_read += chunk_size;
    }

    kfree(sect_buf);
    return bytes_read;
}

static uint32_t partition_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (!node || !buffer) return 0;

    int p_index = node->impl;
    if (p_index < 0 || p_index >= partition_count) return 0;

    partition_t *part = &partitions[p_index];
    disk_t *disk = part->disk;

    uint32_t sector_size = disk->sector_size;
    if (sector_size == 0) return 0;

    uint32_t start_sector = offset / sector_size;
    uint32_t end_sector = (offset + size - 1) / sector_size;
    uint32_t sector_offset = offset % sector_size;

    uint32_t total_sectors_to_write = end_sector - start_sector + 1;

    if (start_sector >= part->sector_count) return 0;
    if (start_sector + total_sectors_to_write > part->sector_count) {
        total_sectors_to_write = part->sector_count - start_sector;
        size = (total_sectors_to_write * sector_size) - sector_offset; // Truncate size
    }

    uint8_t *sect_buf = kmalloc(sector_size);
    if (!sect_buf) return 0;

    uint32_t bytes_written = 0;

    for (uint32_t i = 0; i < total_sectors_to_write; i++) {
        uint32_t current_lba = part->start_lba + start_sector + i;

        uint32_t chunk_start = 0;
        if (i == 0) chunk_start = sector_offset;

        uint32_t chunk_end = sector_size;
        if (i == total_sectors_to_write - 1) {
             uint32_t remaining = size - bytes_written;
             if (chunk_start + remaining < sector_size) {
                 chunk_end = chunk_start + remaining;
             }
        }

        uint32_t chunk_size = chunk_end - chunk_start;

        // Read-Modify-Write: Only read if we're not overwriting the entire sector
        if (chunk_size < sector_size) {
            if (disk->read_sector(disk, current_lba, sect_buf) != 0) {
                break;
            }
        }

        memcpy(sect_buf + chunk_start, buffer + bytes_written, chunk_size);

        if (disk->write_sector(disk, current_lba, sect_buf) != 0) {
            break;
        }

        bytes_written += chunk_size;
    }

    kfree(sect_buf);
    return bytes_written;
}

void partition_scan(disk_t *disk) {
    if (!disk || !disk->read_sector) return;
    if (disk->sector_size == 0) return;

    uint8_t *buffer = kmalloc(disk->sector_size);
    if (!buffer) return;

    // Read MBR (LBA 0)
    if (disk->read_sector(disk, 0, buffer) != 0) {
        kfree(buffer);
        return;
    }

    // Check Signature
    if (*(uint16_t*)(buffer + 510) != MBR_SIGNATURE) {
        hal_console_write("[PARTITION] Invalid MBR Signature\n");
        kfree(buffer);
        return;
    }

    mbr_partition_entry_t *entries = (mbr_partition_entry_t*)(buffer + MBR_OFFSET_PARTITION_TABLE);

    partition_count = 0;
    active_partition_index = -1;

    for (int i = 0; i < 4; i++) {
        if (entries[i].partition_type != 0) {
            partitions[partition_count].disk = disk;
            partitions[partition_count].start_lba = entries[i].start_lba;
            partitions[partition_count].sector_count = entries[i].sector_count;
            partitions[partition_count].type = entries[i].partition_type;
            partitions[partition_count].index = i;
            partitions[partition_count].is_active = (entries[i].boot_indicator == 0x80);

            if (partitions[partition_count].is_active) {
                active_partition_index = partition_count;
            }
            partition_count++;
        }
    }
    kfree(buffer);

    // Default to first partition if none marked active
    if (active_partition_index == -1 && partition_count > 0) {
        active_partition_index = 0;
    }
}

static fs_node_t *create_partition_node(int index) {
    if (index < 0 || index >= partition_count) return NULL;

    fs_node_t *node = kmalloc(sizeof(fs_node_t));
    if (!node) return NULL;

    memset(node, 0, sizeof(fs_node_t));
    node->ref_count = 1;

    partition_t *part = &partitions[index];

    // Naming convention: sdX
    // Since we only support one disk for now, just say "partX"
    strlcpy(node->name, "part", sizeof(node->name));
    char num[2];
    num[0] = '0' + index;
    num[1] = '\0';
    strlcat(node->name, num, sizeof(node->name));

    node->flags = FS_BLOCKDEVICE;
    node->length = part->sector_count * part->disk->sector_size;
    node->impl = index; // Store partition index

    node->read = partition_read;
    node->write = partition_write;
    node->open = 0;
    node->close = 0;
    node->readdir = 0;
    node->finddir = 0;

    return node;
}

// Helper to compute the boot partition index securely once, mitigating post-boot changes.
static int get_booted_active_index(void) {
    static int cached_active_idx = -1;
    if (cached_active_idx != -1) return cached_active_idx;

    int active_idx = active_partition_index;
    uint8_t nvram_part = nvram_get_boot_partition();
    uint8_t update_state = nvram_get_update_state();

    if (nvram_part != 0xFF && nvram_part < partition_count) {
        active_idx = nvram_part;
    }

    // Cache the resolved boot index
    if (active_idx == 0 || active_idx == 1) {
        cached_active_idx = active_idx;
    }

    return active_idx;
}

fs_node_t *partition_get_active_root(void) {
    int active_idx = get_booted_active_index();

    if (active_idx != 0 && active_idx != 1) return NULL;

    if (active_idx < 0 || active_idx >= partition_count) return NULL;

    return create_partition_node(active_idx);
}

fs_node_t *partition_get_passive_root(void) {
    if (partition_count < 2) return NULL;

    int active_idx = get_booted_active_index();

    // Ensure active_idx is strictly 0 or 1 before flipping
    if (active_idx != 0 && active_idx != 1) {
        return NULL; // Prevent overwriting data partitions
    }

    // Simple A/B logic: Flip between 0 and 1
    // If active is 0, passive is 1. If active is 1, passive is 0.
    int passive_index = (active_idx == 0) ? 1 : 0;

    if (passive_index >= partition_count) return NULL;

    return create_partition_node(passive_index);
}
