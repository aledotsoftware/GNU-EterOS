#include <fs/ramfs.h>
#include <string.h>
#include <mm.h>
#include <lock.h>
#include <klog.h>

/* ========================================================================= */
/* Estructuras Internas                                                      */
/* ========================================================================= */

typedef struct ramfs_entry {
    char name[128];
    void *data;
    size_t size;
    uint32_t flags;
    struct ramfs_entry *next;
    struct ramfs_entry *children; /* Solo para directorios */
    spinlock_t lock;
} ramfs_entry_t;

typedef struct ramfs_handle {
    ramfs_entry_t *entry;
    struct dirent dirent_buf;
} ramfs_handle_t;

/* ========================================================================= */
/* Forward Declarations                                                      */
/* ========================================================================= */

static uint32_t ramfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static uint32_t ramfs_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static void ramfs_open(fs_node_t *node);
static void ramfs_close(fs_node_t *node);
static struct dirent *ramfs_readdir(fs_node_t *node, uint32_t index);
static fs_node_t *ramfs_finddir(fs_node_t *node, char *name);
static int ramfs_create(fs_node_t *parent, char *name, uint16_t permission);
static int ramfs_mkdir(fs_node_t *parent, char *name, uint16_t permission);
static int ramfs_unlink(fs_node_t *parent, char *name);
static void ramfs_destroy_node(fs_node_t *node);

/* ========================================================================= */
/* Implementación                                                            */
/* ========================================================================= */

/* Crea un fs_node_t que apunta a una entrada existente */
static fs_node_t *ramfs_create_node_wrapper(ramfs_entry_t *entry) {
    if (!entry) return NULL;

    fs_node_t *node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!node) return NULL;
    memset(node, 0, sizeof(fs_node_t));

    strlcpy(node->name, entry->name, 128);
    node->flags = entry->flags;
    node->length = entry->size; // Snapshot of size
    node->inode = 0; // No inode implementation yet
    node->ref_count = 1;

    node->read = ramfs_read;
    node->write = ramfs_write;
    node->open = ramfs_open;
    node->close = ramfs_close;
    node->readdir = ramfs_readdir;
    node->finddir = ramfs_finddir;
    node->create = ramfs_create;
    node->mkdir = ramfs_mkdir;
    node->unlink = ramfs_unlink;
    node->destroy = ramfs_destroy_node;

    /* Crear handle */
    ramfs_handle_t *handle = (ramfs_handle_t*)kmalloc(sizeof(ramfs_handle_t));
    if (!handle) {
        kfree(node);
        return NULL;
    }
    handle->entry = entry;
    memset(&handle->dirent_buf, 0, sizeof(struct dirent));

    node->private_data = handle;

    return node;
}

static void ramfs_destroy_node(fs_node_t *node) {
    if (node->private_data) {
        kfree(node->private_data);
    }
}

static uint32_t ramfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    ramfs_handle_t *handle = (ramfs_handle_t*)node->private_data;
    ramfs_entry_t *entry = handle->entry;

    spin_lock(&entry->lock);

    if (offset >= entry->size) {
        spin_unlock(&entry->lock);
        return 0;
    }

    if (offset + size > entry->size) {
        size = entry->size - offset;
    }

    memcpy(buffer, (uint8_t*)entry->data + offset, size);

    spin_unlock(&entry->lock);
    return size;
}

static uint32_t ramfs_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    ramfs_handle_t *handle = (ramfs_handle_t*)node->private_data;
    ramfs_entry_t *entry = handle->entry;

    spin_lock(&entry->lock);

    size_t new_end = offset + size;
    if (new_end > entry->size) {
        /* Need to grow */
        void *new_data = krealloc(entry->data, new_end);
        if (!new_data) {
            spin_unlock(&entry->lock);
            return 0; /* ENOMEM */
        }
        entry->data = new_data;
        entry->size = new_end;
        node->length = entry->size; /* Update node view */
    }

    memcpy((uint8_t*)entry->data + offset, buffer, size);

    spin_unlock(&entry->lock);
    return size;
}

static void ramfs_open(fs_node_t *node) {
    (void)node;
}

static void ramfs_close(fs_node_t *node) {
    (void)node;
}

static struct dirent *ramfs_readdir(fs_node_t *node, uint32_t index) {
    ramfs_handle_t *handle = (ramfs_handle_t*)node->private_data;
    ramfs_entry_t *entry = handle->entry;

