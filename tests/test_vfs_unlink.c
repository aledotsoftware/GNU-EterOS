#include <errno.h>
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

static char last_unlink_name[128];
static int mock_unlink_called;
static int mock_unlink_return_val;

void* kmalloc(size_t size) { return calloc(1, size); }
void kfree(void* ptr) { free(ptr); }

#include "../kernel/string.c"
fs_node_t* tty_create_node(void) { return NULL; }
void serial_write_string(const char *str) { (void)str; }
fs_node_t *fs_root = NULL;

int mock_unlink(struct fs_node *parent, char *name) {
    (void)parent;
    mock_unlink_called = 1;
    eteros_strlcpy(last_unlink_name, name, sizeof(last_unlink_name));
    return mock_unlink_return_val;
}

#include "../kernel/fs/vfs.c"

void test_unlink_fs() {
    printf("Running test_unlink_fs...\n");

    fs_node_t parent;
    eteros_memset(&parent, 0, sizeof(parent));
    int ret;

    /* Test 1: Parent is a directory with unlink */
    mock_unlink_called = 0;
    parent.flags = FS_DIRECTORY;
    parent.unlink = mock_unlink;
    mock_unlink_return_val = 0;
    ret = unlink_fs(&parent, "test_file");
    assert(ret == 0);
    assert(mock_unlink_called == 1);
    assert(eteros_strcmp(last_unlink_name, "test_file") == 0);

    /* Test 2: Parent is a file */
    mock_unlink_called = 0;
    parent.flags = FS_FILE;
    ret = unlink_fs(&parent, "test_file");
    assert(ret == -ENOTDIR);

    /* Test 3: Unlink returns error */
    mock_unlink_called = 0;
    parent.flags = FS_DIRECTORY;
    parent.unlink = mock_unlink;
    mock_unlink_return_val = -ENOENT;
    ret = unlink_fs(&parent, "test_file");
    assert(ret == -ENOENT);

    printf("All test_unlink_fs tests passed successfully!\n");
}

int main() {
    test_unlink_fs();
    return 0;
}
