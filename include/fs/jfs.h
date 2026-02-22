#ifndef FS_JFS_H
#define FS_JFS_H

#include <types.h>
#include <fs/vfs.h>

#define JFS_MAGIC 0x4A465321

/* Disk Layout:
 * [Superblock (1)] [Journal (64)] [InodeBitmap(1)] [BlockBitmap(1)] [Inodes(32)] [Data...]
 */

typedef struct {
    uint32_t magic;
    uint32_t block_size;
    uint32_t journal_start;
    uint32_t journal_blocks;
    uint32_t inode_start;
    uint32_t inode_blocks;
    uint32_t data_start;
    uint32_t total_blocks;
    uint32_t inode_bitmap_start;
    uint32_t block_bitmap_start;
} jfs_superblock_t;

#define JFS_TX_BEGIN  1
#define JFS_TX_DATA   2
#define JFS_TX_COMMIT 3

typedef struct {
    uint32_t tx_id;
    uint32_t type;
    uint32_t target_block;
    uint32_t checksum;
} jfs_journal_header_t;

typedef struct {
    uint32_t inode;
    uint32_t size;
    uint32_t flags;
    uint32_t blocks[12]; /* Direct blocks */
} jfs_inode_t;

typedef struct {
    uint32_t inode;
    char name[60];
} jfs_dirent_t;

fs_node_t* jfs_init(void);

#endif
