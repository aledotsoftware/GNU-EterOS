#include <fs/jfs.h>
#include <fs/vfs.h>
#include <mm.h>
#include <string.h>
#include <hal.h>
#include <klog.h>
#include <stdio.h>
#include <serial.h>

/* Simulated Disk */
static uint8_t *jfs_disk_buffer = NULL;
static uint32_t jfs_disk_size = 4 * 1024 * 1024; /* 4MB */
static jfs_superblock_t *sb = NULL;
static uint32_t current_tx_id = 1;
static uint32_t jfs_next_free_block = 0;

static void disk_read(uint32_t block, void *buffer);
static void disk_write(uint32_t block, const void *buffer);
static void jfs_journal_write(uint32_t target_block, const void *data);
static jfs_inode_t* get_inode(uint32_t inode_idx);
static ssize_t jfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static uint32_t jfs_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static int jfs_readdir(fs_node_t *node, uint32_t index, struct dirent *entry);
static fs_node_t* jfs_finddir(fs_node_t *node, char *name);
static int jfs_create(fs_node_t *parent, char *name, uint16_t permission);
static int jfs_mkdir(fs_node_t *parent, char *name, uint16_t permission);

static int jfs_is_directory(uint32_t inode_idx) {
    return (get_inode(inode_idx)->flags & FS_DIRECTORY) == FS_DIRECTORY;
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
    memset(new_node, 0, sizeof(jfs_inode_t));
    new_node->inode = free_inode;
    new_node->size = 0;
    new_node->flags = flags;
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
    if ((dir->flags & FS_DIRECTORY) != FS_DIRECTORY) return -1;

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
                return 0;
            }
        }
    }

    return -1;
}

static int jfs_add_entry(uint32_t dir_inode, const char *name, uint32_t child_inode) {
    jfs_inode_t *dir = get_inode(dir_inode);
    if ((dir->flags & FS_DIRECTORY) != FS_DIRECTORY) return -1;
    if (jfs_find_entry(dir_inode, name, NULL, NULL, NULL) == 0) return -1;

    for (uint32_t i = 0; i < 12; i++) {
        if (jfs_ensure_dir_block(dir, i) != 0) return -1;

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
                return 0;
            }
        }
    }

    return -1;
}

static fs_node_t* jfs_make_node(uint32_t inode_idx, const char *name) {
    jfs_inode_t *inode = get_inode(inode_idx);
    fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fnode) return NULL;

    memset(fnode, 0, sizeof(fs_node_t));
    fnode->inode = inode_idx;
    fnode->length = inode->size;
    fnode->flags = inode->flags;
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

    return fnode;
}

/* Helpers for disk I/O */
static void disk_read(uint32_t block, void *buffer) {
    if (block * 512 >= jfs_disk_size) return;
    memcpy(buffer, jfs_disk_buffer + (block * 512), 512);
}

static void disk_write(uint32_t block, const void *buffer) {
    if (block * 512 >= jfs_disk_size) return;
    memcpy(jfs_disk_buffer + (block * 512), buffer, 512);
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
    /* Simple: Inodes are stored in blocks starting at inode_start.
       Assume 512 bytes / sizeof(jfs_inode_t) inodes per block.
       sizeof(jfs_inode_t) = 4 + 4 + 4 + 12*4 = 60 bytes? No 12*4=48. Total 60.
       Padding to 64 bytes? Let's assume packed for simulation or just use direct offset.
    */
    uint32_t inodes_per_block = 512 / sizeof(jfs_inode_t);
    uint32_t block = sb->inode_start + (inode_idx / inodes_per_block);
    uint32_t offset = (inode_idx % inodes_per_block) * sizeof(jfs_inode_t);

    /* We need to read the block, modify, write back.
       For this simulation, we just return a pointer to the memory buffer directly
       since our disk is RAM.
    */
    uint8_t *ptr = jfs_disk_buffer + (block * 512) + offset;
    return (jfs_inode_t*)ptr;
}

/* VFS Interface */
static ssize_t jfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (jfs_is_directory(node->inode)) return 0;
    jfs_inode_t *inode = get_inode(node->inode);
    if (offset >= inode->size) return 0;
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
    return read_bytes;
}

