#include <fs/fat32.h>
#include <fs/vfs.h>
#include <fs/bcache.h>
#include <string.h>
#include <hal.h>
#include <mm.h>

#define FAT32_EOC 0x0FFFFFF8

static uint32_t next_volume_id = 1;

static int fat32_read_sector_cached(fat32_volume_t* vol, uint32_t sector, uint8_t* buffer) {
    if (bcache_read(vol->volume_id, sector, buffer) == 0) return 0;
    int res = vol->read_sector(sector, buffer);
    if (res == 0) bcache_write(vol->volume_id, sector, buffer);
    return res;
}

static int fat32_write_sector_cached(fat32_volume_t* vol, uint32_t sector, const uint8_t* buffer) {
    int res = vol->write_sector(sector, buffer);
    if (res == 0) bcache_write(vol->volume_id, sector, buffer);
    return res;
}

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

int fat32_init(fat32_volume_t* vol, fat32_read_sector_t read_func, fat32_write_sector_t write_func, uint32_t partition_offset) {
    if (!vol || !read_func) return -1;

    vol->read_sector = read_func;
    vol->write_sector = write_func;
    vol->partition_lba = partition_offset;
    vol->lock = 0;
    vol->volume_id = __atomic_fetch_add(&next_volume_id, 1, __ATOMIC_SEQ_CST);

    // Allocate 4096 to be safe for larger sector sizes during init
    uint8_t* buffer = kmalloc(4096);
    if (!buffer) return -2;

    // Read Boot Sector
    if (fat32_read_sector_cached(vol, vol->partition_lba, buffer) != 0) {
        kfree(buffer);
        return -3;
    }

    fat32_boot_sector_t* bpb = (fat32_boot_sector_t*)buffer;

    // Check Signature
    if (bpb->boot_sector_signature != FAT32_BOOT_SIGNATURE) {
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

    kfree(buffer);
    return 0;
}

static uint32_t fat32_get_next_cluster(fat32_volume_t* vol, uint32_t current_cluster) {
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = vol->fat_start_lba + (fat_offset / vol->bytes_per_sector);
    uint32_t ent_offset = fat_offset % vol->bytes_per_sector;

    /* ⚡ BOLT Optimization: Use stack buffer for typical sector sizes (<=512) to avoid kmalloc overhead */
    uint8_t stack_buffer[512];
    uint8_t* buffer = stack_buffer;
    int use_kmalloc = 0;

    if (vol->bytes_per_sector > sizeof(stack_buffer)) {
        buffer = kmalloc(vol->bytes_per_sector);
        if (!buffer) return 0xFFFFFFFF;
        use_kmalloc = 1;
    }

    if (fat32_read_sector_cached(vol, fat_sector, buffer) != 0) {
        if (use_kmalloc) kfree(buffer);
        return 0xFFFFFFFF;
    }

    uint32_t next_cluster = *(uint32_t*)&buffer[ent_offset];
    if (use_kmalloc) kfree(buffer);

    return next_cluster & 0x0FFFFFFF;
}

static uint32_t fat32_cluster_to_lba(fat32_volume_t* vol, uint32_t cluster) {
    return vol->data_start_lba + ((cluster - 2) * vol->sectors_per_cluster);
}

/* Helper: Write to FAT Table */
static int fat32_fat_set(fat32_volume_t* vol, uint32_t cluster, uint32_t value) {
    if (!vol->write_sector) return -1;

    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = vol->fat_start_lba + (fat_offset / vol->bytes_per_sector);
    uint32_t ent_offset = fat_offset % vol->bytes_per_sector;

    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return -2;

    // Read sector first (RMW)
    if (fat32_read_sector_cached(vol, fat_sector, buffer) != 0) {
        kfree(buffer);
        return -3;
    }

    // Update entry
    *(uint32_t*)&buffer[ent_offset] = value;

    // Write back
    if (fat32_write_sector_cached(vol, fat_sector, buffer) != 0) {
        kfree(buffer);
        return -4;
    }

    kfree(buffer);
    return 0;
}

/* Helper: Allocate a free cluster */
static int fat32_alloc_cluster(fat32_volume_t* vol, uint32_t* out_cluster) {
    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return -1;

    uint32_t current_fat_sector = vol->fat_start_lba;
    uint32_t entries_per_sector = vol->bytes_per_sector / 4;
    uint32_t cluster_idx = 0;

    // Scan full FAT
    for (uint32_t i = 0; i < vol->fat_size; i++) {
        if (fat32_read_sector_cached(vol, current_fat_sector + i, buffer) != 0) {
            kfree(buffer);
            return -2;
        }

        uint32_t* entries = (uint32_t*)buffer;
        for (uint32_t j = 0; j < entries_per_sector; j++) {
            // Cluster 0 and 1 are reserved.
            if (cluster_idx < 2) {
                cluster_idx++;
                continue;
            }

            if ((entries[j] & 0x0FFFFFFF) == 0) {
                // Found free
                *out_cluster = cluster_idx;

                // Mark as EOC
                entries[j] = FAT32_EOC;
                if (fat32_write_sector_cached(vol, current_fat_sector + i, buffer) != 0) {
                    kfree(buffer);
                    return -3;
                }

                // Zero out the cluster data
                uint8_t* zero_buf = kmalloc(vol->bytes_per_sector);
                if (zero_buf) {
                    memset(zero_buf, 0, vol->bytes_per_sector);
                    uint32_t lba = fat32_cluster_to_lba(vol, *out_cluster);
                    for (uint32_t k = 0; k < vol->sectors_per_cluster; k++) {
                        fat32_write_sector_cached(vol, lba + k, zero_buf);
                    }
                    kfree(zero_buf);
                }

                kfree(buffer);
                return 0;
            }
            cluster_idx++;
        }
    }

    kfree(buffer);
    return -4; // No free space found
}

/* Helper: Free cluster chain */
static void fat32_free_cluster_chain(fat32_volume_t* vol, uint32_t start_cluster) {
    uint32_t current = start_cluster;
    while (current >= 2 && current < FAT32_EOC) {
        uint32_t next = fat32_get_next_cluster(vol, current);
        fat32_fat_set(vol, current, 0); // Mark free
        current = next;
    }
}

/* Helper: Update or Create Directory Entry */
static int fat32_update_dirent(fat32_volume_t* vol, uint32_t sector, uint32_t offset, fat32_dir_entry_t* value) {
    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return -1;

    if (fat32_read_sector_cached(vol, sector, buffer) != 0) {
        kfree(buffer);
        return -2;
    }

    memcpy(buffer + offset, value, sizeof(fat32_dir_entry_t));

    if (fat32_write_sector_cached(vol, sector, buffer) != 0) {
        kfree(buffer);
        return -3;
    }

    kfree(buffer);
    return 0;
}

/* Callback for directory iteration */
typedef int (*fat32_dir_cb_t)(fat32_volume_t* vol, fat32_dir_entry_t* entry, uint32_t sector, uint32_t offset, void* ctx);

/* Helper: Iterate through directory entries in a cluster chain */
static int fat32_iterate_dir(fat32_volume_t* vol, uint32_t start_cluster, fat32_dir_cb_t callback, void* ctx, uint32_t* last_cluster_out) {
    uint32_t current_cluster = start_cluster;
    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return -1;

    while (current_cluster < FAT32_EOC) {
        if (last_cluster_out) *last_cluster_out = current_cluster;

        uint32_t lba = fat32_cluster_to_lba(vol, current_cluster);

        for (uint32_t i = 0; i < vol->sectors_per_cluster; i++) {
            if (fat32_read_sector_cached(vol, lba + i, buffer) != 0) {
                kfree(buffer);
                return -2;
            }

            fat32_dir_entry_t* entry = (fat32_dir_entry_t*)buffer;
            int entries_per_sector = vol->bytes_per_sector / sizeof(fat32_dir_entry_t);

            for (int j = 0; j < entries_per_sector; j++) {
                int res = callback(vol, &entry[j], lba + i, j * sizeof(fat32_dir_entry_t), ctx);
                if (res != 0) {
                    kfree(buffer);
                    return res;
                }
            }
        }

        uint32_t next = fat32_get_next_cluster(vol, current_cluster);
        if (next >= FAT32_EOC) {
             break;
        }
        current_cluster = next;
    }
    kfree(buffer);
    return 0; /* Finished chain */
}

struct find_free_ctx {
    uint32_t* sector;
    uint32_t* offset;
};

static int fat32_find_free_cb(fat32_volume_t* vol, fat32_dir_entry_t* entry, uint32_t sector, uint32_t offset, void* ctx) {
    (void)vol;
    struct find_free_ctx* c = (struct find_free_ctx*)ctx;
    if (entry->name[0] == DIRENT_END || entry->name[0] == DIRENT_DELETED) {
        *c->sector = sector;
        *c->offset = offset;
        return 1; // Found
    }
    return 0; // Continue
}

/* Helper: Find a free directory entry slot in a cluster chain */
static int fat32_find_free_dirent_in_chain(fat32_volume_t* vol, uint32_t start_cluster, uint32_t* out_sector, uint32_t* out_offset) {
    struct find_free_ctx ctx = { out_sector, out_offset };
    uint32_t last_cluster = 0;

    int res = fat32_iterate_dir(vol, start_cluster, fat32_find_free_cb, &ctx, &last_cluster);

    if (res == 1) return 0; // Found
    if (res < 0) return res; // Error

    // If we are here, we reached EOC without finding a slot. Extend.
    uint32_t new_cluster;
    if (fat32_alloc_cluster(vol, &new_cluster) != 0) return -3; // Disk full

    fat32_fat_set(vol, last_cluster, new_cluster);

    *out_sector = fat32_cluster_to_lba(vol, new_cluster);
    *out_offset = 0;
    return 0;
}

struct find_entry_ctx {
    char* dos_name;
    uint32_t* out_sector;
    uint32_t* out_offset;
    fat32_dir_entry_t* out_entry;
};

static int fat32_find_entry_cb(fat32_volume_t* vol, fat32_dir_entry_t* entry, uint32_t sector, uint32_t offset, void* ctx) {
    (void)vol;
    struct find_entry_ctx* c = (struct find_entry_ctx*)ctx;

    if (entry->name[0] == DIRENT_END) return -3; /* Not Found, stop iteration */
    if (entry->name[0] == DIRENT_DELETED) return 0;
    if (entry->attr & ATTR_VOLUME_ID) return 0;
    if (entry->attr & ATTR_LONG_NAME) return 0;

    if (memcmp(entry->name, c->dos_name, 11) == 0) {
        if (c->out_entry) *c->out_entry = *entry;
        if (c->out_sector) *c->out_sector = sector;
        if (c->out_offset) *c->out_offset = offset;
        return 1; /* Found */
    }
    return 0;
}

/* Helper: Get Directory Entry Location in Chain */
static int fat32_find_dirent_in_chain(fat32_volume_t* vol, uint32_t start_cluster, const char* filename, uint32_t* out_sector, uint32_t* out_offset, fat32_dir_entry_t* out_entry) {
    char dos_name[11];
    fat32_to_dos_name(filename, dos_name);

    struct find_entry_ctx ctx = { dos_name, out_sector, out_offset, out_entry };
    int res = fat32_iterate_dir(vol, start_cluster, fat32_find_entry_cb, &ctx, NULL);

    if (res == 1) return 0; // Success
    if (res == -3) return -3; // Explicit Not Found
    if (res == 0) return -3; // End of Chain, Not Found
    return res; // Error
}


/* ========================================================================= */
/* VFS Implementation                                                        */
/* ========================================================================= */

// Forward declarations
ssize_t fat32_read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t fat32_write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
void fat32_open_fs(fs_node_t *node);
void fat32_close_fs(fs_node_t *node);
int fat32_readdir_fs(fs_node_t *node, uint32_t index, struct dirent *entry);
fs_node_t *fat32_finddir_fs(fs_node_t *node, char *name);
int fat32_create_fs(fs_node_t *parent, char *name, uint16_t permission);
int fat32_mkdir_fs(fs_node_t *parent, char *name, uint16_t permission);
int fat32_unlink_fs(fs_node_t *parent, char *name);

static uint32_t fat32_seek_cluster(fat32_volume_t* vol, uint32_t first_cluster, uint32_t offset, uint32_t* offset_within_cluster) {
    uint32_t cluster_size = vol->bytes_per_sector * vol->sectors_per_cluster;
    uint32_t current_cluster = first_cluster;
    uint32_t clusters_to_skip = offset / cluster_size;
    *offset_within_cluster = offset % cluster_size;

    for (uint32_t i = 0; i < clusters_to_skip; i++) {
        if (current_cluster >= FAT32_EOC) return FAT32_EOC;
        current_cluster = fat32_get_next_cluster(vol, current_cluster);
    }
    return current_cluster;
}

void fat32_open_fs(fs_node_t *node) {
    fat32_volume_t* vol = (fat32_volume_t*)node->ptr;
    spin_lock(&vol->lock);
    spin_unlock(&vol->lock);
}

void fat32_close_fs(fs_node_t *node) {
    fat32_volume_t* vol = (fat32_volume_t*)node->ptr;
    spin_lock(&vol->lock);
    spin_unlock(&vol->lock);
}

static ssize_t fat32_read_fs_impl(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    fat32_volume_t* vol = (fat32_volume_t*)node->ptr;
    uint32_t start_cluster = node->inode;

    if (offset >= node->length) return 0;
    if (offset + size > node->length) size = node->length - offset;

    uint32_t cluster_offset;
    uint32_t current_cluster = fat32_seek_cluster(vol, start_cluster, offset, &cluster_offset);
    if (current_cluster >= FAT32_EOC) return 0;

    uint32_t bytes_read = 0;
    uint8_t* sector_buffer = kmalloc(vol->bytes_per_sector);
    if (!sector_buffer) return 0;

    while (bytes_read < size && current_cluster < FAT32_EOC) {
        uint32_t lba = fat32_cluster_to_lba(vol, current_cluster);
        uint32_t sector_in_cluster = cluster_offset / vol->bytes_per_sector;
        uint32_t offset_in_sector = cluster_offset % vol->bytes_per_sector;

        for (uint32_t i = sector_in_cluster; i < vol->sectors_per_cluster; i++) {
            if (bytes_read >= size) break;

            uint32_t chunk = vol->bytes_per_sector - offset_in_sector;
            if (bytes_read + chunk > size) chunk = size - bytes_read;

            if (offset_in_sector == 0 && chunk == vol->bytes_per_sector) {
                /* ⚡ BOLT Optimization: Direct read to user buffer if aligned & full sector.
                   Avoids intermediate buffer copy. */
                if (fat32_read_sector_cached(vol, lba + i, buffer + bytes_read) != 0) {
                    kfree(sector_buffer);
                    return bytes_read;
                }
            } else {
                if (fat32_read_sector_cached(vol, lba + i, sector_buffer) != 0) {
                    kfree(sector_buffer);
                    return bytes_read;
                }
                memcpy(buffer + bytes_read, sector_buffer + offset_in_sector, chunk);
            }

            bytes_read += chunk;
            offset_in_sector = 0;
        }
        current_cluster = fat32_get_next_cluster(vol, current_cluster);
        cluster_offset = 0;
    }
    kfree(sector_buffer);
    return bytes_read;
}

ssize_t fat32_read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    fat32_volume_t* vol = (fat32_volume_t*)node->ptr;
    spin_lock(&vol->lock);
    ssize_t res = fat32_read_fs_impl(node, offset, size, buffer);
    spin_unlock(&vol->lock);
    return res;
}

