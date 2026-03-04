#include <fs/vfs.h>
#include <string.h>
#include <mm.h>

#ifndef __ETEROS_HOST_TEST__
fs_node_t *fs_root = NULL;
#include <task.h>
#endif

struct mount_point {
    uint32_t inode;
    uint32_t impl;
    fs_node_t *fs;
    struct mount_point *next;
};

static struct mount_point *mounts = NULL;

ssize_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (node->read != 0) {
        spin_lock(&node->lock);
        ssize_t ret = node->read(node, offset, size, buffer);
        spin_unlock(&node->lock);
        return ret;
    }
    return 0;
}

uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (node->write != 0) {
        spin_lock(&node->lock);
        uint32_t ret = node->write(node, offset, size, buffer);
        spin_unlock(&node->lock);
        return ret;
    }
    return 0;
}

void open_fs(fs_node_t *node, uint8_t read, uint8_t write) {
    if (node->open != 0) {
        spin_lock(&node->lock);
        node->open(node);
        spin_unlock(&node->lock);
    }
    (void)read;
    (void)write;
}

void close_fs(fs_node_t *node) {
    if (node->close != 0) {
        spin_lock(&node->lock);
        node->close(node);
        spin_unlock(&node->lock);
    }
}

int readdir_fs(fs_node_t *node, uint32_t index, struct dirent *entry) {
    if ((node->flags & 0x7) == FS_DIRECTORY && node->readdir != 0) {
        spin_lock(&node->lock);
        int ret = node->readdir(node, index, entry);
        spin_unlock(&node->lock);
        return ret;
    }
    return -1;
}

fs_node_t *finddir_fs(fs_node_t *node, char *name) {
    if ((node->flags & 0x7) == FS_DIRECTORY && node->finddir != 0) {
        spin_lock(&node->lock);
        fs_node_t* ret = node->finddir(node, name);
        spin_unlock(&node->lock);

        if (ret) {
            /* Check for mount points */
            struct mount_point *m = mounts;
            while (m) {
                 if (m->inode == ret->inode && m->impl == ret->impl) {
                     kfree(ret);
                     fs_node_t* clone = (fs_node_t*)kmalloc(sizeof(fs_node_t));
                     if (!clone) return NULL;
                     memcpy(clone, m->fs, sizeof(fs_node_t));
                     clone->ref_count = 1;
                     /* Inherit name from mount point? usually mount root has its own name e.g. "dev" */
                     /* But we are looking up "dev" in parent, so returning devfs root which is named "dev" matches. */
                     return clone;
                 }
                 m = m->next;
            }
        }
        return ret;
    }
    return 0;
}

int create_fs(fs_node_t *parent, char *name, uint16_t permission) {
    if ((parent->flags & 0x7) == FS_DIRECTORY && parent->create != 0)
        return parent->create(parent, name, permission);
    return -1;
}

int mkdir_fs(fs_node_t *parent, char *name, uint16_t permission) {
    if ((parent->flags & 0x7) == FS_DIRECTORY && parent->mkdir != 0)
        return parent->mkdir(parent, name, permission);
    return -1;
}

int unlink_fs(fs_node_t *parent, char *name) {
    if ((parent->flags & 0x7) == FS_DIRECTORY && parent->unlink != 0)
        return parent->unlink(parent, name);
    return -1;
}

int ioctl_fs(fs_node_t *node, int request, void *arg) {
    if (node->ioctl != 0) {
        spin_lock(&node->lock);
        int ret = node->ioctl(node, request, arg);
        spin_unlock(&node->lock);
        return ret;
    }
    return -1;
}

int vfs_mount(const char *path, fs_node_t *fs) {
    if (!path || !fs) return -1;

    fs_node_t *node = vfs_lookup(fs_root, path);
    if (!node) return -1;

    struct mount_point *mp = (struct mount_point*)kmalloc(sizeof(struct mount_point));
    if (!mp) {
        kfree(node);
        return -2;
    }

    mp->inode = node->inode;
    mp->impl = node->impl;
    mp->fs = fs;
    mp->next = mounts;
    mounts = mp;

    kfree(node);
    return 0;
}

int vfs_mkdir(const char *path, uint16_t permission) {
    if (!path) return -1;

    char parent_path[128];
    char name[128];
    int len = strlen(path);
    int last_slash = -1;

    /* Find last slash */
    for (int i = 0; i < len; i++) {
        if (path[i] == '/') last_slash = i;
    }

    if (last_slash == -1) {
        /* No slash? If relative paths not supported, fail.
           But if we assume relative to root if only name given?
           Usually syscalls handle CWD. VFS here seems absolute.
           Lets assume invalid if no slash for now unless we default to root. */
           return -1;
    }

    /* Split path */
    if (last_slash == 0) {
        /* "/name" */
        strlcpy(parent_path, "/", sizeof(parent_path));
        strlcpy(name, path + 1, sizeof(name));
    } else {
        /* "/path/to/name" */
        if (last_slash >= 127) return -1;
        memcpy(parent_path, path, last_slash);
        parent_path[last_slash] = '\0';
        strlcpy(name, path + last_slash + 1, sizeof(name));
    }

    fs_node_t *parent = vfs_lookup(fs_root, parent_path);
    if (!parent) return -1;

    int ret = mkdir_fs(parent, name, permission);
    kfree(parent);
    return ret;
}

