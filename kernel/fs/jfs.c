#include <fs/jfs.h>
#include <fs/vfs.h>
#include <fs/bcache.h>
#include <drivers/disk.h>
#include <mm.h>
#include <string.h>
#include <hal.h>
#include <klog.h>
#include <stdio.h>
#include <serial.h>

/* Real Disk Partition */
static fs_node_t *jfs_part_node = NULL;
static jfs_superblock_t jfs_sb_cache;
static jfs_superblock_t *sb = &jfs_sb_cache;
static uint32_t current_tx_id = 1;
static uint32_t jfs_next_free_block = 0;

#define JFS_MAX_INODES 4096
static uint16_t jfs_inode_mode[JFS_MAX_INODES];
static uint32_t jfs_inode_uid[JFS_MAX_INODES];
static uint32_t jfs_inode_gid[JFS_MAX_INODES];

static void disk_read(uint32_t block, void *buffer);
static void disk_write(uint32_t block, const void *buffer);
static void jfs_journal_write(uint32_t target_block, const void *data);
static jfs_inode_t* get_inode(uint32_t inode_idx);
static void put_inode(jfs_inode_t* inode_ptr, uint32_t inode_idx);
static ssize_t jfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static uint32_t jfs_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static int jfs_readdir(fs_node_t *node, uint32_t index, struct dirent *entry);
static fs_node_t* jfs_finddir(fs_node_t *node, char *name);
static int jfs_create(fs_node_t *parent, char *name, uint16_t permission);
static int jfs_mkdir(fs_node_t *parent, char *name, uint16_t permission);

static void jfs_set_inode_metadata(uint32_t inode_idx, uint16_t mode, uint32_t uid, uint32_t gid) {
    if (inode_idx >= JFS_MAX_INODES) return;
    jfs_inode_mode[inode_idx] = (uint16_t)(mode & 0777);
    jfs_inode_uid[inode_idx] = uid;
    jfs_inode_gid[inode_idx] = gid;
}

static int jfs_is_directory(uint32_t inode_idx) {
    jfs_inode_t* inode = get_inode(inode_idx);
    if (!inode) return 0;
    int ret = (inode->flags & FS_DIRECTORY) == FS_DIRECTORY;
    kfree(inode);
    return ret;
}

static int jfs_alloc_block(uint32_t* out_block) {
    if (!out_block) return -1;
    if (jfs_next_free_block >= sb->total_blocks) return -1;
    *out_block = jfs_next_free_block++;
    return 0;
}

static int jfs_alloc_inode(uint32_t flags, uint32_t* out_inode) {
    static uint32_t next_inode = 1;
    if (!out_inode) return -1;

    uint32_t free_inode = next_inode++;
    jfs_inode_t *new_node = get_inode(free_inode);
    if (!new_node) return -1;
    memset(new_node, 0, sizeof(jfs_inode_t));
    new_node->inode = free_inode;
    new_node->size = 0;
    new_node->flags = flags;
    put_inode(new_node, free_inode);
    jfs_set_inode_metadata(free_inode, (flags & FS_DIRECTORY) ? 0755 : 0644, 0, 0);
    kfree(new_node);
    *out_inode = free_inode;
    return 0;
}

static int jfs_ensure_dir_block(jfs_inode_t *dir, uint32_t block_idx) {
    if (!dir || block_idx >= 12) return -1;
    if (dir->blocks[block_idx] != 0) return 0;

    uint32_t new_block = 0;
    if (jfs_alloc_block(&new_block) != 0) return -1;

    dir->blocks[block_idx] = new_block;
    uint8_t zeroes[512];
    memset(zeroes, 0, sizeof(zeroes));
    disk_write(new_block, zeroes);
    return 0;
}

static int jfs_find_entry(uint32_t dir_inode, const char *name, jfs_dirent_t *out_entry, uint32_t *out_block, uint32_t *out_index) {
    jfs_inode_t *dir = get_inode(dir_inode);
    if (!dir) return -1;
    if ((dir->flags & FS_DIRECTORY) != FS_DIRECTORY) { kfree(dir); return -1; }

    for (uint32_t i = 0; i < 12; i++) {
        if (dir->blocks[i] == 0) continue;

        uint8_t sector[512];
        disk_read(dir->blocks[i], sector);
        jfs_dirent_t *entries = (jfs_dirent_t*)sector;
        uint32_t count = 512 / sizeof(jfs_dirent_t);

        for (uint32_t j = 0; j < count; j++) {
            if (entries[j].inode != 0 && strcmp(entries[j].name, name) == 0) {
                if (out_entry) *out_entry = entries[j];
                if (out_block) *out_block = dir->blocks[i];
                if (out_index) *out_index = j;
                kfree(dir);
                return 0;
            }
        }
    }

    kfree(dir);
    return -1;
}

