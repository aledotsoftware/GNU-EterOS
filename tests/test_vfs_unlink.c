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
    struct fs_node *ptr;
    uint32_t ref_count;
    spinlock_t lock;
} fs_node_t;

void* kmalloc(size_t size) {
    return malloc(size);
}

void kfree(void* ptr) {
    if (!ptr) return;
    free(ptr);
}

/* Externs from vfs.c */
extern fs_node_t *fs_root;
fs_node_t *vfs_lookup(fs_node_t *root, const char *path);

fs_node_t* tty_create_node(void) { return NULL; }

/* Include Code Under Test */
#include "../kernel/fs/vfs.c"

int mock_unlink_success(fs_node_t *node, char *name) {
    return 0; // Success
}

int mock_unlink_failure(fs_node_t *node, char *name) {
    return -2; // Some error code
}

int main() {
    printf("Starting VFS Unlink Test...\n");

    /* Setup Parent Node */
    fs_node_t* parent = (fs_node_t*)malloc(sizeof(fs_node_t));

    // Test 1: Parent is not a directory
    memset(parent, 0, sizeof(fs_node_t));
    parent->flags = FS_FILE;
    parent->unlink = mock_unlink_success;

    int result = unlink_fs(parent, "test.txt");
    assert(result == -1);
    printf("Test 1 Passed: Parent is not a directory\n");

    // Test 2: Parent is a directory, but unlink is null
    memset(parent, 0, sizeof(fs_node_t));
    parent->flags = FS_DIRECTORY;
    parent->unlink = NULL;

    result = unlink_fs(parent, "test.txt");
    assert(result == -1);
    printf("Test 2 Passed: Parent unlink function is NULL\n");

    // Test 3: Parent is a directory, unlink is set and returns success
    memset(parent, 0, sizeof(fs_node_t));
    parent->flags = FS_DIRECTORY;
    parent->unlink = mock_unlink_success;

    result = unlink_fs(parent, "test.txt");
    assert(result == 0);
    printf("Test 3 Passed: Parent unlink function returns success\n");

    // Test 4: Parent is a directory, unlink is set and returns failure
    memset(parent, 0, sizeof(fs_node_t));
    parent->flags = FS_DIRECTORY;
    parent->unlink = mock_unlink_failure;

    result = unlink_fs(parent, "test.txt");
    assert(result == -2);
    printf("Test 4 Passed: Parent unlink function returns failure\n");

    printf("\nPASS: All VFS Unlink tests passed.\n");
    free(parent);
    return 0;
}
