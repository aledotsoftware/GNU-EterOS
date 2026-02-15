/**
 * éterOS Mini-LibC - Memory Allocation (malloc/free)
 * Simple brk-based allocator (musl-compatible interface)
 */

#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

/* Internal brk syscall */
static inline long syscall1_brk(long n, long a1) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

/* Simple block header */
typedef struct block {
    uint64_t size;       /* Size of the data area */
    int      free;       /* Is this block free? */
    struct block *next;  /* Next block in the list */
} block_t;

#define BLOCK_SIZE sizeof(block_t)
#define ALIGN(x) (((x) + 15) & ~15)  /* 16-byte alignment */

static block_t *free_list = (void*)0;
static uint64_t current_brk = 0;

static uint64_t get_brk(void) {
    return (uint64_t)syscall1_brk(SYS_brk, 0);
}

static uint64_t set_brk(uint64_t new_brk) {
    return (uint64_t)syscall1_brk(SYS_brk, (long)new_brk);
}

static block_t *find_free_block(uint64_t size) {
    block_t *curr = free_list;
    while (curr) {
        if (curr->free && curr->size >= size) return curr;
        curr = curr->next;
    }
    return (void*)0;
}

static block_t *request_space(uint64_t size) {
    if (current_brk == 0) {
        current_brk = get_brk();
        if (current_brk == 0) current_brk = 0x400000; /* Fallback */
    }

    uint64_t total = BLOCK_SIZE + size;
    uint64_t new_brk = current_brk + total;
    uint64_t result = set_brk(new_brk);

    if (result < new_brk) return (void*)0; /* Failed */

    block_t *block = (block_t *)current_brk;
    block->size = size;
    block->free = 0;
    block->next = (void*)0;
    current_brk = new_brk;

    return block;
}

void *malloc(size_t size) {
    if (size == 0) return (void*)0;
    size = ALIGN(size);

    block_t *block = find_free_block(size);
    if (block) {
        block->free = 0;
        return (void *)((uint8_t *)block + BLOCK_SIZE);
    }

    block = request_space(size);
    if (!block) return (void*)0;

    /* Add to list */
    if (!free_list) {
        free_list = block;
    } else {
        block_t *curr = free_list;
        while (curr->next) curr = curr->next;
        curr->next = block;
    }

    return (void *)((uint8_t *)block + BLOCK_SIZE);
}

void free(void *ptr) {
    if (!ptr) return;
    block_t *block = (block_t *)((uint8_t *)ptr - BLOCK_SIZE);
    block->free = 1;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return (void*)0; }

    block_t *block = (block_t *)((uint8_t *)ptr - BLOCK_SIZE);
    if (block->size >= size) return ptr;

    void *new_ptr = malloc(size);
    if (!new_ptr) return (void*)0;
    memcpy(new_ptr, ptr, block->size);
    free(ptr);
    return new_ptr;
}
