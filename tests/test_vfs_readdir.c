#define __ETEROS_HOST_TEST__ 1
#define ETEROS_MM_H
#define FS_VFS_H
#define ETEROS_LOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include "../include/types.h"

/* Instead of disabling string.h, include the kernel's implementation so
   functions like eteros_memset are available. */
#include "../kernel/string.c"

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

fs_node_t* tty_create_node(void) { return NULL; }

fs_node_t *fs_root = NULL;
fs_node_t *vfs_lookup(fs_node_t *root, const char *path);

/* Include Code Under Test */
#include "../kernel/fs/vfs.c"

int readdir_called = 0;

int mock_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) {
    (void)node;
    readdir_called++;

    if (index == 0) {
        eteros_strncpy(entry->name, ".", 127);
        entry->inode = 100;
        return 0; // Success
    } else if (index == 1) {
        eteros_strncpy(entry->name, "file.txt", 127);
        entry->inode = 101;
        return 0; // Success
    }

    return -1; // No more entries
}

void test_readdir_fs_success() {
    printf("Running test_readdir_fs_success...\n");

    fs_node_t *node = (fs_node_t *)malloc(sizeof(fs_node_t));
    eteros_memset(node, 0, sizeof(fs_node_t));
    node->flags = FS_DIRECTORY;
    node->readdir = mock_readdir;

    struct dirent entry;
    readdir_called = 0;

    int result = readdir_fs(node, 0, &entry);

    if (result != 0) {
        printf("FAIL: readdir_fs failed for valid index 0, expected 0, got %d\n", result);
        exit(1);
    }

    if (readdir_called != 1) {
        printf("FAIL: mock_readdir was not called exactly once, called %d times\n", readdir_called);
        exit(1);
    }

    if (eteros_strcmp(entry.name, ".") != 0 || entry.inode != 100) {
        printf("FAIL: entry data incorrect, got name='%s', inode=%d\n", entry.name, entry.inode);
        exit(1);
    }

    result = readdir_fs(node, 2, &entry);

    if (result != -1) {
        printf("FAIL: readdir_fs failed for invalid index 2, expected -1, got %d\n", result);
        exit(1);
    }

    printf("PASS: test_readdir_fs_success\n");
    free(node);
}

void test_readdir_fs_not_directory() {
    printf("Running test_readdir_fs_not_directory...\n");

    fs_node_t *node = (fs_node_t *)malloc(sizeof(fs_node_t));
    eteros_memset(node, 0, sizeof(fs_node_t));
    node->flags = FS_FILE; // Not a directory
    node->readdir = mock_readdir;

    struct dirent entry;
    readdir_called = 0;

    int result = readdir_fs(node, 0, &entry);

    if (result != -1) {
        printf("FAIL: readdir_fs should return -1 for non-directory nodes, got %d\n", result);
        exit(1);
    }

    if (readdir_called != 0) {
        printf("FAIL: mock_readdir should not be called for non-directory nodes\n");
        exit(1);
    }

    printf("PASS: test_readdir_fs_not_directory\n");
    free(node);
}

void test_readdir_fs_null_pointer() {
    printf("Running test_readdir_fs_null_pointer...\n");

    fs_node_t *node = (fs_node_t *)malloc(sizeof(fs_node_t));
    eteros_memset(node, 0, sizeof(fs_node_t));
    node->flags = FS_DIRECTORY;
    node->readdir = NULL; // NULL function pointer

    struct dirent entry;

    int result = readdir_fs(node, 0, &entry);

    if (result != -1) {
        printf("FAIL: readdir_fs should return -1 for nodes with NULL readdir pointer, got %d\n", result);
        exit(1);
    }

    printf("PASS: test_readdir_fs_null_pointer\n");
    free(node);
}

int main() {
    printf("Starting VFS readdir_fs Tests...\n");

    test_readdir_fs_success();
    test_readdir_fs_not_directory();
    test_readdir_fs_null_pointer();

    printf("\nAll VFS readdir_fs tests passed successfully!\n");
    return 0;
}