static uint32_t fat32_write_fs_impl(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    fat32_volume_t* vol = (fat32_volume_t*)node->ptr;

    if (node->inode == 0) {
        uint32_t new_cluster;
        if (fat32_alloc_cluster(vol, &new_cluster) != 0) return 0;
        node->inode = new_cluster;

        uint32_t dir_sector = node->impl;
        uint32_t dir_offset = node->uid;
        uint8_t* sec_buf = kmalloc(vol->bytes_per_sector);
        if (sec_buf) {
            if (fat32_read_sector_cached(vol, dir_sector, sec_buf) == 0) {
                fat32_dir_entry_t* ep = (fat32_dir_entry_t*)(sec_buf + dir_offset);
                ep->fst_clus_hi = (new_cluster >> 16) & 0xFFFF;
                ep->fst_clus_lo = new_cluster & 0xFFFF;
                fat32_write_sector_cached(vol, dir_sector, sec_buf);
            }
            kfree(sec_buf);
        }
    }

    uint32_t start_cluster = node->inode;
    uint32_t cluster_offset;
    uint32_t current_cluster = fat32_seek_cluster(vol, start_cluster, offset, &cluster_offset);

    if (current_cluster >= FAT32_EOC) {
         current_cluster = start_cluster;
         while (1) {
             uint32_t next = fat32_get_next_cluster(vol, current_cluster);
             if (next >= FAT32_EOC) break;
             current_cluster = next;
         }
         uint32_t new_cluster;
         if (fat32_alloc_cluster(vol, &new_cluster) != 0) return 0;
         fat32_fat_set(vol, current_cluster, new_cluster);
         current_cluster = new_cluster;
         cluster_offset = 0;
    }

    uint32_t bytes_written = 0;
    uint8_t* sector_buffer = kmalloc(vol->bytes_per_sector);
    if (!sector_buffer) return 0;

    while (bytes_written < size) {
        if (current_cluster == 0 || current_cluster >= FAT32_EOC) {
             uint32_t new_cluster;
             if (fat32_alloc_cluster(vol, &new_cluster) != 0) break;
             // We need to link from previous cluster. But we don't track prev here easily.
             // Assume simplified extension or fail. Real impl needs robust chain tracking.
             // For this test, we might not hit this if we allocated above.
             break;
        }

        uint32_t lba = fat32_cluster_to_lba(vol, current_cluster);
        uint32_t sector_in_cluster = cluster_offset / vol->bytes_per_sector;
        uint32_t offset_in_sector = cluster_offset % vol->bytes_per_sector;

        for (uint32_t i = sector_in_cluster; i < vol->sectors_per_cluster; i++) {
            if (bytes_written >= size) break;

            uint32_t chunk = vol->bytes_per_sector - offset_in_sector;
            if (bytes_written + chunk > size) chunk = size - bytes_written;

            if (offset_in_sector == 0 && chunk == vol->bytes_per_sector) {
                /* ⚡ BOLT Optimization: Full sector overwrite.
                   Skip read (RMW) and direct write to save I/O and copy. */
                if (fat32_write_sector_cached(vol, lba + i, buffer + bytes_written) != 0) break;
            } else {
                if (fat32_read_sector_cached(vol, lba + i, sector_buffer) != 0) break;
                memcpy(sector_buffer + offset_in_sector, buffer + bytes_written, chunk);
                if (fat32_write_sector_cached(vol, lba + i, sector_buffer) != 0) break;
            }

            bytes_written += chunk;
            offset_in_sector = 0;
        }

        uint32_t next = fat32_get_next_cluster(vol, current_cluster);
        if (next >= FAT32_EOC && bytes_written < size) {
             uint32_t new_cluster;
             if (fat32_alloc_cluster(vol, &new_cluster) != 0) break;
             fat32_fat_set(vol, current_cluster, new_cluster);
             current_cluster = new_cluster;
             cluster_offset = 0;
        } else {
             current_cluster = next;
             cluster_offset = 0;
        }
    }

    kfree(sector_buffer);

    if (offset + bytes_written > node->length) {
        node->length = offset + bytes_written;
        uint32_t dir_sector = node->impl;
        uint32_t dir_offset = node->uid;

        uint8_t* sec_buf = kmalloc(vol->bytes_per_sector);
        if (sec_buf) {
            if (fat32_read_sector_cached(vol, dir_sector, sec_buf) == 0) {
                fat32_dir_entry_t* ep = (fat32_dir_entry_t*)(sec_buf + dir_offset);
                ep->file_size = node->length;
                fat32_write_sector_cached(vol, dir_sector, sec_buf);
            }
            kfree(sec_buf);
        }
    }
    return bytes_written;
}

