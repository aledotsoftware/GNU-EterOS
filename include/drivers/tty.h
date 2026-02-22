#ifndef DRIVERS_TTY_H
#define DRIVERS_TTY_H

#include <fs/vfs.h>

/**
 * Creates and initializes a TTY device node.
 *
 * @return A pointer to the newly allocated fs_node_t, or NULL on failure.
 *         The returned node must be freed by the caller using kfree().
 */
fs_node_t* tty_create_node(void);

#endif /* DRIVERS_TTY_H */
