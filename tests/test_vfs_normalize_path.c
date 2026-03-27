#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../include/types.h"
#include "../include/fs/vfs.h"

// Mock definitions
int task_get_current() { return 0; }
void* kmalloc(size_t size) { return malloc(size); }
void kfree(void* ptr) { free(ptr); }
void serial_write_string(const char* str) {}
fs_node_t* tty_create_node(void) { return NULL; }
fs_node_t* fs_root = NULL;

// Include source under test
#include "../kernel/string.c"
#include "../kernel/fs/vfs.c"

void assert_str_eq(const char* expected, const char* actual, const char* msg) {
    if (eteros_strcmp(expected, actual) != 0) {
        printf("FAILED: %s. Expected '%s', got '%s'\n", msg, expected, actual);
        exit(1);
    }
}

void test_vfs_normalize_path() {
    char out[128];
    int res;

    // Test: Absolute path
    res = vfs_normalize_path(out, sizeof(out), "/abc/def", NULL);
    if (res != 0) { printf("FAILED: absolute path\n"); exit(1); }
    assert_str_eq("/abc/def", out, "absolute path");

    // Test: Relative path with base_dir
    res = vfs_normalize_path(out, sizeof(out), "def", "/abc");
    if (res != 0) { printf("FAILED: relative path\n"); exit(1); }
    assert_str_eq("/abc/def", out, "relative path with base_dir");

    // Test: Relative path resolving to root
    res = vfs_normalize_path(out, sizeof(out), "..", "/abc");
    if (res != 0) { printf("FAILED: relative path to root\n"); exit(1); }
    assert_str_eq("/", out, "relative path to root");

    // Test: Extra slashes
    res = vfs_normalize_path(out, sizeof(out), "/abc///def//ghi", NULL);
    if (res != 0) { printf("FAILED: extra slashes\n"); exit(1); }
    assert_str_eq("/abc/def/ghi", out, "extra slashes");

    // Test: Path with .
    res = vfs_normalize_path(out, sizeof(out), "/abc/./def", NULL);
    if (res != 0) { printf("FAILED: path with .\n"); exit(1); }
    assert_str_eq("/abc/def", out, "path with .");

    // Test: Path with ..
    res = vfs_normalize_path(out, sizeof(out), "/abc/def/../ghi", NULL);
    if (res != 0) { printf("FAILED: path with ..\n"); exit(1); }
    assert_str_eq("/abc/ghi", out, "path with ..");

    // Test: Path with lots of ..
    res = vfs_normalize_path(out, sizeof(out), "/abc/def/../../../../ghi", NULL);
    if (res != 0) { printf("FAILED: path with lots of ..\n"); exit(1); }
    assert_str_eq("/ghi", out, "path with lots of ..");

    // Test: Empty path handling
    res = vfs_normalize_path(out, sizeof(out), "", "/abc");
    if (res != 0) { printf("FAILED: empty path\n"); exit(1); }
    assert_str_eq("/abc", out, "empty path");

    // Test: root path
    res = vfs_normalize_path(out, sizeof(out), "/", NULL);
    if (res != 0) { printf("FAILED: root path\n"); exit(1); }
    assert_str_eq("/", out, "root path");

    // Test: root path with trailing slash
    res = vfs_normalize_path(out, sizeof(out), "/abc/", NULL);
    if (res != 0) { printf("FAILED: root path\n"); exit(1); }
    assert_str_eq("/abc", out, "root path with trailing slash");

    // Test: trailing slash with base_dir
    res = vfs_normalize_path(out, sizeof(out), "def/", "/abc/");
    if (res != 0) { printf("FAILED: relative path with trailing slash\n"); exit(1); }
    assert_str_eq("/abc/def", out, "relative path with trailing slash");

    // Test: base dir missing trailing slash
    res = vfs_normalize_path(out, sizeof(out), "def", "/abc");
    if (res != 0) { printf("FAILED: relative path with base_dir without trailing slash\n"); exit(1); }
    assert_str_eq("/abc/def", out, "relative path with base_dir without trailing slash");

    // Test: base dir root trailing slash
    res = vfs_normalize_path(out, sizeof(out), "def", "/");
    if (res != 0) { printf("FAILED: relative path with base_dir without trailing slash\n"); exit(1); }
    assert_str_eq("/def", out, "relative path with root base_dir");

    printf("All basic and edge-case vfs_normalize_path tests passed!\n");
}

void test_edge_cases() {
    char out[128];
    int res;

    // Test: NULL out_path
    res = vfs_normalize_path(NULL, sizeof(out), "/abc", NULL);
    if (res != -1) { printf("FAILED: NULL out_path should return -1\n"); exit(1); }

    // Test: NULL path
    res = vfs_normalize_path(out, sizeof(out), NULL, NULL);
    if (res != -1) { printf("FAILED: NULL path should return -1\n"); exit(1); }

    // Test: size <= 0
    res = vfs_normalize_path(out, 0, "/abc", NULL);
    if (res != -1) { printf("FAILED: size 0 should return -1\n"); exit(1); }

    // Test: too many segments (> 64)
    char long_path[512] = "";
    for (int i = 0; i < 65; i++) {
        eteros_strlcat(long_path, "/a", sizeof(long_path));
    }
    res = vfs_normalize_path(out, sizeof(out), long_path, NULL);
    if (res != -1) { printf("FAILED: > 64 segments should return -1 (returned %d instead)\n", res); exit(1); }

    // Test: path that exceeds temp buffer size in vfs_normalize_path (temp[512])
    char very_long_path[1024];
    eteros_memset(very_long_path, 'x', 1000);
    very_long_path[0] = '/';
    very_long_path[1000] = '\0';
    res = vfs_normalize_path(out, sizeof(out), very_long_path, NULL);
    // Should be truncated but handled gracefully
    if (res != 0) { printf("FAILED: very long path returned %d\n", res); exit(1); }

    // Test: missing base dir and relative path when in host test mode (base_dir defaults to "/")
    // Under __ETEROS_HOST_TEST__, vfs.c explicitly falls back to "/" instead of returning -1:
    // #else
    //         if (base_dir) {
    //             strlcpy(temp, base_dir, sizeof(temp));
    //         } else {
    //             strlcpy(temp, "/", sizeof(temp));
    //         }
    // #endif
    res = vfs_normalize_path(out, sizeof(out), "abc", NULL);
    if (res != 0) { printf("FAILED: relative path without base_dir\n"); exit(1); }
    assert_str_eq("/abc", out, "relative path without base_dir");

    printf("All error conditions passed!\n");
}

int main() {
    test_vfs_normalize_path();
    test_edge_cases();
    printf("All tests completed successfully!\n");
    return 0;
}