static int jfs_add_entry(uint32_t dir_inode, const char *name, uint32_t child_inode) {
    jfs_inode_t *dir = get_inode(dir_inode);
    if (!dir) return -1;
    if ((dir->flags & FS_DIRECTORY) != FS_DIRECTORY) { kfree(dir); return -1; }
    if (jfs_find_entry(dir_inode, name, NULL, NULL, NULL) == 0) { kfree(dir); return -1; }

    for (uint32_t i = 0; i < 12; i++) {
        if (jfs_ensure_dir_block(dir, i) != 0) { kfree(dir); return -1; }
        put_inode(dir, dir_inode);

        uint8_t sector[512];
        disk_read(dir->blocks[i], sector);
        jfs_dirent_t *entries = (jfs_dirent_t*)sector;
        uint32_t count = 512 / sizeof(jfs_dirent_t);

        for (uint32_t j = 0; j < count; j++) {
            if (entries[j].inode == 0) {
                entries[j].inode = child_inode;
                strlcpy(entries[j].name, name, sizeof(entries[j].name));
                jfs_journal_write(dir->blocks[i], sector);
                dir->size += sizeof(jfs_dirent_t);
                put_inode(dir, dir_inode);
                kfree(dir);
                return 0;
            }
        }
    }

    kfree(dir);
    return -1;
}

static fs_node_t* jfs_make_node(uint32_t inode_idx, const char *name) {
    jfs_inode_t *inode = get_inode(inode_idx);
    if (!inode) return NULL;
    fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fnode) { kfree(inode); return NULL; }

    memset(fnode, 0, sizeof(fs_node_t));
    fnode->inode = inode_idx;
    fnode->length = inode->size;
    fnode->flags = inode->flags;
    fnode->mask = (inode_idx < JFS_MAX_INODES) ? jfs_inode_mode[inode_idx] : ((inode->flags & FS_DIRECTORY) ? 0755 : 0644);
    fnode->uid = (inode_idx < JFS_MAX_INODES) ? jfs_inode_uid[inode_idx] : 0;
    fnode->gid = (inode_idx < JFS_MAX_INODES) ? jfs_inode_gid[inode_idx] : 0;
    strlcpy(fnode->name, name, sizeof(fnode->name));

    if ((inode->flags & FS_DIRECTORY) == FS_DIRECTORY) {
        fnode->readdir = jfs_readdir;
        fnode->finddir = jfs_finddir;
        fnode->create = jfs_create;
        fnode->mkdir = jfs_mkdir;
    } else {
        fnode->read = jfs_read;
        fnode->write = jfs_write;
    }

    kfree(inode);
    return fnode;
}

/* Helpers for disk I/O */
static void disk_read(uint32_t block, void *buffer) {
    if (!jfs_part_node) return;
    /* Always read through cache */
    if (bcache_read(jfs_part_node->impl, block, buffer) != 0) {
        /* Cache miss, read from partition and cache it */
        if (jfs_part_node->read) {
            jfs_part_node->read(jfs_part_node, block * 512, 512, buffer);
        }
        bcache_write(jfs_part_node->impl, block, buffer);
    }
}

static void disk_write(uint32_t block, const void *buffer) {
    if (!jfs_part_node) return;

    uint8_t temp[512];
    memcpy(temp, buffer, 512);

    if (jfs_part_node->write) {
        jfs_part_node->write(jfs_part_node, block * 512, 512, temp);
    }
    bcache_write(jfs_part_node->impl, block, temp);
}

/* Journaling Core */
static void jfs_journal_write(uint32_t target_block, const void *data) {
    /*
     * In a real implementation, we would append to the journal log.
     * Here we simulate a single-entry transaction for simplicity.
     * Journal starts at block sb->journal_start.
     */

    uint32_t j_start = sb->journal_start;
    uint8_t sector[512];
    jfs_journal_header_t *hdr = (jfs_journal_header_t*)sector;

    /* 1. Write TX_BEGIN */
    memset(sector, 0, 512);
    hdr->tx_id = current_tx_id;
    hdr->type = JFS_TX_BEGIN;
    disk_write(j_start, sector);
    // kprintf("[JFS] Journal: TX %d BEGIN\n", current_tx_id);

    /* 2. Write TX_DATA (Metadata) */
    hdr->type = JFS_TX_DATA;
    hdr->target_block = target_block;
    disk_write(j_start + 1, sector);

    /* 3. Write Actual Data to Journal */
    disk_write(j_start + 2, data);
    // kprintf("[JFS] Journal: TX %d DATA (Block %d)\n", current_tx_id, target_block);

    /* 4. Write TX_COMMIT */
    memset(sector, 0, 512);
    hdr->tx_id = current_tx_id;
    hdr->type = JFS_TX_COMMIT;
    disk_write(j_start + 3, sector);
    // kprintf("[JFS] Journal: TX %d COMMIT\n", current_tx_id);

    /* 5. Checkpoint: Write to actual location */
    disk_write(target_block, data);
    // kprintf("[JFS] Checkpoint: Block %d written.\n", target_block);

    current_tx_id++;
}

