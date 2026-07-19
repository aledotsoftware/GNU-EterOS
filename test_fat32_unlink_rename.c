#define __ETEROS_HOST_TEST__ 1
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "include/types.h"
#include "include/fs/vfs.h"

// Globals
fs_node_t *fs_root = NULL;

void spin_lock(void* lock) { (void)lock; }
void spin_unlock(void* lock) { (void)lock; }

void* kmalloc(size_t size) { return malloc(size); }
void kfree(void* ptr) { if (ptr) free(ptr); }

// Simple strings
size_t eteros_strlen(const char* s) { return strlen(s); }
int eteros_strcmp(const char* a, const char* b) { return strcmp(a, b); }
char* eteros_strncpy(char* dest, const char* src, size_t n) { return strncpy(dest, src, n); }
char* eteros_strcat(char* dest, const char* src) { return strcat(dest, src); }
int eteros_strncmp(const char* a, const char* b, size_t n) { return strncmp(a, b, n); }

int bcache_read(uint32_t disk_id, uint64_t sector, uint8_t *buffer) { return 0; }
int bcache_write(uint32_t disk_id, uint64_t sector, const uint8_t *buffer) { return 0; }

// Include code
#include "kernel/fs/fat32.c"

int main() {
    printf("Compiles!\n");
    return 0;
}
