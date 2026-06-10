
#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

/* Include string implementation (provides eteros_strlen, eteros_strlcpy, eteros_strcmp, etc) */
#include "../kernel/string.c"
#include "../include/task.h"
#include "../include/fs/vfs.h"

static task_t mock_task = {0};
int task_get_max(void) { return 0; }
task_t* task_get_at(int i) { (void)i; return NULL; }
void task_wakeup(task_t* t) { (void)t; }
task_t* task_get_current(void) { return &mock_task; }
bool keyboard_has_input(void) { return false; }
void task_yield(void) {}

/* Mock framebuffer */
uint32_t framebuffer_get_width(void) { return 1024; }
uint32_t framebuffer_get_height(void) { return 768; }
uint32_t framebuffer_get_bpp(void) { return 32; }
uint32_t framebuffer_get_pitch(void) { return 1024 * 4; }

/* Mock memory allocation */
void* kmalloc(size_t size) {
    return malloc(size);
}

void kfree(void* ptr) {
    free(ptr);
}

/* Mock keyboard and input dependencies */

char keyboard_getchar(void) {
    return 'a';
}

/* Mock terminal output */
void terminal_putchar(char c) {
    (void)c;
}

/* Mock input events */
#include "../include/input/event.h"

int input_read(input_event_t* events, int count) {
    (void)events;
    return count;
}

int input_read_mouse(input_event_t* events, int count) {
    (void)events;
    return count;
}

int input_pending(void) {
    return 1;
}

int input_mouse_pending(void) {
    return 1;
}


/* Stubs for DRM functions */
int devfs_dri_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) { return 1; }
fs_node_t *devfs_dri_finddir(fs_node_t *node, char *name) { return 0; }

/* Include DevFS source directly to test static functions */
#include "../kernel/fs/devfs.c"

void test_devfs_finddir() {
    printf("Testing devfs_finddir...\n");

    fs_node_t *root = devfs_init();
    ASSERT(root != NULL);

    // Test existing nodes
    const char *existing_nodes[] = {
        "null",
        "zero",
        "tty",
        "random",
        "urandom",
        "input"
    };

    for (size_t i = 0; i < sizeof(existing_nodes) / sizeof(existing_nodes[0]); i++) {
        fs_node_t *node = devfs_finddir(root, (char*)existing_nodes[i]);
        if (node == NULL) {
            printf("FAILED: Could not find existing node '%s'\n", existing_nodes[i]);
            exit(1);
        }

        // Ensure name was set properly
        ASSERT(eteros_strcmp(node->name, existing_nodes[i]) == 0);

        kfree(node);
    }

    // Test non-existing node
    fs_node_t *non_existing_node = devfs_finddir(root, "does_not_exist");
    if (non_existing_node != NULL) {
        printf("FAILED: Found non-existing node 'does_not_exist'\n");
        exit(1);
    }

    // Test NULL string
    fs_node_t *null_node = devfs_finddir(root, NULL);
    if (null_node != NULL) {
        printf("FAILED: Handled NULL name incorrectly\n");
        exit(1);
    }

    printf("PASSED: devfs_finddir tests.\n");
}

int main() {
    printf("Running DevFS finddir tests...\n");
    test_devfs_finddir();
    return 0;
}
void serial_write_string(const char* s) {}


// STUBS ADDED
#include <stdint.h>
int ata_read_sector(uint32_t lba, uint8_t *buffer) { return 0; }
int ata_write_sector(uint32_t lba, uint8_t *buffer) { return 0; }