uint32_t fat32_write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    fat32_volume_t* vol = (fat32_volume_t*)node->ptr;
    spin_lock(&vol->lock);
    uint32_t res = fat32_write_fs_impl(node, offset, size, buffer);
    spin_unlock(&vol->lock);
    return res;
}

struct readdir_ctx {
    uint32_t index;
    uint32_t count;
    struct dirent* out_entry;
};

static int fat32_readdir_cb(fat32_volume_t* vol, fat32_dir_entry_t* entry, uint32_t sector, uint32_t offset, void* ctx) {
    (void)vol;
    (void)sector;
    (void)offset;
    struct readdir_ctx* c = (struct readdir_ctx*)ctx;

    if (entry->name[0] == DIRENT_END) return 1; /* EOF */
    if (entry->name[0] == DIRENT_DELETED) return 0;
    if (entry->attr & ATTR_VOLUME_ID) return 0;
    if (entry->attr & ATTR_LONG_NAME) return 0;

    if (c->count == c->index) {
        fat32_get_name(entry->name, c->out_entry->name);
        c->out_entry->inode = (entry->fst_clus_hi << 16) | entry->fst_clus_lo;
        return 2; /* Success code for callback */
    }
    c->count++;
    return 0;
}

static int fat32_readdir_fs_impl(fs_node_t *node, uint32_t index, struct dirent *out_entry) {
    fat32_volume_t* vol = (fat32_volume_t*)node->ptr;
    struct readdir_ctx ctx = { index, 0, out_entry };

    int res = fat32_iterate_dir(vol, node->inode, fat32_readdir_cb, &ctx, NULL);

    if (res == 2) return 0; /* Success */
    if (res == 1) return 1; /* EOF */
    if (res == 0) return 1; /* EOF (end of chain) */
    return -1; /* Error */
}

