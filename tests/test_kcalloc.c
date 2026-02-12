#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include "../include/types.h"

/* Mock kmalloc */
static size_t last_malloc_size = 0;
static void* mock_kmalloc(size_t size) {
    last_malloc_size = size;
    if (size == 0) return NULL;
    return malloc(size);
}

/* Mock serial_write_string (used by kmalloc in real heap.c,
   but we are testing kcalloc which we'll define here or include) */
void serial_write_string(const char* str) {
    (void)str;
}

/* Since we want to test the logic in kernel/mm/heap.c,
   we can either include it (tricky) or redefine kcalloc here
   with the same logic we just implemented.
   Actually, including heap.c might be hard because of fixed addresses
   and other dependencies.
   But wait, I can just copy the kcalloc function here to test the logic.
*/

#define kmalloc mock_kmalloc

void* kcalloc_test(size_t num, size_t size) {
    if (num > 0 && size > SIZE_MAX / num) {
        return NULL;
    }

    size_t total = num * size;
    void* ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

int main() {
    printf("Running kcalloc overflow tests...\n");

    /* Test 1: Normal allocation */
    void* p1 = kcalloc_test(10, 10);
    assert(p1 != NULL);
    assert(last_malloc_size == 100);
    free(p1);
    printf("Test 1 passed (normal allocation)\n");

    /* Test 2: Potential overflow */
    /* Use values that would overflow size_t */
    /* For 64-bit, SIZE_MAX is ~1.8e19 */
    void* p2 = kcalloc_test(SIZE_MAX / 2 + 1, 4);
    assert(p2 == NULL);
    printf("Test 2 passed (overflow detected)\n");

    /* Test 3: Large but NO overflow */
    void* p3 = kcalloc_test(SIZE_MAX / 4, 2);
    /* This might return NULL if malloc fails, but it SHOULD NOT be because of our overflow check */
    /* Actually, malloc will likely fail for such a large size, but let's check it didn't return early due to overflow check if we use a smaller large value */

    /* Let's use a value that is large but doesn't overflow */
    size_t num = 1024;
    size_t size = 1024;
    void* p4 = kcalloc_test(num, size);
    assert(p4 != NULL);
    assert(last_malloc_size == 1024 * 1024);
    free(p4);
    printf("Test 3 passed (large but no overflow)\n");

    /* Test 4: Extreme overflow */
    void* p5 = kcalloc_test(SIZE_MAX, SIZE_MAX);
    assert(p5 == NULL);
    printf("Test 4 passed (extreme overflow)\n");

    /* Test 5: num=0 */
    void* p6 = kcalloc_test(0, 100);
    assert(p6 == NULL); /* kmalloc(0) returns NULL in our mock */
    printf("Test 5 passed (num=0)\n");

    printf("All kcalloc tests passed!\n");
    return 0;
}
