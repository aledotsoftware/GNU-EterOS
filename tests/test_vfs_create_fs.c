#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Mock types */
#include "../include/types.h"
#include "../include/fs/vfs.h"

#undef assert
#define assert(x) if (!(x)) { printf("Assertion failed: %s at line %d\n", #x, __LINE__); exit(1); }

/* Global state for assertions */
static char last_create_name[128];
static uint16_t last_create_permission;
static int mock_create_called;
static int mock_create_return_val;

/* Mock Memory Management */
void* kmalloc(size_t size) {
    return calloc(1, size);
}

void kfree(void* ptr) {
    free(ptr);
}

/* Include string implementation */
#include "../kernel/string.c"

/* Mock TTY */
fs_node_t* tty_create_node(void) {
    return NULL;
}

void serial_write_string(const char *str) {
    (void)str;
}

fs_node_t *fs_root = NULL;

int mock_create(struct fs_node *parent, char *name, uint16_t permission) {
    (void)parent;
    mock_create_called = 1;
    eteros_strlcpy(last_create_name, name, sizeof(last_create_name));
    last_create_permission = permission;
    return mock_create_return_val;
}

/* Include source under test */
#include "../kernel/fs/vfs.c"

void reset_mocks() {
    mock_create_called = 0;
    mock_create_return_val = 0;
    eteros_memset(last_create_name, 0, sizeof(last_create_name));
    last_create_permission = 0;
}

void test_create_fs() {
    printf("Running test_create_fs...\n");

    fs_node_t parent;
    eteros_memset(&parent, 0, sizeof(parent));
    int ret;

    /* Test 1: Parent is a directory with a create function */
    reset_mocks();
    parent.flags = FS_DIRECTORY;
    parent.create = mock_create;
    mock_create_return_val = 0;

    ret = create_fs(&parent, "test_file", 0644);

    assert(ret == 0);
    assert(mock_create_called == 1);
    assert(eteros_strcmp(last_create_name, "test_file") == 0);
    assert(last_create_permission == 0644);
    printf("Test 1 (Valid parent dir with create function) passed.\n");

    /* Test 2: Parent is a directory but create function is NULL */
    reset_mocks();
    parent.flags = FS_DIRECTORY;
    parent.create = NULL;

    ret = create_fs(&parent, "test_file", 0644);

    assert(ret == -1);
    assert(mock_create_called == 0);
    printf("Test 2 (Directory with NULL create function) passed.\n");

    /* Test 3: Parent is NOT a directory (e.g. FS_FILE) */
    reset_mocks();
    parent.flags = FS_FILE;
    parent.create = mock_create;

    ret = create_fs(&parent, "test_file", 0644);

    assert(ret == -1);
    assert(mock_create_called == 0);
    printf("Test 3 (Parent is a file, not a directory) passed.\n");

    /* Test 4: mock_create returns an error code */
    reset_mocks();
    parent.flags = FS_DIRECTORY;
    parent.create = mock_create;
    mock_create_return_val = -2;

    ret = create_fs(&parent, "test_file", 0644);

    assert(ret == -2);
    assert(mock_create_called == 1);
    printf("Test 4 (Create function returns error) passed.\n");

    printf("All test_create_fs tests passed successfully!\n");
}

int main() {
    test_create_fs();
    return 0;
}
