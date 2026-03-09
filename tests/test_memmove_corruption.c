#include <stdlib.h>
#include <assert.h>
#ifndef assert
#define assert(x) do { if (!(x)) { printf("ASSERT FAILED: %s\n", #x); exit(1); } } while(0)
#endif
#define __ETEROS_HOST_TEST__
#define __x86_64__

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

/* Include the source file directly to test internal implementation */
#include "../kernel/string.c"

int main() {
    printf("Testing memmove backward copy implementation...\n");

    /* Buffer size large enough to trigger qwords copy */
    #define BUF_SIZE 64
    #define GUARD_SIZE 16

    /* Allocation with guard zones */
    uint8_t memory[BUF_SIZE + 2 * GUARD_SIZE];
    uint8_t *guard_before = memory;
    uint8_t *buffer = memory + GUARD_SIZE;
    uint8_t *guard_after = memory + GUARD_SIZE + BUF_SIZE;

    /* Initialize guards with a pattern */
    memset(guard_before, 0xAA, GUARD_SIZE);
    memset(guard_after, 0xBB, GUARD_SIZE);

    /* Initialize buffer with known data */
    for (int i = 0; i < BUF_SIZE; i++) {
        buffer[i] = (uint8_t)i;
    }

    /*
     * Test case: Backward copy (dest > src)
     * Move data from buffer[0] to buffer[8] (shift right by 8)
     * Size = 16 bytes.
     * This means we copy buffer[0..15] to buffer[8..23].
     * qwords = 16 / 8 = 2.
     * remainder = 0.
     */

    size_t n = 16;
    size_t shift = 8;

    /* Verify guards before operation */
    for (int i = 0; i < GUARD_SIZE; i++) {
        assert(guard_before[i] == 0xAA);
    }

    printf("Running eteros_memmove(buffer + %lu, buffer, %lu)...\n", shift, n);
    eteros_memmove(buffer + shift, buffer, n);

    /* Check guards */
    int corrupted = 0;
    for (int i = 0; i < GUARD_SIZE; i++) {
        if (guard_before[i] != 0xAA) {
            printf("Guard before corrupted at index %d: expected 0xAA, got 0x%02X\n", i, guard_before[i]);
            corrupted = 1;
        }
    }

    if (corrupted) {
        printf("TEST FAILED: Memory corruption detected!\n");
        return 1;
    }

    printf("TEST PASSED: No corruption detected.\n");

    /* Verify data correctness */
    for (int i = 0; i < n; i++) {
        if (buffer[shift + i] != (uint8_t)i) {
             printf("Data mismatch at index %d: expected %d, got %d\n", i, i, buffer[shift + i]);
             return 1;
        }
    }

    return 0;
}