/* Inode Management */
static jfs_inode_t* get_inode(uint32_t inode_idx) {
    uint32_t inodes_per_block = 512 / sizeof(jfs_inode_t);
    uint32_t block = sb->inode_start + (inode_idx / inodes_per_block);
    uint32_t offset = (inode_idx % inodes_per_block) * sizeof(jfs_inode_t);

    uint8_t sector[512];
    disk_read(block, sector);

    jfs_inode_t *inode = kmalloc(sizeof(jfs_inode_t));
    if (!inode) return NULL;
    memcpy(inode, sector + offset, sizeof(jfs_inode_t));
    return inode;
}

static void put_inode(jfs_inode_t* inode_ptr, uint32_t inode_idx) {
    uint32_t inodes_per_block = 512 / sizeof(jfs_inode_t);
    uint32_t block = sb->inode_start + (inode_idx / inodes_per_block);
    uint32_t offset = (inode_idx % inodes_per_block) * sizeof(jfs_inode_t);

    uint8_t sector[512];
    disk_read(block, sector);

    memcpy(sector + offset, inode_ptr, sizeof(jfs_inode_t));
    disk_write(block, sector);
}

/* VFS Interface */
static ssize_t jfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (jfs_is_directory(node->inode)) return 0;
    jfs_inode_t *inode = get_inode(node->inode);
    if (!inode) return 0;
    if (offset >= inode->size) { kfree(inode); return 0; }
    if (offset + size > inode->size) size = inode->size - offset;

    uint32_t read_bytes = 0;
    while (read_bytes < size) {
        uint32_t block_idx = (offset + read_bytes) / 512;
        uint32_t block_off = (offset + read_bytes) % 512;

        if (block_idx >= 12) break; /* Only direct blocks supported for now */

        uint32_t disk_block = inode->blocks[block_idx];
        if (disk_block == 0) {
            /* Sparse file hole */
            memset(buffer + read_bytes, 0, 1); // Simplification, should fill chunk
            read_bytes++;
            continue;
        }

        uint8_t sector[512];
        disk_read(disk_block, sector);

        uint32_t chunk = 512 - block_off;
        if (chunk > size - read_bytes) chunk = size - read_bytes;

        memcpy(buffer + read_bytes, sector + block_off, chunk);
        read_bytes += chunk;
    }
    kfree(inode);
    return read_bytes;
}

static uint32_t jfs_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (jfs_is_directory(node->inode)) return 0;
    jfs_inode_t *inode = get_inode(node->inode);
    if (!inode) return 0;

    /* Auto-allocate blocks if needed */
    uint32_t written_bytes = 0;

    while (written_bytes < size) {
        uint32_t block_idx = (offset + written_bytes) / 512;
        uint32_t block_off = (offset + written_bytes) % 512;

        if (block_idx >= 12) break;

        if (inode->blocks[block_idx] == 0) {
            /* Alloc new block - primitive allocator: scan for free from data start */
            /* In this toy FS, we just increment a global counter or scan. */
            inode->blocks[block_idx] = jfs_next_free_block++;
        }

        uint32_t disk_block = inode->blocks[block_idx];

        uint8_t sector[512];
        disk_read(disk_block, sector); /* Read-Modify-Write */

        uint32_t chunk = 512 - block_off;
        if (chunk > size - written_bytes) chunk = size - written_bytes;

        memcpy(sector + block_off, buffer + written_bytes, chunk);

        /* WRITE THROUGH JOURNAL */
        jfs_journal_write(disk_block, sector);

        written_bytes += chunk;
    }

    if (offset + written_bytes > inode->size) {
        inode->size = offset + written_bytes;
        put_inode(inode, node->inode);
    }
    node->length = inode->size;
    kfree(inode);
    return written_bytes;
}

static int jfs_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) {
    jfs_inode_t *dir = get_inode(node->inode);
    if (!dir) return -1;
    if ((dir->flags & FS_DIRECTORY) != FS_DIRECTORY) { kfree(dir); return -1; }

    uint32_t seen = 0;
    for (uint32_t i = 0; i < 12; i++) {
        if (dir->blocks[i] == 0) continue;

        uint8_t sector[512];
        disk_read(dir->blocks[i], sector);
        jfs_dirent_t *entries = (jfs_dirent_t*)sector;
        uint32_t count = 512 / sizeof(jfs_dirent_t);

        for (uint32_t j = 0; j < count; j++) {
            if (entries[j].inode == 0) continue;
            if (seen++ != index) continue;

            strlcpy(entry->name, entries[j].name, sizeof(entry->name));
            entry->inode = entries[j].inode;
            kfree(dir);
            return 0;
        }
    }

    kfree(dir);
    return 1;
}

