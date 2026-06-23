#define __ETEROS_HOST_TEST__ 1
#define ETEROS_MM_H
#define FS_VFS_H
#define ETEROS_LOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "../include/types.h"

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x03
#define FS_BLOCKDEVICE 0x04
#define FS_PIPE        0x05
#define FS_SYMLINK     0x06
#define FS_MOUNTPOINT  0x08

typedef int spinlock_t;
void spin_lock(spinlock_t *lock) { (void)lock; }
void spin_unlock(spinlock_t *lock) { (void)lock; }

struct fs_node;
struct dirent {
    char name[128];
    uint32_t inode;
};

typedef ssize_t (*read_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef uint32_t (*write_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef void (*open_type_t)(struct fs_node*);
typedef void (*close_type_t)(struct fs_node*);
typedef int (*readdir_type_t)(struct fs_node*, uint32_t, struct dirent*);
typedef struct fs_node * (*finddir_type_t)(struct fs_node*, char *name);
typedef int (*ioctl_type_t)(struct fs_node*, int, void*);
typedef int (*create_type_t)(struct fs_node*, char*, uint16_t);
typedef int (*mkdir_type_t)(struct fs_node*, char*, uint16_t);
typedef int (*unlink_type_t)(struct fs_node*, char*);
typedef int (*link_type_t)(struct fs_node*, struct fs_node*, const char*);

typedef struct fs_node {
    char name[128];
    uint32_t mask;
    uint32_t uid;
    uint32_t gid;
    uint32_t flags;
    uint32_t inode;
    uint32_t length;
    uint32_t impl;
    time_t   atime;
    time_t   mtime;
    time_t   ctime;
    read_type_t read;
    write_type_t write;
    open_type_t open;
    close_type_t close;
    ioctl_type_t ioctl;
    readdir_type_t readdir;
    finddir_type_t finddir;
    create_type_t create;
    mkdir_type_t mkdir;
    unlink_type_t unlink;
    link_type_t link;
    struct fs_node *ptr;
    uint32_t ref_count;
    spinlock_t lock;
} fs_node_t;

/* Externs from vfs.c */
fs_node_t *fs_root = NULL;
extern fs_node_t *vfs_lookup(fs_node_t *root, const char *path);
void serial_write_string(const char* s) {}

/* Global Tracking */
int kmalloc_count = 0;
int kfree_count = 0;

void* kmalloc(size_t size) {
    kmalloc_count++;
    void* ptr = malloc(size);
    return ptr;
}

void kfree(void* ptr) {
    if (!ptr) return;
    kfree_count++;
    free(ptr);
}

/* Helper for finding root */
fs_node_t* finddir_fs(fs_node_t* node, char* name);

fs_node_t* tty_create_node(void) { return NULL; }

/* Include Code Under Test */
#include "../kernel/fs/vfs.c"

/* Mock FS Implementation */
fs_node_t* mock_finddir(fs_node_t* node, char* name) {
    (void)node;
    fs_node_t* new_node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!new_node) return NULL;
    memset(new_node, 0, sizeof(fs_node_t));
    strncpy(new_node->name, name, 127);
    new_node->flags = FS_DIRECTORY;
    new_node->finddir = mock_finddir;
    return new_node;
}

int main() {
    printf("Starting VFS Leak Test...\n");

    /* Setup Root */
    fs_root = (fs_node_t*)malloc(sizeof(fs_node_t));
    memset(fs_root, 0, sizeof(fs_node_t));
    fs_root->flags = FS_DIRECTORY;
    fs_root->finddir = mock_finddir;
    strlcpy(fs_root->name, "root", sizeof(fs_root->name));

    /* Test 1: Lookup single level */
    printf("\nTest 1: Lookup '/a'...\n");
    kmalloc_count = 0;
    kfree_count = 0;
    fs_node_t* node = vfs_lookup(fs_root, "/a");

    if (node) {
        printf("Result: Found node '%s'\n", node->name);
        printf("Stats: Alloc=%d, Free=%d\n", kmalloc_count, kfree_count);
        if (kmalloc_count != 3 || kfree_count != 2) {
            printf("FAIL: Expected 3 allocs, 2 frees.\n");
            return 1;
        }
        kfree(node);
    } else {
        printf("FAIL: Node not found.\n");
        return 1;
    }

    /* Test 2: Lookup multi level */
    printf("\nTest 2: Lookup '/a/b/c'...\n");
    kmalloc_count = 0;
    kfree_count = 0;

    node = vfs_lookup(fs_root, "/a/b/c");

    if (node) {
        printf("Result: Found node '%s'\n", node->name);
        printf("Stats: Alloc=%d, Free=%d\n", kmalloc_count, kfree_count);

        if (kmalloc_count != 5) {
             printf("FAIL: Expected 5 allocs. Got %d.\n", kmalloc_count);
             return 1;
        }
        if (kfree_count != 4) {
             printf("FAIL: Expected 4 frees. Got %d.\n", kfree_count);
             return 1;
        }

        kfree(node);
    } else {
        printf("FAIL: Node not found.\n");
        return 1;
    }

    /* Test 3: Root lookup */
    printf("\nTest 3: Lookup '/'...\n");
    kmalloc_count = 0;
    kfree_count = 0;
    node = vfs_lookup(fs_root, "/");
    if (node) {
         printf("Stats: Alloc=%d, Free=%d\n", kmalloc_count, kfree_count);
         if (kmalloc_count != 1 || kfree_count != 0) {
             printf("FAIL: Expected 1 alloc, 0 free.\n");
             return 1;
         }
         kfree(node);
    } else {
         printf("FAIL: Root not found.\n");
         return 1;
    }

    printf("\nPASS: All VFS tests passed.\n");
    free(fs_root);
    return 0;
}

