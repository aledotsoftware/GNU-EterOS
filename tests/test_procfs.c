#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* Mock Headers */
#include "../include/types.h"
#include "../include/fs/vfs.h"
#include "../include/mm.h"
#include "../include/pmm.h"
#include "../include/timer.h"
#include "../include/hal.h"

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

/* HAL Console Mock */
void hal_console_write(const char* str) {
    // printf("%s", str); // Optional logging
}

/* Timer Mock */
static uint32_t mock_uptime_seconds = 0;
uint32_t timer_get_uptime_seconds(void) {
    return mock_uptime_seconds;
}

/* PMM Mocks */
static uint64_t mock_total_ram = 0;
static uint64_t mock_free_ram = 0;
static uint64_t mock_used_ram = 0;

uint64_t pmm_get_total_ram(void) { return mock_total_ram; }
uint64_t pmm_get_free_ram(void) { return mock_free_ram; }
uint64_t pmm_get_used_ram(void) { return mock_used_ram; }

/* Task Mocks */
#include "../include/task.h"
task_t current_task_mock;
task_t* task_get_current(void) {
    return &current_task_mock;
}

/* Include implementation under test */
/* This will include include/string.h which renames memcpy to eteros_memcpy */
#include "../kernel/string.c"
#include "../kernel/fs/procfs.c"

/* ========================================================================= */
/* Tests                                                                     */
/* ========================================================================= */

void test_proc_version() {
    printf("Running test_proc_version...\n");

    fs_node_t* root = procfs_init();
    assert(root != NULL);

    fs_node_t* version_node = root->finddir(root, "version");
    assert(version_node != NULL);
    assert(eteros_strcmp(version_node->name, "version") == 0);

    char buffer[256];
    eteros_memset(buffer, 0, sizeof(buffer));

    uint32_t read_bytes = version_node->read(version_node, 0, sizeof(buffer), (uint8_t*)buffer);
    assert(read_bytes > 0);
    assert(eteros_strncmp(buffer, "eterOS version", 14) == 0);

    printf("  Content: %s", buffer);

    /* Test partial read */
    char partial[10];
    read_bytes = version_node->read(version_node, 0, 6, (uint8_t*)partial);
    partial[6] = '\0';
    assert(read_bytes == 6);
    assert(eteros_strcmp(partial, "eterOS") == 0);

    kfree(version_node);
    kfree(root);
    printf("  Passed.\n");
}

void test_proc_uptime() {
    printf("Running test_proc_uptime...\n");

    mock_uptime_seconds = 12345;

    fs_node_t* root = procfs_init();
    fs_node_t* uptime_node = root->finddir(root, "uptime");
    assert(uptime_node != NULL);

    char buffer[256];
    eteros_memset(buffer, 0, sizeof(buffer));

    uint32_t read_bytes = uptime_node->read(uptime_node, 0, sizeof(buffer), (uint8_t*)buffer);
    buffer[read_bytes] = '\0';

    /* Expected: "12345.00 0.00\n" */
    printf("  Uptime string: %s", buffer);
    assert(eteros_strncmp(buffer, "12345.00 0.00", 13) == 0);

    kfree(uptime_node);
    kfree(root);
    printf("  Passed.\n");
}

void test_proc_meminfo() {
    printf("Running test_proc_meminfo...\n");

    /* Set mock values (in bytes) */
    mock_total_ram = 1024 * 1024 * 64; // 64 MB
    mock_free_ram = 1024 * 1024 * 32;  // 32 MB
    mock_used_ram = 1024 * 1024 * 32;  // 32 MB

    fs_node_t* root = procfs_init();
    fs_node_t* meminfo_node = root->finddir(root, "meminfo");
    assert(meminfo_node != NULL);

    char buffer[512];
    eteros_memset(buffer, 0, sizeof(buffer));

    uint32_t read_bytes = meminfo_node->read(meminfo_node, 0, sizeof(buffer), (uint8_t*)buffer);
    buffer[read_bytes] = '\0';

    printf("  Meminfo content:\n%s", buffer);

    /* Verify key strings exist */
    /* Note: pmm returns bytes, but meminfo prints kB */
    /* 64MB = 65536 kB */
    /* 32MB = 32768 kB */

    assert(strstr(buffer, "MemTotal:       65536 kB"));
    assert(strstr(buffer, "MemFree:        32768 kB"));
    assert(strstr(buffer, "MemUsed:        32768 kB"));

    kfree(meminfo_node);
    kfree(root);
    printf("  Passed.\n");
}

void test_proc_directory() {
    printf("Running test_proc_directory...\n");

    fs_node_t* root = procfs_init();
    assert(root->flags == FS_DIRECTORY);

    struct dirent entry;

    /* Index 0: version */
    assert(root->readdir(root, 0, &entry) == 0);
    assert(eteros_strcmp(entry.name, "version") == 0);

    /* Index 1: uptime */
    assert(root->readdir(root, 1, &entry) == 0);
    assert(eteros_strcmp(entry.name, "uptime") == 0);

    /* Index 2: meminfo */
    assert(root->readdir(root, 2, &entry) == 0);
    assert(eteros_strcmp(entry.name, "meminfo") == 0);

    /* Index 3: self */
    assert(root->readdir(root, 3, &entry) == 0);
    assert(eteros_strcmp(entry.name, "self") == 0);

    /* Index 4: EOF */
    assert(root->readdir(root, 4, &entry) != 0);

    /* Find invalid */
    fs_node_t* invalid = root->finddir(root, "invalid");
    assert(invalid == NULL);

    kfree(root);
    printf("  Passed.\n");
}

int main() {
    printf("Starting ProcFS tests...\n");
    test_proc_version();
    test_proc_uptime();
    test_proc_meminfo();
    test_proc_directory();
    printf("All ProcFS tests passed!\n");
    return 0;
}
