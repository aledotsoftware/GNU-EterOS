#ifndef FS_DEVFS_H
#define FS_DEVFS_H

#include <fs/vfs.h>

/**
 * Initializes the Device Filesystem (DevFS).
 * @return The root node of DevFS.
 */
fs_node_t* devfs_init(void);

/**
 * Generates cryptographically secure pseudo-random bytes.
 * @param buf Pointer to the buffer to fill.
 * @param len Number of bytes to generate.
 */
void get_random_bytes(uint8_t *buf, size_t len);

#endif