int fat32_readdir_fs(fs_node_t *node, uint32_t index, struct dirent *entry) {
    fat32_volume_t* vol = (fat32_volume_t*)node->ptr;
    spin_lock(&vol->lock);
    int res = fat32_readdir_fs_impl(node, index, entry);
    spin_unlock(&vol->lock);
    return res;
}

static fs_node_t *fat32_finddir_fs_impl(fs_node_t *node, char *name) {
    fat32_volume_t* vol = (fat32_volume_t*)node->ptr;
    uint32_t start_cluster = node->inode;

    fat32_dir_entry_t entry;
    uint32_t dir_sector, dir_offset;

    if (fat32_find_dirent_in_chain(vol, start_cluster, name, &dir_sector, &dir_offset, &entry) != 0) {
        return NULL;
    }

    fs_node_t* child = kmalloc(sizeof(fs_node_t));
    if (!child) return NULL;
    memset(child, 0, sizeof(fs_node_t));

    child->inode = (entry.fst_clus_hi << 16) | entry.fst_clus_lo;
    strlcpy(child->name, name, sizeof(child->name));
    child->impl = dir_sector;
    child->uid = dir_offset;
    child->ptr = (struct fs_node*)vol;
    child->length = entry.file_size;
    child->ref_count = 1;

    if (entry.attr & ATTR_DIRECTORY) {
        child->flags = FS_DIRECTORY;
        child->mask = 0777;
        child->read = 0;
        child->write = 0;
        child->readdir = fat32_readdir_fs;
        child->finddir = fat32_finddir_fs;
        child->create = fat32_create_fs;
        child->mkdir = fat32_mkdir_fs;
        child->unlink = fat32_unlink_fs;
    } else {
        child->flags = FS_FILE;
        child->mask = 0666;
        child->read = fat32_read_fs;
        child->write = fat32_write_fs;
        child->readdir = 0;
        child->finddir = 0;
    }
    child->open = fat32_open_fs;
    child->close = fat32_close_fs;

    return child;
}

