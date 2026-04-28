#include <fs/shmfs.h>
#include <fs/vfs.h>
#include <mm.h>
#include <pmm.h>
#include <vmm.h>
#include <string.h>
#include <lock.h>

static fs_node_t* shmfs_root = NULL;
static shm_object_t* shm_objects = NULL;
static spinlock_t shm_lock = 0;

/* Find an existing shm object by name */
static shm_object_t* shm_find_object(const char* name) {
    shm_object_t* curr = shm_objects;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

/* Create a new shm object */
static shm_object_t* shm_create_object(const char* name) {
    shm_object_t* obj = (shm_object_t*)kmalloc(sizeof(shm_object_t));
    if (!obj) return NULL;
    
    memset(obj, 0, sizeof(shm_object_t));
    strlcpy(obj->name, name, sizeof(obj->name));
    obj->ref_count = 1;
    obj->lock = 0;
    
    /* If name is empty, it's an anonymous memfd object, don't link it to the global list */
    if (name && name[0] != '\0') {
        obj->next = shm_objects;
        shm_objects = obj;
    } else {
        obj->next = NULL;
    }
    return obj;
}

/* Free a shm object and its pages */
static void shm_free_object(shm_object_t* obj) {
    if (!obj) return;
    
    /* Remove from list */
    if (shm_objects == obj) {
        shm_objects = obj->next;
    } else {
        shm_object_t* curr = shm_objects;
        while (curr && curr->next != obj) {
            curr = curr->next;
        }
        if (curr) curr->next = obj->next;
    }
    
    /* Free pages */
    if (obj->pages) {
        for (uint32_t i = 0; i < obj->page_count; i++) {
            if (obj->pages[i]) {
                pmm_free_page((void*)obj->pages[i]);
            }
        }
        kfree(obj->pages);
    }
    
    kfree(obj);
}

static ssize_t shmfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; (void)size; (void)buffer;
    /* SHM nodes are typically manipulated via mmap, not read/write directly, 
       but basic support could be added if needed. */
    return 0;
}

static uint32_t shmfs_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; (void)size; (void)buffer;
    /* Basic write support not typically needed for SHM. Returns 0 to indicate error or no bytes written. */
    return 0;
}

static void shmfs_open(fs_node_t *node) {
    /* If called during open(), increment object ref_count.
       However, finddir already returns a clone, so we typically handle lifecycle there.
       For SHM, we keep it simple. */
    (void)node;
}

static void shmfs_close(fs_node_t *node) {
    /* Decrement ref count when an open fd is closed */
    if (!node) return;
    shm_object_t* obj = (shm_object_t*)(uintptr_t)node->impl;
    if (!obj) return;
    
    spin_lock(&shm_lock);
    spin_lock(&obj->lock);
    
    obj->ref_count--;
    int should_free = 0;

    /* If it's an anonymous object (memfd), free it when ref count reaches 0 */
    if (obj->ref_count == 0 && obj->name[0] == '\0') {
        should_free = 1;
    }
    
    spin_unlock(&obj->lock);

    if (should_free) {
        shm_free_object(obj);
    }
    spin_unlock(&shm_lock);
}

int shmfs_truncate(fs_node_t *node, uint32_t length);

fs_node_t* shmfs_create_memfd(const char* name) {
    shm_object_t* obj = shm_create_object(""); /* Empty name = anonymous */
    if (!obj) return NULL;

    /* We optionally copy the name just for debugging/display, but keep it out of the global list.
       Actually, `memfd_create` names are just for debugging in `/proc/self/maps`.
       For our implementation, empty name means anonymous. We can store the debug name in node->name. */

    fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fnode) {
        shm_free_object(obj);
        return NULL;
    }
    memset(fnode, 0, sizeof(fs_node_t));

    strlcpy(fnode->name, name ? name : "memfd", sizeof(fnode->name));
    fnode->flags = FS_FILE;
    fnode->mask = 0666;
    fnode->impl = (uintptr_t)obj;
    fnode->length = obj->size;

    fnode->read = shmfs_read;
    fnode->write = shmfs_write;
    fnode->open = shmfs_open;
    fnode->close = shmfs_close;
    fnode->truncate = shmfs_truncate;

    fnode->ref_count = 1;

    return fnode;
}

int shmfs_truncate(fs_node_t *node, uint32_t length) {
    if (!node) return -1;
    shm_object_t* obj = (shm_object_t*)(uintptr_t)node->impl;
    if (!obj) return -1;

    spin_lock(&obj->lock);
    
    uint32_t new_page_count = PAGE_ALIGN_UP(length) / PAGE_SIZE;
    
    if (new_page_count > obj->page_count) {
        /* Grow */
        uint64_t* new_pages = (uint64_t*)kmalloc(new_page_count * sizeof(uint64_t));
        if (!new_pages) {
            spin_unlock(&obj->lock);
            return -1; /* ENOMEM */
        }
        
        /* Copy old pages */
        if (obj->pages) {
            memcpy(new_pages, obj->pages, obj->page_count * sizeof(uint64_t));
            kfree(obj->pages);
        }
        
        /* Allocate new pages */
        for (uint32_t i = obj->page_count; i < new_page_count; i++) {
            new_pages[i] = (uint64_t)pmm_alloc_page();
            if (new_pages[i]) {
                memset((void*)new_pages[i], 0, PAGE_SIZE);
            } else {
                /* OOM recovery could be added here */
            }
        }
        
        obj->pages = new_pages;
        obj->page_count = new_page_count;
        
    } else if (new_page_count < obj->page_count) {
        /* Shrink */
        for (uint32_t i = new_page_count; i < obj->page_count; i++) {
            if (obj->pages[i]) {
                pmm_free_page((void*)obj->pages[i]);
                obj->pages[i] = 0;
            }
        }
        obj->page_count = new_page_count;
        /* Optionally realloc the array to save space */
    }
    
    obj->size = length;
    node->length = length;
    
    spin_unlock(&obj->lock);
    return 0;
}