extern fs_node_t* tty_create_node(void);

int vfs_normalize_path(char* out_path, int size, const char* path, const char* base_dir) {
    if (!out_path || !path || size <= 0) return -1;

    char temp[512];
    temp[0] = '\0';

    if (path[0] != '/') {
#ifndef __ETEROS_HOST_TEST__
        if (base_dir) {
            strlcpy(temp, base_dir, sizeof(temp));
        } else {
            task_t* current = task_get_current();
            if (current) strlcpy(temp, current->cwd, sizeof(temp));
            else strlcpy(temp, "/", sizeof(temp));
        }
#else
        if (base_dir) {
            strlcpy(temp, base_dir, sizeof(temp));
        } else {
            strlcpy(temp, "/", sizeof(temp));
        }
#endif
        if (temp[strlen(temp) - 1] != '/') strlcat(temp, "/", sizeof(temp));
        strlcat(temp, path, sizeof(temp));
    } else {
        strlcpy(temp, path, sizeof(temp));
    }

    char* segments[64];
    int count = 0;

    char* p = temp;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        char* start = p;
        while (*p && *p != '/') p++;

        if (*p) {
            *p = '\0';
            p++;
        }

        if (strcmp(start, ".") == 0) {
            continue;
        } else if (strcmp(start, "..") == 0) {
            if (count > 0) count--;
        } else {
            if (count < 64) {
                segments[count++] = start;
            } else {
                return -1; // Too many segments
            }
        }
    }

    out_path[0] = '\0';
    if (count == 0) {
        strlcpy(out_path, "/", size);
        return 0;
    }

    for (int i = 0; i < count; i++) {
        strlcat(out_path, "/", size);
        strlcat(out_path, segments[i], size);
    }

    return 0;
}

fs_node_t *vfs_lookup_ext(fs_node_t *root, const char *path, int follow_symlink);

fs_node_t *vfs_lookup(fs_node_t *root, const char *path) {
    return vfs_lookup_ext(root, path, 1);
}

fs_node_t *vfs_lookup_ext(fs_node_t *root, const char *path, int follow_symlink) {
    if (!root || !path) return 0;

    /* Hack for /dev/tty */
    if (strcmp(path, "/dev/tty") == 0) {
        return tty_create_node();
    }

    /* Handle root path specially */
    if (path[0] == '/' && path[1] == '\0') {
        fs_node_t* clone = (fs_node_t*)kmalloc(sizeof(fs_node_t));
        if (clone) {
            memcpy(clone, root, sizeof(fs_node_t));
            clone->ref_count = 1;
            clone->lock = 0; /* New handle starts unlocked */
        }
        return clone;
    }

    /* Skip leading slash */
    if (path[0] == '/') path++;
    if (!path[0]) {
        /* Duplicate of handle root path case but for safety */
        fs_node_t* clone = (fs_node_t*)kmalloc(sizeof(fs_node_t));
        if (clone) {
            memcpy(clone, root, sizeof(fs_node_t));
            clone->ref_count = 1;
            clone->lock = 0; /* New handle starts unlocked */
        }
        return clone;
    }

    char segment[128];
    const char *p = path;
    fs_node_t *current = NULL;
    fs_node_t *next = 0;

    /* Start from root */
    /* We need a clone of root to start with if we want to treat 'current' uniformly as freeable */
    /* But standard traversal usually doesn't free the starting point. */
    /* Let's iterate. */

    fs_node_t *start_node = root;

    while (*p) {
        int i = 0;
        while (*p && *p != '/' && i < 127) {
            segment[i++] = *p++;
        }
        segment[i] = 0;

        /* SECURITY FIX: Check for path segment truncation */
        if (i == 127 && *p && *p != '/') {
            if (current) kfree(current);
            return 0;
        }

        if (*p == '/') p++;

        if (i == 0) continue;

        if (current == NULL) {
            /* First step from root */
            next = finddir_fs(start_node, segment);
        } else {
            next = finddir_fs(current, segment);
            /* Free previous step */
            kfree(current);
        }

        if (!next) return 0;
        current = next;

        /* Handle Symlinks */
        int symlink_count = 0;
        while ((current->flags & 0x7) == FS_SYMLINK) {
            // Do not follow final component if follow_symlink is false and it is the end of the path
            if (!follow_symlink && !*p) {
                break;
            }

            if (symlink_count++ > 40) {
                kfree(current);
                return 0; // Too many levels of symbolic links
            }

            char target[256];
            ssize_t read_bytes = 0;
            if (current->read) {
                read_bytes = current->read(current, 0, sizeof(target) - 1, (uint8_t*)target);
            }

            if (read_bytes <= 0) {
                kfree(current);
                return 0;
            }
            target[read_bytes] = '\0';

            if (target[0] == '/') {
                fs_node_t* target_node = vfs_lookup(root, target);
                kfree(current);
                current = target_node;
            } else {
                // Simplified relative lookup
                fs_node_t* target_node = vfs_lookup(root, target);
                kfree(current);
                current = target_node;
            }
            if (!current) return 0;
        }

        if ((current->flags & 0x7) != FS_DIRECTORY && *p) {
            kfree(current);
            return 0;
        }
    }

    return current;
}