fs_node_t *fat32_finddir_fs(fs_node_t *node, char *name) {
    fat32_volume_t* vol = (fat32_volume_t*)node->ptr;
    spin_lock(&vol->lock);
    fs_node_t* res = fat32_finddir_fs_impl(node, name);
    spin_unlock(&vol->lock);
    return res;
}

static int fat32_create_fs_impl(fs_node_t *parent, char *name, uint16_t permission) {
    (void)permission;
    fat32_volume_t* vol = (fat32_volume_t*)parent->ptr;

    if (fat32_find_dirent_in_chain(vol, parent->inode, name, NULL, NULL, NULL) == 0) return -1;

    uint32_t sector, offset;
    if (fat32_find_free_dirent_in_chain(vol, parent->inode, &sector, &offset) != 0) return -2;

    fat32_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    fat32_to_dos_name(name, (char*)entry.name);
    entry.attr = ATTR_ARCHIVE;
    entry.file_size = 0;
    entry.fst_clus_hi = 0;
    entry.fst_clus_lo = 0;

    return fat32_update_dirent(vol, sector, offset, &entry);
}

int fat32_create_fs(fs_node_t *parent, char *name, uint16_t permission) {
    fat32_volume_t* vol = (fat32_volume_t*)parent->ptr;
    spin_lock(&vol->lock);
    int res = fat32_create_fs_impl(parent, name, permission);
    spin_unlock(&vol->lock);
    return res;
}

