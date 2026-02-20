#define __ETEROS_HOST_TEST__ 1
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* Setup includes */
#include "../include/types.h"
#include "../include/fs/vfs.h"
#include "../include/fs/ramfs.h"
#include "../include/fs/overlay.h"
#include "../include/mm.h"
#include "../include/lock.h"

/* Mock kernel dependencies */
void* kmalloc(size_t size) { return calloc(1, size); }
void* kcalloc(size_t num, size_t size) { return calloc(num, size); }
void kfree(void* ptr) { free(ptr); }
void* krealloc(void* ptr, size_t size) { return realloc(ptr, size); }

void hal_console_write(const char* str) { printf("%s", str); }
void klog(int level, const char* fmt, ...) { (void)level; (void)fmt; }

/* Include implementation */
#include "../kernel/string.c"
#include "../kernel/fs/vfs.c"
#include "../kernel/fs/ramfs.c"
#include "../kernel/fs/overlay.c"

/* Mock TTY needed by vfs.c */
fs_node_t* tty_create_node(void) { return NULL; }

/* Test Cases */
void test_overlay_lookup() {
    printf("Test: Overlay Lookup\n");

    /* 1. Setup Lower (RamFS for simplicity, pretended to be RO) */
    fs_node_t *lower = ramfs_init();
    create_fs(lower, "lower_file", 0);

    /* 2. Setup Upper (RamFS) */
    fs_node_t *upper = ramfs_init();
    create_fs(upper, "upper_file", 0);

    /* 3. Setup Overlay */
    fs_node_t *overlay = overlay_init(lower, upper);
    assert(overlay != NULL);

    /* 4. Lookup file in Lower */
    fs_node_t *f1 = vfs_lookup(overlay, "lower_file");
    assert(f1 != NULL);
    assert(strcmp(f1->name, "lower_file") == 0);
    vfs_destroy_node(f1);

    /* 5. Lookup file in Upper */
    fs_node_t *f2 = vfs_lookup(overlay, "upper_file");
    assert(f2 != NULL);
    assert(strcmp(f2->name, "upper_file") == 0);
    vfs_destroy_node(f2);

    /* 6. Lookup non-existent */
    fs_node_t *f3 = vfs_lookup(overlay, "non_existent");
    assert(f3 == NULL);

    printf("PASS\n");
}

void test_overlay_whiteout() {
    printf("Test: Overlay Whiteout\n");

    fs_node_t *lower = ramfs_init();
    create_fs(lower, "target", 0);

    fs_node_t *upper = ramfs_init();

    fs_node_t *overlay = overlay_init(lower, upper);

    /* Verify existence */
    fs_node_t *f = vfs_lookup(overlay, "target");
    assert(f != NULL);
    vfs_destroy_node(f);

    /* Unlink (creates whiteout) */
    int res = unlink_fs(overlay, "target");
    assert(res == 0);

    /* Verify hidden */
    f = vfs_lookup(overlay, "target");
    assert(f == NULL);

    /* Verify whiteout exists in upper manually */
    fs_node_t *wh = vfs_lookup(upper, ".wh.target");
    assert(wh != NULL);
    vfs_destroy_node(wh);

    printf("PASS\n");
}

void test_overlay_write_copyup() {
    printf("Test: Overlay Write Copy-Up\n");

    fs_node_t *lower = ramfs_init();
    create_fs(lower, "data.txt", 0);

    /* Write data to lower manually */
    fs_node_t *l_file = vfs_lookup(lower, "data.txt");
    char *msg = "Hello";
    write_fs(l_file, 0, 5, (uint8_t*)msg);
    vfs_destroy_node(l_file);

    fs_node_t *upper = ramfs_init();
    fs_node_t *overlay = overlay_init(lower, upper);

    /* Open overlay file */
    fs_node_t *o_file = vfs_lookup(overlay, "data.txt");
    assert(o_file != NULL);

    /* Verify read from lower */
    char buf[10] = {0};
    read_fs(o_file, 0, 5, (uint8_t*)buf);
    assert(strcmp(buf, "Hello") == 0);

    /* Write to overlay (triggers copy-up) */
    char *new_msg = "World";
    write_fs(o_file, 0, 5, (uint8_t*)new_msg);

    /* Verify read new data */
    memset(buf, 0, 10);
    read_fs(o_file, 0, 5, (uint8_t*)buf);
    assert(strcmp(buf, "World") == 0);

    /* Verify upper has file now */
    fs_node_t *u_file = vfs_lookup(upper, "data.txt");
    assert(u_file != NULL);
    memset(buf, 0, 10);
    read_fs(u_file, 0, 5, (uint8_t*)buf);
    assert(strcmp(buf, "World") == 0);
    vfs_destroy_node(u_file);

    /* Verify lower is unchanged */
    l_file = vfs_lookup(lower, "data.txt");
    memset(buf, 0, 10);
    read_fs(l_file, 0, 5, (uint8_t*)buf);
    assert(strcmp(buf, "Hello") == 0);
    vfs_destroy_node(l_file);

    vfs_destroy_node(o_file);
    printf("PASS\n");
}

void test_overlay_subdir_creation() {
    printf("Test: Overlay Subdir Creation\n");

    /* Setup Lower with /etc/config */
    fs_node_t *lower = ramfs_init();
    mkdir_fs(lower, "etc", 0);
    fs_node_t *etc = vfs_lookup(lower, "etc");
    create_fs(etc, "config", 0);
    vfs_destroy_node(etc);

    /* Setup Upper (Empty) */
    fs_node_t *upper = ramfs_init();

    /* Overlay */
    fs_node_t *overlay = overlay_init(lower, upper);

    /* Lookup /etc */
    fs_node_t *o_etc = vfs_lookup(overlay, "etc");
    assert(o_etc != NULL);

    /* Initially, o_etc has NO upper node (only lower) */
    /* Try to create file in /etc */
    /* This should trigger copy_up of 'etc' directory to upper */
    int res = create_fs(o_etc, "new_file", 0);
    assert(res == 0);

    /* Verify new_file exists in overlay */
    fs_node_t *nf = vfs_lookup(o_etc, "new_file");
    assert(nf != NULL);
    vfs_destroy_node(nf);

    /* Verify 'etc' exists in upper */
    fs_node_t *u_etc = vfs_lookup(upper, "etc");
    assert(u_etc != NULL);

    /* Verify new_file exists in upper/etc */
    fs_node_t *u_nf = vfs_lookup(u_etc, "new_file");
    assert(u_nf != NULL);
    vfs_destroy_node(u_nf);
    vfs_destroy_node(u_etc);

    vfs_destroy_node(o_etc);
    printf("PASS\n");
}

int main() {
    test_overlay_lookup();
    test_overlay_whiteout();
    test_overlay_write_copyup();
    test_overlay_subdir_creation();
    return 0;
}