    if ((entry->flags & 0x7) != FS_DIRECTORY) return NULL;

    spin_lock(&entry->lock);

    /* Locate child at index */
    ramfs_entry_t *child = entry->children;
    uint32_t i = 0;
    while (child && i < index) {
        child = child->next;
        i++;
    }

    if (!child) {
        spin_unlock(&entry->lock);
        return NULL;
    }

    strlcpy(handle->dirent_buf.name, child->name, 128);
    handle->dirent_buf.inode = 0;

    spin_unlock(&entry->lock);
    return &handle->dirent_buf;
}

static fs_node_t *ramfs_finddir(fs_node_t *node, char *name) {
    ramfs_handle_t *handle = (ramfs_handle_t*)node->private_data;
    ramfs_entry_t *entry = handle->entry;

    if ((entry->flags & 0x7) != FS_DIRECTORY) return NULL;

    spin_lock(&entry->lock);

    ramfs_entry_t *child = entry->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            spin_unlock(&entry->lock);
            return ramfs_create_node_wrapper(child);
        }
        child = child->next;
    }

    spin_unlock(&entry->lock);
    return NULL;
}

/* Helper to allocate new entry */
static ramfs_entry_t *alloc_entry(char *name, uint32_t flags) {
    ramfs_entry_t *new_entry = (ramfs_entry_t*)kmalloc(sizeof(ramfs_entry_t));
    if (!new_entry) return NULL;
    memset(new_entry, 0, sizeof(ramfs_entry_t));
    strlcpy(new_entry->name, name, 128);
    new_entry->flags = flags;
    return new_entry;
}

static int ramfs_create(fs_node_t *parent, char *name, uint16_t permission) {
    (void)permission;
    ramfs_handle_t *handle = (ramfs_handle_t*)parent->private_data;
    ramfs_entry_t *entry = handle->entry;

    spin_lock(&entry->lock);

    /* Check existence */
    ramfs_entry_t *child = entry->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            spin_unlock(&entry->lock);
            return -1; /* EEXIST */
        }
        child = child->next;
    }

    ramfs_entry_t *new_entry = alloc_entry(name, FS_FILE);
    if (!new_entry) {
        spin_unlock(&entry->lock);
        return -1;
    }

    new_entry->next = entry->children;
    entry->children = new_entry;

    spin_unlock(&entry->lock);
    return 0;
}

static int ramfs_mkdir(fs_node_t *parent, char *name, uint16_t permission) {
    (void)permission;
    ramfs_handle_t *handle = (ramfs_handle_t*)parent->private_data;
    ramfs_entry_t *entry = handle->entry;

    spin_lock(&entry->lock);

    /* Check existence */
    ramfs_entry_t *child = entry->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            spin_unlock(&entry->lock);
            return -1; /* EEXIST */
        }
        child = child->next;
    }

    ramfs_entry_t *new_entry = alloc_entry(name, FS_DIRECTORY);
    if (!new_entry) {
        spin_unlock(&entry->lock);
        return -1;
    }

    new_entry->next = entry->children;
    entry->children = new_entry;

    spin_unlock(&entry->lock);
    return 0;
}

static int ramfs_unlink(fs_node_t *parent, char *name) {
    ramfs_handle_t *handle = (ramfs_handle_t*)parent->private_data;
    ramfs_entry_t *entry = handle->entry;

    spin_lock(&entry->lock);

    ramfs_entry_t *curr = entry->children;
    ramfs_entry_t *prev = NULL;

    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            /* Found */
            if ((curr->flags & 0x7) == FS_DIRECTORY) {
                /* Check empty */
                if (curr->children != NULL) {
                     spin_unlock(&entry->lock);
                     return -1; /* ENOTEMPTY */
                }
            }

            if (prev) {
                prev->next = curr->next;
            } else {
                entry->children = curr->next;
            }

            /* Free data */
            if (curr->data) kfree(curr->data);
            kfree(curr);

            spin_unlock(&entry->lock);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    spin_unlock(&entry->lock);
    return -1; /* ENOENT */
}

/* ========================================================================= */
/* Initialization                                                            */
/* ========================================================================= */

fs_node_t *ramfs_init(void) {
    ramfs_entry_t *root_entry = alloc_entry("ramfs_root", FS_DIRECTORY);
    if (!root_entry) return NULL;

    return ramfs_create_node_wrapper(root_entry);
}