static fs_node_t* jfs_finddir(fs_node_t *node, char *name) {
    jfs_dirent_t found;
    if (jfs_find_entry(node->inode, name, &found, NULL, NULL) != 0) return 0;
    return jfs_make_node(found.inode, found.name);
}

static int jfs_create(fs_node_t *parent, char *name, uint16_t permission) {
    if (!parent || !name || !*name) return -1;
    if (!jfs_is_directory(parent->inode)) return -1;

    uint32_t free_inode = 0;
    if (jfs_alloc_inode(FS_FILE, &free_inode) != 0) return -1;
    jfs_set_inode_metadata(free_inode, permission ? permission : 0644, parent->uid, parent->gid);
    return jfs_add_entry(parent->inode, name, free_inode);
}

static int jfs_mkdir(fs_node_t *parent, char *name, uint16_t permission) {
    if (!parent || !name || !*name) return -1;
    if (!jfs_is_directory(parent->inode)) return -1;

    uint32_t free_inode = 0;
    if (jfs_alloc_inode(FS_DIRECTORY, &free_inode) != 0) return -1;
    jfs_set_inode_metadata(free_inode, permission ? permission : 0755, parent->uid, parent->gid);
    jfs_inode_t* free_inode_ptr = get_inode(free_inode);
    if (!free_inode_ptr) return -1;
    if (jfs_ensure_dir_block(free_inode_ptr, 0) != 0) { kfree(free_inode_ptr); return -1; }
    put_inode(free_inode_ptr, free_inode);
    kfree(free_inode_ptr);
    return jfs_add_entry(parent->inode, name, free_inode);
}

/* Init */
fs_node_t* jfs_init(void) {
    serial_write_string("[JFS] Initializing using block cache & active partition...\n");

    jfs_part_node = partition_get_active_root();
    if (!jfs_part_node) {
        serial_write_string("[JFS] ERROR: No active partition found!\n");
        return NULL;
    }

    memset(jfs_inode_mode, 0, sizeof(jfs_inode_mode));
    memset(jfs_inode_uid, 0, sizeof(jfs_inode_uid));
    memset(jfs_inode_gid, 0, sizeof(jfs_inode_gid));

    uint8_t sb_sector[512];
    disk_read(0, sb_sector);
    memcpy(sb, sb_sector, sizeof(jfs_superblock_t));

    if (sb->magic != JFS_MAGIC) {
        serial_write_string("[JFS] Magic not found. Formatting partition...\n");
        sb->magic = JFS_MAGIC;
        sb->block_size = 512;
        sb->journal_start = 1;
        sb->journal_blocks = 64;
        sb->inode_start = 65;
        sb->inode_blocks = 32;
        sb->data_start = 100;
        sb->total_blocks = jfs_part_node->length / 512;

        memset(sb_sector, 0, 512);
        memcpy(sb_sector, sb, sizeof(jfs_superblock_t));
        disk_write(0, sb_sector);

        /* Create Root Inode (0) */
        jfs_inode_t *root = get_inode(0);
        if (root) {
            root->inode = 0;
            root->size = 0;
            root->flags = FS_DIRECTORY;
            jfs_set_inode_metadata(0, 0777, 0, 0);
            /* Alloc block 0 for root dir entries */
            root->blocks[0] = sb->data_start; /* Block 100 */
            put_inode(root, 0);
            kfree(root);
        }
        jfs_next_free_block = sb->data_start + 1;
    } else {
        serial_write_string("[JFS] Magic found. Mounting partition...\n");
        /* Assume everything is fine, though we should really find jfs_next_free_block by scanning */
        jfs_next_free_block = sb->data_start + 1;
    }

    fs_node_t *fs = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fs) return NULL;
    memset(fs, 0, sizeof(fs_node_t));
    strlcpy(fs->name, "jfs_root", sizeof(fs->name));
    fs->flags = FS_DIRECTORY;
    fs->inode = 0;
    fs->mask = 0777;
    fs->uid = 0;
    fs->gid = 0;
    fs->readdir = jfs_readdir;
    fs->finddir = jfs_finddir;
    fs->create = jfs_create;
    fs->mkdir = jfs_mkdir;

    hal_console_write("[JFS] Initialized with disk backend. Journaling Enabled.\n");
    return fs;
}
