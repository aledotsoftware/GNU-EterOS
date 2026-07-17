#define __ETEROS_HOST_TEST__ 1
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../include/types.h"
#include "../include/fs/vfs.h"

int kmalloc_count = 0;
int kfree_count = 0;

void* kmalloc(size_t size) {
    kmalloc_count++;
    return malloc(size);
}
void kfree(void* ptr) {
    if (ptr) kfree_count++;
    free(ptr);
}
void serial_write_string(const char* s) { (void)s; }
fs_node_t* tty_create_node(void) { return NULL; }

// Global fs_root
fs_node_t *fs_root = NULL;

#include "../kernel/fs/vfs.c"

int unlink_called = 0;
int rename_called = 0;

int mock_unlink(struct fs_node *parent, char *name) {
    unlink_called++;
    return 0;
}

int mock_rename(struct fs_node *old_parent, char *old_name, struct fs_node *new_parent, char *new_name) {
    rename_called++;
    return 0;
}

void test_vfs_unlink() {
    printf("Running test_vfs_unlink...\n");

    fs_node_t *node = (fs_node_t *)malloc(sizeof(fs_node_t));
    memset(node, 0, sizeof(fs_node_t));

    // Test 1: Successful unlink
    node->flags = FS_DIRECTORY;
    node->unlink = mock_unlink;
    unlink_called = 0;
    int result = unlink_fs(node, "file.txt");
    if (result != 0 || unlink_called != 1) {
        printf("FAIL: unlink_fs failed for valid directory, got %d\n", result);
        exit(1);
    }

    // Test 2: Unlink on non-directory should return -ENOTDIR, but EPERM is actually better? Wait... let's see.
    // Actually the instruction says: "In EterOS VFS, operations like unlink and rename must fallback to returning -EPERM when not implemented by the underlying filesystem."

    node->flags = FS_DIRECTORY;
    node->unlink = NULL;
    result = unlink_fs(node, "file.txt");
    if (result != -EPERM) {
        printf("FAIL: unlink_fs should return -EPERM when not implemented by underlying filesystem, got %d\n", result);
        exit(1);
    }

    printf("PASS: test_vfs_unlink\n");
    free(node);
}

void test_vfs_rename() {
    printf("Running test_vfs_rename...\n");

    fs_node_t *old_node = (fs_node_t *)malloc(sizeof(fs_node_t));
    memset(old_node, 0, sizeof(fs_node_t));

    fs_node_t *new_node = (fs_node_t *)malloc(sizeof(fs_node_t));
    memset(new_node, 0, sizeof(fs_node_t));

    // Test 1: Successful rename
    old_node->flags = FS_DIRECTORY;
    new_node->flags = FS_DIRECTORY;
    old_node->rename = mock_rename;
    rename_called = 0;
    int result = rename_fs(old_node, "old.txt", new_node, "new.txt");
    if (result != 0 || rename_called != 1) {
        printf("FAIL: rename_fs failed for valid directories, got %d\n", result);
        exit(1);
    }

    // Test 2: Rename not implemented should return -EPERM
    old_node->rename = NULL;
    result = rename_fs(old_node, "old.txt", new_node, "new.txt");
    if (result != -EPERM) {
        printf("FAIL: rename_fs should return -EPERM when not implemented by underlying filesystem, got %d\n", result);
        exit(1);
    }

    printf("PASS: test_vfs_rename\n");
    free(old_node);
    free(new_node);
}

int main() {
    test_vfs_unlink();
    test_vfs_rename();
    return 0;
}
