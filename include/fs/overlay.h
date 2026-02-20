#ifndef FS_OVERLAY_H
#define FS_OVERLAY_H

#include <fs/vfs.h>

/**
 * Mounts an OverlayFS with the given lower (Read-Only) and upper (Read-Write) roots.
 * Returns the root node of the overlay.
 */
fs_node_t *overlay_init(fs_node_t *lower, fs_node_t *upper);

#endif
