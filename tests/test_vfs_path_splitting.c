#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

/* Include string.h from host first to get standard declarations?
   No, my custom string.h will override them if included later with macros.
   I need the custom implementations available.
*/

/* Mock types */
#include "../include/types.h"
#include "../include/fs/vfs.h"
#include "../include/mm.h"
#include "../include/hal.h"

/* Mock HAL Console */
void hal_console_write(const char* str) {
    // printf("%s", str);
}

/* Mock Memory Management */
void* kmalloc(size_t size) {
    return malloc(size);
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

/* Mock Finddir Logic */
fs_node_t* mock_finddir(fs_node_t* node, char* name) {
    // Check if name is "A"*127
    int is_long_A = 1;
    if (eteros_strlen(name) != 127) is_long_A = 0;
    else {
        for (int i = 0; i < 127; i++) {
            if (name[i] != 'A') {
                is_long_A = 0;
                break;
            }
        }
    }

    if (is_long_A) {
        // Return Directory Node
        fs_node_t* dir = (fs_node_t*)malloc(sizeof(fs_node_t));
        eteros_memset(dir, 0, sizeof(fs_node_t));
        // Use standard strcpy for host buffer init if name is short, but we have custom strlcpy
        // Let's use eteros_strlcpy
        eteros_strlcpy(dir->name, "long_dir", sizeof(dir->name));
        dir->flags = FS_DIRECTORY;
        dir->finddir = mock_finddir;
        dir->ref_count = 1;
        return dir;
    }

    if (eteros_strcmp(name, "B") == 0) {
        // Return File Node
        fs_node_t* file = (fs_node_t*)malloc(sizeof(fs_node_t));
        eteros_memset(file, 0, sizeof(fs_node_t));
        eteros_strlcpy(file->name, "B", sizeof(file->name));
        file->flags = FS_FILE;
        file->ref_count = 1;
        return file;
    }

    return NULL;
}

/* Include source under test */
#include "../kernel/fs/vfs.c"

void test_vfs_path_splitting() {
    printf("Running test_vfs_path_splitting...\n");

    // Setup Root
    fs_node_t* root = (fs_node_t*)malloc(sizeof(fs_node_t));
    eteros_memset(root, 0, sizeof(fs_node_t));
    eteros_strlcpy(root->name, "root", sizeof(root->name));
    root->flags = FS_DIRECTORY;
    root->finddir = mock_finddir;
    root->ref_count = 1;

    // Construct Path: "A" * 127 + "B"
    char path[200];
    eteros_memset(path, 'A', 127);
    path[127] = 'B';
    path[128] = '\0';

    printf("Testing path lookup for 127 'A's followed by 'B' (no slash)...\n");

    // Vulnerable code interprets this as "A...A/B"
    // So it finds "A...A" dir, then "B" file inside it.
    fs_node_t* result = vfs_lookup(root, path);

    if (result != NULL) {
        if (eteros_strcmp(result->name, "B") == 0) {
            printf("VULNERABILITY CONFIRMED: vfs_lookup treated 'A...AB' as 'A...A/B'!\n");
            exit(1);
        } else {
            printf("Unexpected result: Found node '%s'\n", result->name);
        }
        kfree(result);
    } else {
        printf("PASSED: vfs_lookup returned NULL (blocked).\n");
    }

    // Also test normal case: "A...A/B"
    path[127] = '/';
    path[128] = 'B';
    path[129] = '\0';
    printf("Testing normal path lookup 'A...A/B'...\n");

    result = vfs_lookup(root, path);
    if (result != NULL && eteros_strcmp(result->name, "B") == 0) {
        printf("Normal path lookup working correctly.\n");
        kfree(result);
    } else {
        printf("ERROR: Normal path lookup failed!\n");
        exit(2);
    }

    kfree(root);
}

int main() {
    test_vfs_path_splitting();
    return 0;
}
