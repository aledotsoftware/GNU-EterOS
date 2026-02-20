#include <fs/overlay.h>
#include <fs/ramfs.h>
#include <string.h>
#include <mm.h>
#include <klog.h>
#include <lock.h>

/* ========================================================================= */
/* Estructuras Internas                                                      */
/* ========================================================================= */

typedef struct overlay_entry {
    fs_node_t *lower;
    fs_node_t *upper;
    fs_node_t *upper_parent; /* Needed for copy-up */
    char name[128];          /* Needed for copy-up */
    struct dirent dirent_buf;
} overlay_entry_t;

/* ========================================================================= */
/* Forward Declarations                                                      */
/* ========================================================================= */

static uint32_t overlay_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static uint32_t overlay_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static void overlay_open(fs_node_t *node);
static void overlay_close(fs_node_t *node);
static struct dirent *overlay_readdir(fs_node_t *node, uint32_t index);
static fs_node_t *overlay_finddir(fs_node_t *node, char *name);
static int overlay_create(fs_node_t *parent, char *name, uint16_t permission);
static int overlay_mkdir(fs_node_t *parent, char *name, uint16_t permission);
static int overlay_unlink(fs_node_t *parent, char *name);
static void overlay_destroy(fs_node_t *node);

/* Helpers */
static int copy_up(fs_node_t *node);
static int is_whiteout(char *name);

/* ========================================================================= */
/* Implementación                                                            */
/* ========================================================================= */

static fs_node_t *overlay_create_node(fs_node_t *lower, fs_node_t *upper, fs_node_t *upper_parent, char *name) {
    fs_node_t *node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!node) return NULL;
    memset(node, 0, sizeof(fs_node_t));

    /* Inherit attributes from upper if exists, else lower */
    fs_node_t *source = upper ? upper : lower;
    if (source) {
        strlcpy(node->name, source->name, 128);
        node->flags = source->flags;
        node->length = source->length;
        node->mask = source->mask;
        node->uid = source->uid;
        node->gid = source->gid;
    } else {
        strlcpy(node->name, name ? name : "overlay", 128);
    }

    node->inode = 0;
    node->ref_count = 1;

    node->read = overlay_read;
    node->write = overlay_write;
    node->open = overlay_open;
    node->close = overlay_close;
    node->readdir = overlay_readdir;
    node->finddir = overlay_finddir;
    node->create = overlay_create;
    node->mkdir = overlay_mkdir;
    node->unlink = overlay_unlink;
    node->destroy = overlay_destroy;

    overlay_entry_t *entry = (overlay_entry_t*)kmalloc(sizeof(overlay_entry_t));
    if (!entry) {
        kfree(node);
        return NULL;
    }
    memset(entry, 0, sizeof(overlay_entry_t));
    entry->lower = lower;
    entry->upper = upper;
    entry->upper_parent = upper_parent;
    if (upper_parent) {
        __atomic_add_fetch(&upper_parent->ref_count, 1, __ATOMIC_SEQ_CST);
    }
    if (name) strlcpy(entry->name, name, 128);

    node->private_data = entry;
    return node;
}

static void overlay_destroy(fs_node_t *node) {
    overlay_entry_t *entry = (overlay_entry_t*)node->private_data;
    if (entry) {
        /* Recursively destroy children nodes if we own them */
        /* Wait, lower/upper are nodes returned by finddir, so we own them (ref=1) */
        /* We should destroy them unless they are just references? */
        /* VFS lookup returns new nodes. So yes, we own them. */

        if (entry->lower) vfs_destroy_node(entry->lower);
        if (entry->upper) vfs_destroy_node(entry->upper);

        if (entry->upper_parent) {
            if (__atomic_sub_fetch(&entry->upper_parent->ref_count, 1, __ATOMIC_SEQ_CST) == 0) {
                close_fs(entry->upper_parent);
                vfs_destroy_node(entry->upper_parent);
            }
        }

        kfree(entry);
    }
}

