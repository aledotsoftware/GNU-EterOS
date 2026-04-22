#include <stdio.h>
#include <stdlib.h>

void my_assert(int condition) {
    if (!condition) {
        printf("Assertion failed!\n");
        exit(1);
    }
}
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define AT_FDCWD -100
#define FS_SYMLINK 4

// Mocks
typedef struct fs_node {
    int flags;
    ssize_t (*read)(struct fs_node*, uint64_t, uint64_t, uint8_t*);
} fs_node_t;

fs_node_t* fs_root = NULL;
void schedule(void) {}

int vmm_verify_user_access(const void* ptr, size_t size, int is_write) {
    if (!ptr || size == 0) return 0;
    return 1;
}

int resolve_path(int dirfd, const char* path, char* resolved_path, size_t size) {
    strncpy(resolved_path, path, size);
    return 0;
}

void* kmalloc(size_t size) { return malloc(size); }
void kfree(void* p) { free(p); }
int split_path(char* path, char* parent, char* filename) {
    strcpy(filename, "file");
    strcpy(parent, "/dir");
    return 0;
}

fs_node_t* mock_node = NULL;
fs_node_t* vfs_lookup_ext(fs_node_t* root, const char* path, int follow) {
    if (strcmp(path, "/valid_symlink") == 0) return mock_node;
    if (strcmp(path, "/invalid_symlink") == 0) {
        fs_node_t* n = malloc(sizeof(fs_node_t));
        n->flags = 0; // NOT a symlink
        return n;
    }
    return NULL;
}

ssize_t mock_read(fs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    const char* target = "/target_path";
    strncpy((char*)buffer, target, size);
    return strlen(target);
}

// Function to test
static int64_t sys_readlinkat(int dirfd, const char* path, char* buf, size_t bufsiz) {
    if (!vmm_verify_user_access(buf, bufsiz, 1)) return -EFAULT;

    char kpath[256];
    int res = resolve_path(dirfd, path, kpath, sizeof(kpath));
    if (res < 0) return res;

    fs_node_t* node = vfs_lookup_ext(fs_root, kpath, 0); // 0 means do not follow the last symlink
    if (!node) return -ENOENT;

    ssize_t read_bytes = 0;
    if ((node->flags & 0x7) == FS_SYMLINK && node->read != NULL) {
         read_bytes = node->read(node, 0, bufsiz, (uint8_t*)buf);
    } else {
        free(node);
        return -EINVAL; // Not a symlink
    }

    return read_bytes;
}

static int64_t sys_readlink(const char* path, char* buf, size_t bufsiz) {
    return sys_readlinkat(AT_FDCWD, path, buf, bufsiz);
}

// Minimal stubs for rename/symlink
static int64_t sys_renameat(int olddirfd, const char* oldpath, int newdirfd, const char* newpath) { return 0; }
static int64_t sys_symlinkat(const char* target, int newdirfd, const char* linkpath) { return 0; }

static int64_t sys_rename(const char* oldpath, const char* newpath) {
    return sys_renameat(AT_FDCWD, oldpath, AT_FDCWD, newpath);
}

static int64_t sys_symlink(const char* target, const char* linkpath) {
    return sys_symlinkat(target, AT_FDCWD, linkpath);
}

int main() {
    mock_node = malloc(sizeof(fs_node_t));
    mock_node->flags = FS_SYMLINK;
    mock_node->read = mock_read;

    char buf[256];
    int64_t res;

    // Test valid readlinkat
    res = sys_readlinkat(AT_FDCWD, "/valid_symlink", buf, sizeof(buf));
    my_assert(res > 0);
    my_assert(strncmp(buf, "/target_path", res) == 0);

    // Test valid readlink
    res = sys_readlink("/valid_symlink", buf, sizeof(buf));
    my_assert(res > 0);
    my_assert(strncmp(buf, "/target_path", res) == 0);

    // Test invalid pointer
    res = sys_readlinkat(AT_FDCWD, "/valid_symlink", NULL, 0);
    my_assert(res == -EFAULT);

    // Test not found
    res = sys_readlinkat(AT_FDCWD, "/missing", buf, sizeof(buf));
    my_assert(res == -ENOENT);

    // Test not a symlink
    res = sys_readlinkat(AT_FDCWD, "/invalid_symlink", buf, sizeof(buf));
    my_assert(res == -EINVAL);

    // Test rename and symlink (stubbed)
    res = sys_rename("/old", "/new");
    my_assert(res == 0);

    res = sys_symlink("/target", "/link");
    my_assert(res == 0);

    printf("test_syscall_linux_readlink passed!\n");
    free(mock_node);
    return 0;
}
