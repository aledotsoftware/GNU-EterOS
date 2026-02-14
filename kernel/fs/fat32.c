#include <fs/fat32.h>
#include <string.h>
#include <hal.h>
#include <mm.h>

#define FAT32_EOC 0x0FFFFFF8

/* Helper: Convert 8.3 name from entry to normal string */
static void fat32_get_name(const uint8_t* entry_name, char* out_name) {
    int i, j = 0;
    // Copy Name
    for (i = 0; i < 8; i++) {
        if (entry_name[i] == ' ') break;
        out_name[j++] = entry_name[i];
    }
    // Copy Extension
    if (entry_name[8] != ' ') {
        out_name[j++] = '.';
        for (i = 8; i < 11; i++) {
            if (entry_name[i] == ' ') break;
            out_name[j++] = entry_name[i];
        }
    }
    out_name[j] = '\0';
}

/* Helper: Convert input name to 8.3 format for comparison */
static void fat32_to_dos_name(const char* name, char* dos_name) {
    memset(dos_name, ' ', 11);
    int i = 0, j = 0;

    // Copy name part
    while (name[i] && name[i] != '.' && j < 8) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32; // To Upper
        dos_name[j++] = c;
        i++;
    }

    // Skip to dot or end
    while (name[i] && name[i] != '.') i++;

    if (name[i] == '.') {
        i++;
        j = 8;
        // Copy extension
        while (name[i] && j < 11) {
            char c = name[i];
            if (c >= 'a' && c <= 'z') c -= 32; // To Upper
            dos_name[j++] = c;
            i++;
        }
    }
}

int fat32_init(fat32_volume_t* vol, fat32_read_sector_t read_func, uint32_t partition_offset) {
    if (!vol || !read_func) return -1;

    vol->read_sector = read_func;
    vol->partition_lba = partition_offset;

    uint8_t* buffer = kmalloc(512);
    if (!buffer) return -2;

    // Read Boot Sector
    if (vol->read_sector(vol->partition_lba, buffer) != 0) {
        kfree(buffer);
        return -3;
    }

    fat32_boot_sector_t* bpb = (fat32_boot_sector_t*)buffer;

    // Check Signature
    if (bpb->boot_sector_signature != 0xAA55) {
        hal_console_write("[FAT32] Invalid Boot Sector Signature\n");
        kfree(buffer);
        return -4;
    }

    // Load geometry
    vol->bytes_per_sector = bpb->bytes_per_sector;
    vol->sectors_per_cluster = bpb->sectors_per_cluster;
    vol->root_cluster = bpb->root_cluster;

    // Check minimal sector size (usually 512)
    if (vol->bytes_per_sector == 0) {
        kfree(buffer);
        return -5;
    }

    // Calculate offsets
    uint32_t fat_size = bpb->fat_size_32; // FAT32 uses fat_size_32
    if (fat_size == 0) fat_size = bpb->fat_size_16; // Fallback, though FAT32 should use 32

    vol->fat_size = fat_size;
    vol->fat_start_lba = vol->partition_lba + bpb->reserved_sectors;

    // Data Start = Partition + Reserved + (NumFATs * FATSize)
    // Note: Root Directory in FAT32 is a cluster chain, usually starting at cluster 2 (start of data area).
    vol->data_start_lba = vol->fat_start_lba + (bpb->num_fats * fat_size);

    // Calculate total clusters just in case
    // uint32_t total_sectors = (bpb->total_sectors_16 == 0) ? bpb->total_sectors_32 : bpb->total_sectors_16;
    // uint32_t data_sectors = total_sectors - (bpb->reserved_sectors + (bpb->num_fats * fat_size));
    // vol->total_clusters = data_sectors / vol->sectors_per_cluster;

    kfree(buffer);
    return 0;
}

static uint32_t fat32_get_next_cluster(fat32_volume_t* vol, uint32_t current_cluster) {
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = vol->fat_start_lba + (fat_offset / vol->bytes_per_sector);
    uint32_t ent_offset = fat_offset % vol->bytes_per_sector;

    uint8_t* buffer = kmalloc(512);
    if (!buffer) return 0xFFFFFFFF;

    if (vol->read_sector(fat_sector, buffer) != 0) {
        kfree(buffer);
        return 0xFFFFFFFF;
    }

    uint32_t next_cluster = *(uint32_t*)&buffer[ent_offset];
    kfree(buffer);

    return next_cluster & 0x0FFFFFFF;
}

static uint32_t fat32_cluster_to_lba(fat32_volume_t* vol, uint32_t cluster) {
    return vol->data_start_lba + ((cluster - 2) * vol->sectors_per_cluster);
}

