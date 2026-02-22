/* Mock Headers to prevent including real kernel headers that might have inline asm or dependencies */
#define ETEROS_LOCK_H
#define ETEROS_MM_H
#define ETEROS_HAL_H
#define ETEROS_KEYBOARD_H
#define ETEROS_VGA_H
/* We define DRIVERS_TTY_H to prevent including include/drivers/tty.h
   because we want to provide our own prototype/implementation of tty_create_node logic
   via the mock below, and we don't want the header to interfere (though it just declares it).
   Actually, including the header is fine, but since we are in a test, let's control it. */
#define DRIVERS_TTY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

/* Define types usually found in types.h or lock.h */
typedef int spinlock_t;

/* Define fs_node_t and constants by including fs/vfs.h.
   We need to ensure -Iinclude is passed to gcc.
   Since we defined ETEROS_LOCK_H, lock.h inclusion will be skipped.
*/
#include <fs/vfs.h>

/* Need input/event.h for devfs.c */
#include <input/event.h>

/* Mock implementations of kernel functions */

void spin_lock(spinlock_t *lock) { (void)lock; }
void spin_unlock(spinlock_t *lock) { (void)lock; }

void* kmalloc(size_t size) {
    return malloc(size);
}

void kfree(void* ptr) {
    free(ptr);
}

void* kcalloc(size_t num, size_t size) {
    return calloc(num, size);
}

/* HAL mocks */
void hal_console_write(const char* str) {
    printf("%s", str);
}

/* Keyboard/VGA mocks */
char keyboard_getchar(void) { return 0; }
void terminal_putchar(char c) { (void)c; }

/* Input mocks */
int input_read(input_event_t* buffer, int count) { (void)buffer; (void)count; return 0; }
int input_read_mouse(input_event_t* buffer, int count) { (void)buffer; (void)count; return 0; }

/* TTY Create Node Mock */
fs_node_t* tty_create_node(void) {
    fs_node_t* node = (fs_node_t*)malloc(sizeof(fs_node_t));
    if (!node) return NULL;
    memset(node, 0, sizeof(fs_node_t));
    strcpy(node->name, "tty_mock");
    node->flags = FS_CHARDEVICE;
    node->inode = 999; /* Distinct inode to verify it's from here */
    return node;
}

/* Include code under test */
/* We include the .c file directly to test static functions and internal logic */
#include "../kernel/fs/devfs.c"

int main() {
    printf("Starting DevFS TTY Test...\n");

    /* Initialize DevFS */
    /* devfs_init is non-static in devfs.c, so we can call it. */
    fs_node_t* root = devfs_init();
    if (!root) {
        printf("FAIL: devfs_init returned NULL\n");
        return 1;
    }

    if (strcmp(root->name, "dev") != 0) {
        printf("FAIL: root name is '%s', expected 'dev'\n", root->name);
        return 1;
    }

    /* Lookup "tty" */
    printf("Looking up 'tty'...\n");
    /* devfs_finddir is static in devfs.c, but since we included the .c file,
       it is available in this compilation unit. */
    fs_node_t* tty = devfs_finddir(root, "tty");

    if (tty) {
        printf("Found node: %s, inode: %d\n", tty->name, tty->inode);
        if (strcmp(tty->name, "tty_mock") == 0 && tty->inode == 999) {
            printf("PASS: 'tty' lookup returned mock tty node.\n");
        } else {
            printf("FAIL: 'tty' lookup returned unexpected node.\n");
            return 1;
        }
        kfree(tty);
    } else {
        printf("FAIL: 'tty' not found.\n");
        return 1;
    }

    /* Lookup "null" to verify normal devfs logic still works */
    printf("Looking up 'null'...\n");
    fs_node_t* null_dev = devfs_finddir(root, "null");
    if (null_dev) {
        printf("Found node: %s, inode: %d\n", null_dev->name, null_dev->inode);
        if (strcmp(null_dev->name, "null") == 0) {
             printf("PASS: 'null' lookup works.\n");
        } else {
             printf("FAIL: 'null' lookup returned wrong name.\n");
             return 1;
        }
        kfree(null_dev);
    } else {
        printf("FAIL: 'null' not found.\n");
        return 1;
    }

    kfree(root);
    return 0;
}
