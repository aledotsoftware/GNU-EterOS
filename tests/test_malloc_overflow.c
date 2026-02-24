#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* Mock sbrk */
static char heap[1024 * 1024]; // 1MB heap
static size_t heap_pos = 0;

void *sbrk(intptr_t increment) {
    if (increment == 0) return &heap[heap_pos];
    if (heap_pos + increment > sizeof(heap)) return (void*)-1;
    void *prev = &heap[heap_pos];
    heap_pos += increment;
    return prev;
}

/* Rename functions */
#define malloc my_malloc
#define free my_free
#define calloc my_calloc
#define realloc my_realloc

#include "../userspace/libc/src/malloc.c"

int main() {
    printf("Testing custom allocation overflows...\n");

    /* Test 1: calloc overflow */
    size_t huge = (size_t)-1;
    size_t half = huge / 2;

    void *ptr = my_calloc(half + 2, 2);
    if (ptr != NULL) {
        printf("FAIL: calloc(half+2, 2) returned %p (expected NULL)\n", ptr);
        return 1;
    }
    printf("PASS: calloc overflow check working.\n");

    /* Test 2: malloc overflow */
    /* huge is SIZE_MAX. huge - 10 should overflow alignment/block size addition */
    ptr = my_malloc(huge - 10);
    if (ptr != NULL) {
         printf("FAIL: malloc(huge-10) returned %p (expected NULL)\n", ptr);
         return 1;
    }
    printf("PASS: malloc overflow check working.\n");

    /* Test 3: realloc overflow */
    void *p = my_malloc(10);
    if (!p) {
        printf("FAIL: malloc(10) failed (basic allocation broken)\n");
        return 1;
    }
    void *p2 = my_realloc(p, huge - 10);
    if (p2 != NULL) {
         printf("FAIL: realloc(p, huge-10) returned %p (expected NULL)\n", p2);
         return 1;
    }
    printf("PASS: realloc overflow check working.\n");

    /* Test 4: Normal allocation */
    p2 = my_realloc(p, 20);
    if (!p2) {
        printf("FAIL: realloc(p, 20) failed (normal realloc broken)\n");
        return 1;
    }
    my_free(p2);
    printf("PASS: Normal allocation working.\n");

    return 0;
}
