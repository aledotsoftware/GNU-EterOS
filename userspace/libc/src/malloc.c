#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define BLOCK_SIZE ALIGN(sizeof(block_t))

typedef struct block {
    size_t size;
    struct block *next;
    struct block *prev;
    int free;
    int padding; // explicit padding to ensure sizeof is multiple of 16
} block_t;

// sizeof(block_t) is 8+8+8+4+4 = 32 bytes.
// ALIGN(32) = 32.

static block_t *heap_start = NULL;

static block_t *find_free_block(block_t **last, size_t size) {
    block_t *current = heap_start;
    while (current) {
        if (current->free && current->size >= size) {
            return current;
        }
        *last = current;
        current = current->next;
    }
    return NULL;
}

static block_t *request_space(block_t *last, size_t size) {
    block_t *block;
    block = sbrk(0);
    void *request = sbrk(size + BLOCK_SIZE);

    if (request == (void*) -1) {
        return NULL; // sbrk failed
    }

    if (last) {
        last->next = block;
    }

    block->size = size;
    block->next = NULL;
    block->prev = last;
    block->free = 0;

    return block;
}

static void split_block(block_t *block, size_t size) {
    // block->size is the size of data (excluding header).
    // we want to split if remaining space is enough for another header + min data.
    if (block->size >= size + BLOCK_SIZE + ALIGNMENT) {
        block_t *new_block = (block_t *)((uint8_t*)block + BLOCK_SIZE + size);

        new_block->size = block->size - size - BLOCK_SIZE;
        new_block->next = block->next;
        new_block->prev = block;
        new_block->free = 1;

        if (new_block->next) {
            new_block->next->prev = new_block;
        }

        block->size = size;
        block->next = new_block;
    }
}

static void coalesce_block(block_t *block) {
    if (block->next && block->next->free) {
        block->size += BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    if (block->prev && block->prev->free) {
        block->prev->size += BLOCK_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
        // block is now merged into prev.
    }
}

void *malloc(size_t size) {
    block_t *block;

    if (size <= 0) {
        return NULL;
    }

    /* Check for integer overflow before alignment and allocation */
    if (size > SIZE_MAX - BLOCK_SIZE - ALIGNMENT) {
        return NULL;
    }

    size = ALIGN(size);

    if (!heap_start) {
        block = request_space(NULL, size);
        if (!block) {
            return NULL;
        }
        heap_start = block;
    } else {
        block_t *last = heap_start;
        block = find_free_block(&last, size);
        if (!block) {
            block = request_space(last, size);
            if (!block) {
                return NULL;
            }
        } else {
            block->free = 0;
            split_block(block, size);
        }
    }

    return (void *)((uint8_t*)block + BLOCK_SIZE);
}

void free(void *ptr) {
    if (!ptr) {
        return;
    }

    block_t *block = (block_t*)((uint8_t*)ptr - BLOCK_SIZE);
    block->free = 1;

    coalesce_block(block);
}

void *calloc(size_t nelem, size_t elsize) {
    /* Check for multiplication overflow */
    if (nelem && elsize > SIZE_MAX / nelem) {
        return NULL;
    }

    size_t size = nelem * elsize;
    void *ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size);
    }

    if (size == 0) {
        free(ptr);
        return NULL;
    }

    /* Check for integer overflow before alignment */
    if (size > SIZE_MAX - BLOCK_SIZE - ALIGNMENT) {
        return NULL;
    }

    size = ALIGN(size);
    block_t *block = (block_t*)((uint8_t*)ptr - BLOCK_SIZE);

    if (block->size >= size) {
        split_block(block, size); // Attempt to recover unused space
        return ptr;
    }

    // Attempt to merge with next block if free and sufficient
    if (block->next && block->next->free && (block->size + BLOCK_SIZE + block->next->size >= size)) {
        block->size += BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;

        split_block(block, size);
        return ptr;
    }

    void *new_ptr = malloc(size);
    if (!new_ptr) {
        return NULL;
    }

    memcpy(new_ptr, ptr, block->size);
    free(ptr);
    return new_ptr;
}