static int copy_up(fs_node_t *node) {
    overlay_entry_t *entry = (overlay_entry_t*)node->private_data;
    if (entry->upper) return 0; /* Already copied up */
    if (!entry->lower) return -1; /* Nothing to copy from */
    if (!entry->upper_parent) return -1; /* Cannot create in root without parent? Logic error if root */

    /* Create file in upper */
    /* Only supports files for now, not directories (directories are merged implicitly) */
    /* If lower is directory, we just mkdir in upper? */
    if ((entry->lower->flags & 0x7) == FS_DIRECTORY) {
         /* Use mkdir for directories */
         if (mkdir_fs(entry->upper_parent, entry->name, entry->lower->mask) != 0) return -1;

         entry->upper = finddir_fs(entry->upper_parent, entry->name);
         return entry->upper ? 0 : -1;
    }

    if (create_fs(entry->upper_parent, entry->name, 0) != 0) return -1;

    fs_node_t *new_upper = finddir_fs(entry->upper_parent, entry->name);
    if (!new_upper) return -1;

    /* Copy data */
    uint32_t size = entry->lower->length;
    if (size > 0) {
        uint8_t *buf = (uint8_t*)kmalloc(size);
        if (buf) {
            read_fs(entry->lower, 0, size, buf);
            write_fs(new_upper, 0, size, buf);
            kfree(buf);
        }
    }

    entry->upper = new_upper;
    return 0;
}

static uint32_t overlay_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    overlay_entry_t *entry = (overlay_entry_t*)node->private_data;
    if (entry->upper) {
        return read_fs(entry->upper, offset, size, buffer);
    }
    if (entry->lower) {
        return read_fs(entry->lower, offset, size, buffer);
    }
    return 0;
}

static uint32_t overlay_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    overlay_entry_t *entry = (overlay_entry_t*)node->private_data;

    if (!entry->upper) {
        if (copy_up(node) != 0) return 0; /* Failed to copy up */
    }

    if (entry->upper) {
        uint32_t written = write_fs(entry->upper, offset, size, buffer);
        node->length = entry->upper->length; /* Update view */
        return written;
    }
    return 0;
}

static void overlay_open(fs_node_t *node) {
    overlay_entry_t *entry = (overlay_entry_t*)node->private_data;
    if (entry->upper) open_fs(entry->upper, 1, 1);
    if (entry->lower) open_fs(entry->lower, 1, 0);
}

static void overlay_close(fs_node_t *node) {
    overlay_entry_t *entry = (overlay_entry_t*)node->private_data;
    if (entry->upper) close_fs(entry->upper);
    if (entry->lower) close_fs(entry->lower);
}

static int is_whiteout(char *name) {
    return (strncmp(name, ".wh.", 4) == 0);
}

static struct dirent *overlay_readdir(fs_node_t *node, uint32_t index) {
    overlay_entry_t *entry = (overlay_entry_t*)node->private_data;

    /* Naive implementation:
       We need to iterate upper, then lower.
       We can't rely on simple index split because we don't know count.
       We MUST iterate.
       Strategy:
       1. Count upper items.
       2. If index < upper_count, return upper[index].
       3. Else, scan lower items. For each, check if obscured by upper (file or whiteout).
          If not obscured, decrement virtual index. If 0, return it.
    */

    /* 1. Count Upper */
    uint32_t upper_count = 0;
    if (entry->upper) {
        /* We can't easily count without iterating... this is O(N^2) for readdir sequence.
           But acceptable for now. */
        struct dirent *d;
        int i = 0;
        while ((d = readdir_fs(entry->upper, i++)) != 0) {
            if (!is_whiteout(d->name)) {
                if (index == upper_count) {
                    /* Found in upper phase */
                    memcpy(&entry->dirent_buf, d, sizeof(struct dirent));
                    return &entry->dirent_buf;
                }
                upper_count++;
            }
        }
    }

    /* 2. Scan Lower */
    if (entry->lower) {
        /* Target virtual index in lower set is: */
        uint32_t target_lower_index = index - upper_count;
        uint32_t lower_matches = 0;

        struct dirent *d;
        int i = 0;
        while ((d = readdir_fs(entry->lower, i++)) != 0) {
            /* Check if obscured by upper */
            int obscured = 0;
            if (entry->upper) {
                /* Check if file exists in upper */
                fs_node_t *up = finddir_fs(entry->upper, d->name);
                if (up) {
                    obscured = 1;
                    vfs_destroy_node(up);
                } else {
                    /* Check whiteout */
                    char wh_name[128];
                    strlcpy(wh_name, ".wh.", 128);
                    strlcat(wh_name, d->name, 128);
                    fs_node_t *wh = finddir_fs(entry->upper, wh_name);
                    if (wh) {
                        obscured = 1;
                        vfs_destroy_node(wh);
                    }
                }
            }

            if (!obscured) {
                if (lower_matches == target_lower_index) {
                    memcpy(&entry->dirent_buf, d, sizeof(struct dirent));
                    return &entry->dirent_buf;
                }
                lower_matches++;
            }
        }
    }

