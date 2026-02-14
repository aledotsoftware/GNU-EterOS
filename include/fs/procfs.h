#ifndef FS_PROCFS_H
#define FS_PROCFS_H

#include <fs/vfs.h>

/**
 * Initializes the Process Filesystem (ProcFS).
 * @return The root node of ProcFS.
 */
fs_node_t* procfs_init(void);

#endif
