#ifndef FS_RAMFS_H
#define FS_RAMFS_H

#include <fs/vfs.h>

/**
 * Initializes a new RamFS instance and returns the root node.
 */
fs_node_t *ramfs_init(void);

#endif