void fat32_list_directory(fat32_volume_t* vol) {
    uint32_t current_cluster = vol->root_cluster;
    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return;

    hal_console_write("  [FAT32] Listing Root Directory:\n");

    while (current_cluster < FAT32_EOC) {
        uint32_t lba = fat32_cluster_to_lba(vol, current_cluster);

        for (uint32_t i = 0; i < vol->sectors_per_cluster; i++) {
            if (vol->read_sector(lba + i, buffer) != 0) {
                hal_console_write("Error reading directory sector\n");
                kfree(buffer);
                return;
            }

            fat32_dir_entry_t* entry = (fat32_dir_entry_t*)buffer;
            int entries_per_sector = vol->bytes_per_sector / sizeof(fat32_dir_entry_t);

            for (int j = 0; j < entries_per_sector; j++) {
                if (entry[j].name[0] == DIRENT_END) {
                    kfree(buffer);
                    return; // End of directory
                }
                if (entry[j].name[0] == DIRENT_DELETED) continue;
                if (entry[j].attr & ATTR_LONG_NAME) continue; // Skip LFN for now
                if (entry[j].attr & ATTR_VOLUME_ID) continue;

                char name_buf[13];
                fat32_get_name(entry[j].name, name_buf);

                hal_console_write("    - ");
                hal_console_write(name_buf);
                if (entry[j].attr & ATTR_DIRECTORY) {
                    hal_console_write("/");
                }
                hal_console_write("\n");
            }
        }
        current_cluster = fat32_get_next_cluster(vol, current_cluster);
    }
    kfree(buffer);
}

int fat32_find_file(fat32_volume_t* vol, const char* filename, fat32_dir_entry_t* out_entry) {
    char dos_name[11];
    fat32_to_dos_name(filename, dos_name);

    uint32_t current_cluster = vol->root_cluster;
    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return -1;

    while (current_cluster < FAT32_EOC) {
        uint32_t lba = fat32_cluster_to_lba(vol, current_cluster);

        for (uint32_t i = 0; i < vol->sectors_per_cluster; i++) {
            if (vol->read_sector(lba + i, buffer) != 0) {
                kfree(buffer);
                return -2;
            }

            fat32_dir_entry_t* entry = (fat32_dir_entry_t*)buffer;
            int entries_per_sector = vol->bytes_per_sector / sizeof(fat32_dir_entry_t);

            for (int j = 0; j < entries_per_sector; j++) {
                if (entry[j].name[0] == DIRENT_END) {
                    kfree(buffer);
                    return -3; // Not found
                }
                if (entry[j].name[0] == DIRENT_DELETED) continue;
                if (entry[j].attr & ATTR_VOLUME_ID) continue;
                if (entry[j].attr & ATTR_LONG_NAME) continue;

                // Compare name
                if (memcmp(entry[j].name, dos_name, 11) == 0) {
                    *out_entry = entry[j];
                    kfree(buffer);
                    return 0; // Found
                }
            }
        }
        current_cluster = fat32_get_next_cluster(vol, current_cluster);
    }
    kfree(buffer);
    return -3; // Not found
}

int fat32_read_file(fat32_volume_t* vol, const char* filename, void* buffer, uint32_t size) {
    fat32_dir_entry_t entry;
    if (fat32_find_file(vol, filename, &entry) != 0) {
        return -1; // File not found
    }

    if (entry.attr & ATTR_DIRECTORY) return -2; // Cannot read directory as file

    uint32_t cluster = (uint32_t)entry.fst_clus_hi << 16 | entry.fst_clus_lo;
    uint32_t bytes_read = 0;
    uint32_t bytes_to_read = (size < entry.file_size) ? size : entry.file_size;
    uint8_t* out_ptr = (uint8_t*)buffer;

    uint8_t* sector_buffer = kmalloc(vol->bytes_per_sector);
    if (!sector_buffer) return -3;

    while (bytes_read < bytes_to_read && cluster < FAT32_EOC) {
        uint32_t lba = fat32_cluster_to_lba(vol, cluster);

        for (uint32_t i = 0; i < vol->sectors_per_cluster; i++) {
            if (bytes_read >= bytes_to_read) break;

            if (vol->read_sector(lba + i, sector_buffer) != 0) {
                kfree(sector_buffer);
                return -4; // Read error
            }

            uint32_t chunk = vol->bytes_per_sector;
            if (bytes_read + chunk > bytes_to_read) {
                chunk = bytes_to_read - bytes_read;
            }

            memcpy(out_ptr + bytes_read, sector_buffer, chunk);
            bytes_read += chunk;
        }
        cluster = fat32_get_next_cluster(vol, cluster);
    }

    kfree(sector_buffer);
    return bytes_read;
}
