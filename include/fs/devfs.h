#ifndef FS_DEVFS_H
#define FS_DEVFS_H

#include <fs/vfs.h>

/**
 * Initializes the Device Filesystem (DevFS).
 * @return The root node of DevFS.
 */
fs_node_t* devfs_init(void);

#endif