    return NULL;
}

static fs_node_t *overlay_finddir(fs_node_t *node, char *name) {
    overlay_entry_t *entry = (overlay_entry_t*)node->private_data;

    /* Check whiteout first */
    if (entry->upper) {
        char wh_name[128];
        strlcpy(wh_name, ".wh.", 128);
        strlcat(wh_name, name, 128);
        fs_node_t *wh = finddir_fs(entry->upper, wh_name);
        if (wh) {
            vfs_destroy_node(wh);
            return NULL; /* Whiteout exists, file is hidden */
        }
    }

    fs_node_t *upper_child = entry->upper ? finddir_fs(entry->upper, name) : NULL;
    fs_node_t *lower_child = entry->lower ? finddir_fs(entry->lower, name) : NULL;

    if (!upper_child && !lower_child) return NULL;

    return overlay_create_node(lower_child, upper_child, entry->upper, name);
}

static int overlay_create(fs_node_t *parent, char *name, uint16_t permission) {
    overlay_entry_t *entry = (overlay_entry_t*)parent->private_data;

    if (!entry->upper) {
        if (copy_up(parent) != 0) return -1;
    }

    /* If it was whiteout-ed, we might need to remove whiteout?
       Standard implementation: creating a file replaces whiteout. */
    char wh_name[128];
    strlcpy(wh_name, ".wh.", 128);
    strlcat(wh_name, name, 128);
    fs_node_t *wh = finddir_fs(entry->upper, wh_name);
    if (wh) {
        unlink_fs(entry->upper, wh_name);
        vfs_destroy_node(wh);
    }

    return create_fs(entry->upper, name, permission);
}

static int overlay_mkdir(fs_node_t *parent, char *name, uint16_t permission) {
    overlay_entry_t *entry = (overlay_entry_t*)parent->private_data;

    if (!entry->upper) {
        if (copy_up(parent) != 0) return -1;
    }

    /* Remove whiteout if exists */
    char wh_name[128];
    strlcpy(wh_name, ".wh.", 128);
    strlcat(wh_name, name, 128);
    fs_node_t *wh = finddir_fs(entry->upper, wh_name);
    if (wh) {
        unlink_fs(entry->upper, wh_name);
        vfs_destroy_node(wh);
    }

    return mkdir_fs(entry->upper, name, permission);
}

static int overlay_unlink(fs_node_t *parent, char *name) {
    overlay_entry_t *entry = (overlay_entry_t*)parent->private_data;

    if (!entry->upper) {
        if (copy_up(parent) != 0) return -1;
    }

    int in_upper = 0;
    fs_node_t *up = finddir_fs(entry->upper, name);
    if (up) {
        in_upper = 1;
        vfs_destroy_node(up);
    }

    int in_lower = 0;
    if (entry->lower) {
        fs_node_t *lo = finddir_fs(entry->lower, name);
        if (lo) {
            in_lower = 1;
            vfs_destroy_node(lo);
        }
    }

    if (!in_upper && !in_lower) return -1; /* ENOENT */

    /* Delete from upper */
    if (in_upper) {
        if (unlink_fs(entry->upper, name) != 0) return -1;
    }

    /* Mask lower */
    if (in_lower) {
        char wh_name[128];
        strlcpy(wh_name, ".wh.", 128);
        strlcat(wh_name, name, 128);
        if (create_fs(entry->upper, wh_name, 0) != 0) return -1;
    }

    return 0;
}

/* ========================================================================= */
/* Initialization                                                            */
/* ========================================================================= */

fs_node_t *overlay_init(fs_node_t *lower, fs_node_t *upper) {
    if (!lower || !upper) return NULL;
    return overlay_create_node(lower, upper, NULL, NULL); /* Root has no parent */
}