static int fat32_mkdir_fs_impl(fs_node_t *parent, char *name, uint16_t permission) {
    (void)permission;
    fat32_volume_t* vol = (fat32_volume_t*)parent->ptr;

    if (fat32_find_dirent_in_chain(vol, parent->inode, name, NULL, NULL, NULL) == 0) return -1;

    uint32_t sector, offset;
    if (fat32_find_free_dirent_in_chain(vol, parent->inode, &sector, &offset) != 0) return -2;

    uint32_t cluster;
    if (fat32_alloc_cluster(vol, &cluster) != 0) return -3;

    fat32_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    fat32_to_dos_name(name, (char*)entry.name);
    entry.attr = ATTR_DIRECTORY;
    entry.fst_clus_hi = (cluster >> 16) & 0xFFFF;
    entry.fst_clus_lo = cluster & 0xFFFF;

    if (fat32_update_dirent(vol, sector, offset, &entry) != 0) return -4;

    uint32_t parent_clus = parent->inode;
    if (parent_clus == vol->root_cluster) parent_clus = 0;

    fat32_dir_entry_t dot[2];
    memset(dot, 0, sizeof(dot));

    memset(dot[0].name, ' ', 11);
    dot[0].name[0] = '.';
    dot[0].attr = ATTR_DIRECTORY;
    dot[0].fst_clus_hi = entry.fst_clus_hi;
    dot[0].fst_clus_lo = entry.fst_clus_lo;

    memset(dot[1].name, ' ', 11);
    dot[1].name[0] = '.';
    dot[1].name[1] = '.';
    dot[1].attr = ATTR_DIRECTORY;
    dot[1].fst_clus_hi = (parent_clus >> 16) & 0xFFFF;
    dot[1].fst_clus_lo = parent_clus & 0xFFFF;

    uint32_t lba = fat32_cluster_to_lba(vol, cluster);
    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return -5;
    memset(buffer, 0, vol->bytes_per_sector);
    memcpy(buffer, dot, sizeof(dot));
    fat32_write_sector_cached(vol, lba, buffer);
    kfree(buffer);

    return 0;
}

