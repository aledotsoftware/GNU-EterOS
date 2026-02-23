#ifndef FAT32_H
#define FAT32_H

#include <types.h>
#include <lock.h>
#include <fs/vfs.h>

#define FAT32_BOOT_SIGNATURE 0xAA55

/**
 * FAT32 Boot Sector (BPB)
 */
typedef struct {
    uint8_t  jmp_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    // FAT32 Extended BPB
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
    uint8_t  boot_code[420];
    uint16_t boot_sector_signature;
} __attribute__((packed)) fat32_boot_sector_t;

/**
 * FSInfo Sector Structure
 */
typedef struct {
    uint32_t lead_signature;    // 0x41615252
    uint8_t  reserved1[480];
    uint32_t struct_signature;  // 0x61417272
    uint32_t free_count;        // Last known free cluster count
    uint32_t next_free;         // Hint for next free cluster
    uint8_t  reserved2[12];
    uint32_t trail_signature;   // 0xAA550000
} __attribute__((packed)) fat32_fs_info_t;

/**
 * Directory Entry (Standard 8.3)
 */
typedef struct {
    uint8_t  name[11];          // 8.3 format (no dot)
    uint8_t  attr;
    uint8_t  nt_res;
    uint8_t  crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t fst_clus_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t fst_clus_lo;
    uint32_t file_size;
} __attribute__((packed)) fat32_dir_entry_t;

// File Attributes
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20
#define ATTR_LONG_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

// End of Directory Marker
#define DIRENT_END 0x00
// Deleted Entry Marker
#define DIRENT_DELETED 0xE5

/**
 * Callback function to read a sector from disk
 * @param sector Sector number (LBA)
 * @param buffer Buffer to store data (must be at least 512 bytes)
 * @return 0 on success, non-zero on error
 */
typedef int (*fat32_read_sector_t)(uint32_t sector, uint8_t* buffer);

/**
 * Callback function to write a sector to disk
 * @param sector Sector number (LBA)
 * @param buffer Buffer containing data to write (must be at least 512 bytes)
 * @return 0 on success, non-zero on error
 */
typedef int (*fat32_write_sector_t)(uint32_t sector, const uint8_t* buffer);

/**
 * FAT32 Internal State
 */
typedef struct {
    fat32_read_sector_t read_sector;
    fat32_write_sector_t write_sector;
    uint32_t partition_lba;
    uint32_t fat_start_lba;
    uint32_t data_start_lba;
    uint32_t root_cluster;
    uint32_t sectors_per_cluster;
    uint16_t bytes_per_sector;
    uint32_t fat_size;          // in sectors
    uint32_t volume_id;         // Unique ID for caching
    spinlock_t lock;
} fat32_volume_t;

/**
 * Public API
 */

/**
 * Initialize the FAT32 driver with read and write callbacks
 */
int fat32_init(fat32_volume_t* vol, fat32_read_sector_t read_func, fat32_write_sector_t write_func, uint32_t partition_offset);

/**
 * Mount the FAT32 volume and return the root filesystem node.
 * @param vol Initialized FAT32 volume structure
 * @return Pointer to root fs_node_t, or NULL on failure.
 */
fs_node_t* fat32_mount(fat32_volume_t* vol);

#endif /* FAT32_H */
