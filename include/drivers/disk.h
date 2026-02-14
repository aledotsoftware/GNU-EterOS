#ifndef DRIVERS_DISK_H
#define DRIVERS_DISK_H

#include <types.h>
#include <fs/vfs.h>

typedef struct disk disk_t;

typedef int (*disk_read_sector_t)(disk_t *disk, uint32_t lba, uint8_t *buffer);
typedef int (*disk_write_sector_t)(disk_t *disk, uint32_t lba, uint8_t *buffer);

struct disk {
    uint32_t sector_size;
    uint32_t sector_count;
    disk_read_sector_t read_sector;
    disk_write_sector_t write_sector;
    void *priv; // Private driver data
};

// Initializes the partition manager for a disk
void partition_scan(disk_t *disk);

// Get the fs_node for the active and passive partitions
fs_node_t *partition_get_active_root(void);
fs_node_t *partition_get_passive_root(void);

#endif