static fs_node_t *shmfs_finddir(fs_node_t *node, char *name) {
    (void)node;
    if (!name) return NULL;
    
    spin_lock(&shm_lock);
    shm_object_t* obj = shm_find_object(name);
    spin_unlock(&shm_lock);
    
    if (!obj) return NULL;
    
    fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fnode) return NULL;
    memset(fnode, 0, sizeof(fs_node_t));
    
    strlcpy(fnode->name, name, sizeof(fnode->name));
    fnode->flags = FS_FILE;
    fnode->mask = 0666;
    fnode->impl = (uintptr_t)obj;
    fnode->length = obj->size;
    
    fnode->read = shmfs_read;
    fnode->write = shmfs_write;
    fnode->open = shmfs_open;
    fnode->close = shmfs_close;
    fnode->truncate = shmfs_truncate;
    
    fnode->ref_count = 1;
    
    return fnode;
}

static int shmfs_create(fs_node_t *parent, char *name, uint16_t permission) {
    (void)parent; (void)permission;
    if (!name) return -1;
    
    spin_lock(&shm_lock);
    shm_object_t* obj = shm_find_object(name);
    if (obj) {
        spin_unlock(&shm_lock);
        return 0; /* Already exists */
    }
    
    obj = shm_create_object(name);
    spin_unlock(&shm_lock);
    
    return obj ? 0 : -1;
}

static int shmfs_unlink(fs_node_t *parent, char *name) {
    (void)parent;
    if (!name) return -1;
    
    spin_lock(&shm_lock);
    shm_object_t* obj = shm_find_object(name);
    if (!obj) {
        spin_unlock(&shm_lock);
        return -1;
    }
    
    /* For simplicity, free immediately. A true POSIX shm unlinks the name but 
       delays freeing until ref_count == 0. */
    shm_free_object(obj);
    spin_unlock(&shm_lock);
    
    return 0;
}

static int shmfs_rename(fs_node_t *old_parent, char *old_name, fs_node_t *new_parent, char *new_name) {
    (void)old_parent;
    (void)new_parent;
    if (!old_name || !new_name) return -1;

    spin_lock(&shm_lock);
    shm_object_t* obj = shm_find_object(old_name);
    if (!obj) {
        spin_unlock(&shm_lock);
        return -1;
    }

    if (shm_find_object(new_name)) {
        shm_object_t* existing = shm_find_object(new_name);
        /* Remove the existing one from the list but keep its memory alive until closed */
        if (shm_objects == existing) {
            shm_objects = existing->next;
        } else {
            shm_object_t* curr = shm_objects;
            while (curr && curr->next != existing) curr = curr->next;
            if (curr) curr->next = existing->next;
        }
        /* Ideally we would delay free until ref_count == 0, but shm_free_object works for simple RAM FS */
        if (existing->ref_count <= 0) shm_free_object(existing);
    }

    strlcpy(obj->name, new_name, sizeof(obj->name));
    spin_unlock(&shm_lock);
    return 0;
}

static int shmfs_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) {
    (void)node;
    
    spin_lock(&shm_lock);
    shm_object_t* curr = shm_objects;
    uint32_t i = 0;
    while (curr && i < index) {
        curr = curr->next;
        i++;
    }
    
    if (curr) {
        strlcpy(entry->name, curr->name, sizeof(entry->name));
        entry->inode = i + 1; /* Dummy inode */
        spin_unlock(&shm_lock);
        return 0; /* success */
    }
    
    spin_unlock(&shm_lock);
    return 1; /* EOF */
}

fs_node_t* shmfs_init(void) {
    if (shmfs_root) return shmfs_root;
    
    shmfs_root = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!shmfs_root) return NULL;
    memset(shmfs_root, 0, sizeof(fs_node_t));
    
    strlcpy(shmfs_root->name, "shm", sizeof(shmfs_root->name));
    shmfs_root->flags = FS_DIRECTORY;
    shmfs_root->mask = 0777;
    shmfs_root->ref_count = 1;
    
    shmfs_root->finddir = shmfs_finddir;
    shmfs_root->create = shmfs_create;
    shmfs_root->unlink = shmfs_unlink;
    shmfs_root->readdir = shmfs_readdir;
    shmfs_root->rename = shmfs_rename;
    
    return shmfs_root;
}