static uint32_t jfs_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (jfs_is_directory(node->inode)) return 0;
    jfs_inode_t *inode = get_inode(node->inode);

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

    if (offset + written_bytes > inode->size) inode->size = offset + written_bytes;
    node->length = inode->size;
    return written_bytes;
}

static int jfs_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) {
    jfs_inode_t *dir = get_inode(node->inode);
    if ((dir->flags & FS_DIRECTORY) != FS_DIRECTORY) return -1;

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
            return 0;
        }
    }

    return 1;
}

static fs_node_t* jfs_finddir(fs_node_t *node, char *name) {
    jfs_dirent_t found;
    if (jfs_find_entry(node->inode, name, &found, NULL, NULL) != 0) return 0;
    return jfs_make_node(found.inode, found.name);
}

static int jfs_create(fs_node_t *parent, char *name, uint16_t permission) {
    (void)permission;
    if (!parent || !name || !*name) return -1;
    if (!jfs_is_directory(parent->inode)) return -1;

    uint32_t free_inode = 0;
    if (jfs_alloc_inode(FS_FILE, &free_inode) != 0) return -1;
    return jfs_add_entry(parent->inode, name, free_inode);
}

static int jfs_mkdir(fs_node_t *parent, char *name, uint16_t permission) {
    (void)permission;
    if (!parent || !name || !*name) return -1;
    if (!jfs_is_directory(parent->inode)) return -1;

    uint32_t free_inode = 0;
    if (jfs_alloc_inode(FS_DIRECTORY, &free_inode) != 0) return -1;
    if (jfs_ensure_dir_block(get_inode(free_inode), 0) != 0) return -1;
    return jfs_add_entry(parent->inode, name, free_inode);
}

/* Init */
fs_node_t* jfs_init(void) {
    char dbg_buf[64];
    serial_write_string("[JFS] Initializing (Size: ");
    itoa_s(jfs_disk_size, dbg_buf, sizeof(dbg_buf), 10);
    serial_write_string(dbg_buf);
    serial_write_string(")...\n");

    jfs_disk_buffer = (uint8_t*)kmalloc(jfs_disk_size);
    if (!jfs_disk_buffer) {
        serial_write_string("[JFS] ERROR: OOM allocating disk buffer!\n");
        return NULL;
    }

    serial_write_string("[JFS] Buffer allocated at 0x");
    utoa_hex_s((uint64_t)jfs_disk_buffer, dbg_buf, sizeof(dbg_buf));
    serial_write_string(dbg_buf);
    serial_write_string("\n");

    serial_write_string("[JFS] Clearing buffer...\n");
    memset(jfs_disk_buffer, 0, jfs_disk_size);
    serial_write_string("[JFS] Buffer cleared.\n");

    /* Format Disk */
    sb = (jfs_superblock_t*)jfs_disk_buffer;
    sb->magic = JFS_MAGIC;
    sb->block_size = 512;
    sb->journal_start = 1;
    sb->journal_blocks = 64;
    sb->inode_start = 65;
    sb->inode_blocks = 32;
    sb->data_start = 100;
    sb->total_blocks = jfs_disk_size / 512;

    /* Create Root Inode (0) */
    jfs_inode_t *root = get_inode(0);
    root->inode = 0;
    root->size = 0;
    root->flags = FS_DIRECTORY;
    /* Alloc block 0 for root dir entries */
    root->blocks[0] = sb->data_start; /* Block 100 */
    jfs_next_free_block = sb->data_start + 1;

    fs_node_t *fs = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fs) return NULL;
    memset(fs, 0, sizeof(fs_node_t));
    strlcpy(fs->name, "jfs_root", sizeof(fs->name));
    fs->flags = FS_DIRECTORY;
    fs->inode = 0;
    fs->readdir = jfs_readdir;
    fs->finddir = jfs_finddir;
    fs->create = jfs_create;
    fs->mkdir = jfs_mkdir;

    hal_console_write("[JFS] Initialized (4MB RAM Disk). Journaling Enabled.\n");
    return fs;
}