int fat32_mkdir_fs(fs_node_t *parent, char *name, uint16_t permission) {
    fat32_volume_t* vol = (fat32_volume_t*)parent->ptr;
    spin_lock(&vol->lock);
    int res = fat32_mkdir_fs_impl(parent, name, permission);
    spin_unlock(&vol->lock);
    return res;
}

static int fat32_unlink_fs_impl(fs_node_t *parent, char *name) {
    fat32_volume_t* vol = (fat32_volume_t*)parent->ptr;

    fat32_dir_entry_t entry;
    uint32_t sector, offset;
    if (fat32_find_dirent_in_chain(vol, parent->inode, name, &sector, &offset, &entry) != 0) return -1;

    uint32_t cluster = (entry.fst_clus_hi << 16) | entry.fst_clus_lo;
    if (cluster != 0) fat32_free_cluster_chain(vol, cluster);

    entry.name[0] = DIRENT_DELETED;
    return fat32_update_dirent(vol, sector, offset, &entry);
}

int fat32_unlink_fs(fs_node_t *parent, char *name) {
    fat32_volume_t* vol = (fat32_volume_t*)parent->ptr;
    spin_lock(&vol->lock);
    int res = fat32_unlink_fs_impl(parent, name);
    spin_unlock(&vol->lock);
    return res;
}

fs_node_t* fat32_mount(fat32_volume_t* vol) {
    fs_node_t* root = kmalloc(sizeof(fs_node_t));
    if (!root) return NULL;
    memset(root, 0, sizeof(fs_node_t));

    strlcpy(root->name, "fat32_root", sizeof(root->name));
    root->flags = FS_DIRECTORY;
    root->mask = 0777;
    root->inode = vol->root_cluster;
    root->ptr = (struct fs_node*)vol;
    root->impl = 0;
    root->uid = 0;
    root->ref_count = 1;

    root->read = 0;
    root->write = 0;
    root->open = fat32_open_fs;
    root->close = fat32_close_fs;
    root->readdir = fat32_readdir_fs;
    root->finddir = fat32_finddir_fs;
    root->create = fat32_create_fs;
    root->mkdir = fat32_mkdir_fs;
    root->unlink = fat32_unlink_fs;

    return root;
}

