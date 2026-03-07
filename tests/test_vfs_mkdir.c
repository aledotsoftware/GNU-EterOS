#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

/* Mock types */
#include "../include/types.h"
#include "../include/fs/vfs.h"
#include "../include/mm.h"

/* Global state for assertions */
static char last_mkdir_name[128];
static uint16_t last_mkdir_permission;
static int mock_mkdir_called;
static int mock_mkdir_return_val;

/* Mock Memory Management */
void* kmalloc(size_t size) {
    return calloc(1, size);
}

void kfree(void* ptr) {
    free(ptr);
}

/* Include string implementation (provides eteros_strlen, etc) */
#include "../kernel/string.c"

/* Mock TTY */
fs_node_t* tty_create_node(void) {
    return NULL;
}

int mock_mkdir(struct fs_node *parent, char *name, uint16_t permission) {
    (void)parent;
    mock_mkdir_called = 1;
    eteros_strlcpy(last_mkdir_name, name, sizeof(last_mkdir_name));
    last_mkdir_permission = permission;
    return mock_mkdir_return_val;
}

/* Mock Finddir Logic for lookup */
fs_node_t* mock_finddir(fs_node_t* node, char* name) {
    (void)node;

    if (eteros_strcmp(name, "existing_dir") == 0 || eteros_strcmp(name, "long_parent") == 0) {
        fs_node_t* dir = (fs_node_t*)kmalloc(sizeof(fs_node_t));
        eteros_strlcpy(dir->name, name, sizeof(dir->name));
        dir->flags = FS_DIRECTORY;
        dir->finddir = mock_finddir;
        dir->mkdir = mock_mkdir;
        dir->ref_count = 1;
        return dir;
    }

    if (eteros_strcmp(name, "existing_file") == 0) {
        fs_node_t* file = (fs_node_t*)kmalloc(sizeof(fs_node_t));
        eteros_strlcpy(file->name, name, sizeof(file->name));
        file->flags = FS_FILE;
        file->ref_count = 1;
        return file;
    }

    return NULL;
}

/* Include source under test */
#include "../kernel/fs/vfs.c"

void reset_mocks() {
    mock_mkdir_called = 0;
    mock_mkdir_return_val = 0;
    eteros_memset(last_mkdir_name, 0, sizeof(last_mkdir_name));
    last_mkdir_permission = 0;
}

void test_vfs_mkdir() {
    printf("Running test_vfs_mkdir...\n");

    // Setup Root
    fs_node_t* root = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    eteros_strlcpy(root->name, "root", sizeof(root->name));
    root->flags = FS_DIRECTORY;
    root->finddir = mock_finddir;
    root->mkdir = mock_mkdir;
    root->ref_count = 1;
    fs_root = root;

    int ret;

    /* Test 1: NULL path */
    reset_mocks();
    ret = vfs_mkdir(NULL, 0x777);
    assert(ret == -1);
    printf("Test 1 (NULL path) passed.\n");

    /* Test 2: No slash in path */
    reset_mocks();
    ret = vfs_mkdir("name", 0x777);
    assert(ret == -1);
    printf("Test 2 (No slash) passed.\n");

    /* Test 3: Path too long (last_slash >= 127) */
    reset_mocks();
    char long_path[200];
    eteros_memset(long_path, 'A', 127);
    long_path[0] = '/';
    long_path[127] = '/';
    long_path[128] = 'B';
    long_path[129] = '\0';
    ret = vfs_mkdir(long_path, 0x777);
    assert(ret == -1);
    printf("Test 3 (Path too long) passed.\n");

    /* Test 4: Parent directory lookup fails */
    reset_mocks();
    ret = vfs_mkdir("/nonexistent_dir/new_dir", 0x777);
    assert(ret == -1);
    assert(mock_mkdir_called == 0);
    printf("Test 4 (Parent directory not found) passed.\n");

    /* Test 5: Successful creation in root (/dir1) */
    reset_mocks();
    mock_mkdir_return_val = 0;
    ret = vfs_mkdir("/new_root_dir", 0x755);
    assert(ret == 0);
    assert(mock_mkdir_called == 1);
    assert(eteros_strcmp(last_mkdir_name, "new_root_dir") == 0);
    assert(last_mkdir_permission == 0x755);
    printf("Test 5 (Successful creation in root) passed.\n");

    /* Test 6: Successful creation in a subdirectory (/existing_dir/new_dir) */
    reset_mocks();
    mock_mkdir_return_val = 0;
    ret = vfs_mkdir("/existing_dir/new_dir", 0x700);
    assert(ret == 0);
    assert(mock_mkdir_called == 1);
    assert(eteros_strcmp(last_mkdir_name, "new_dir") == 0);
    assert(last_mkdir_permission == 0x700);
    printf("Test 6 (Successful creation in subdirectory) passed.\n");

    /* Test 7: Parent is a file, not a directory */
    reset_mocks();
    ret = vfs_mkdir("/existing_file/new_dir", 0x777);
    assert(ret == -1);
    assert(mock_mkdir_called == 0);
    printf("Test 7 (Parent is a file) passed.\n");

    /* Test 8: mkdir_fs fails (returns error code) */
    reset_mocks();
    mock_mkdir_return_val = -2;
    ret = vfs_mkdir("/existing_dir/failed_dir", 0x777);
    assert(ret == -2);
    assert(mock_mkdir_called == 1);
    printf("Test 8 (mkdir_fs returns failure) passed.\n");

    kfree(root);
    printf("All vfs_mkdir tests passed successfully!\n");
}

int main() {
    test_vfs_mkdir();
    return 0;
}
