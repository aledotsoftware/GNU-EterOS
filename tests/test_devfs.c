#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

/* Mock Headers */
#include "../include/types.h"
#include "../include/fs/vfs.h"
#include "../include/mm.h"
#include "../include/hal.h"
#include "../include/keyboard.h"
#include "../include/vga.h"
#include "../include/input/event.h"
#include "../include/lock.h"

/* ========================================================================= */
/* Mocks Implementation                                                      */
/* ========================================================================= */

/* Memory Management Mocks */
void* kmalloc(size_t size) {
    return malloc(size);
}

void kfree(void* ptr) {
    free(ptr);
}

/* Keyboard/Input Mocks */
char keyboard_getchar(void) {
    return 'a';
}

void terminal_putchar(char c) {
    // mock
}

int input_read(input_event_t* events, int count) {
    return 0;
}

int input_read_mouse(input_event_t* events, int count) {
    return 0;
}

/* HAL Mock */
void hal_console_write(const char* str) {
    // mock
}

/* Spinlock Mock (already handled if we mock lock.h properly, but since it's inline we need it to work or do nothing) */
/* lock.h provides static inline spin_lock so we don't need to implement it. */

/* Include implementation under test */
/* This will include include/string.h which renames memset, strcmp, etc. */
#include "../kernel/string.c"
#include "../kernel/fs/devfs.c"

/* ========================================================================= */
/* Tests                                                                     */
/* ========================================================================= */

void test_dev_random_read() {
    printf("Running test_dev_random_read...\n");

    fs_node_t* root = devfs_init();
    assert(root != NULL);

    fs_node_t* random_node = devfs_finddir(root, "random");
    assert(random_node != NULL);
    assert(eteros_strcmp(random_node->name, "random") == 0);
    assert(random_node->read == dev_random_read);

    /* Test 1: Read a chunk of bytes */
    uint8_t buffer1[64];
    eteros_memset(buffer1, 0, sizeof(buffer1));

    ssize_t read_bytes1 = random_node->read(random_node, 0, sizeof(buffer1), buffer1);
    assert(read_bytes1 == sizeof(buffer1));

    /* Test 2: Read another chunk of bytes */
    uint8_t buffer2[64];
    eteros_memset(buffer2, 0, sizeof(buffer2));

    ssize_t read_bytes2 = random_node->read(random_node, 0, sizeof(buffer2), buffer2);
    assert(read_bytes2 == sizeof(buffer2));

    /* Verify that the two chunks are not identical (entropy check) */
    /* This has a very small chance of failing legitimately if the RNG produces exactly the same 64 bytes,
       but that chance is practically zero (1 in 256^64). */
    int differs = 0;
    for (size_t i = 0; i < sizeof(buffer1); i++) {
        if (buffer1[i] != buffer2[i]) {
            differs = 1;
            break;
        }
    }
    assert(differs == 1);

    /* Verify that writing to the node works (mixes entropy) */
    assert(random_node->write == dev_random_write);
    uint8_t seed_data[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    uint32_t written = random_node->write(random_node, 0, sizeof(seed_data), seed_data);
    assert(written == sizeof(seed_data));

    /* Read again after seeding */
    uint8_t buffer3[64];
    ssize_t read_bytes3 = random_node->read(random_node, 0, sizeof(buffer3), buffer3);
    assert(read_bytes3 == sizeof(buffer3));

    /* Clean up */
    kfree(random_node);
    kfree(root);

    printf("  Passed.\n");
}

int main() {
    printf("Starting DevFS tests...\n");
    test_dev_random_read();
    printf("All DevFS tests passed!\n");
    return 0;
}
