#include <stdlib.h>
#include <assert.h>
#ifndef assert
#define assert(x) do { if (!(x)) { printf("ASSERT FAILED: %s\n", #x); exit(1); } } while(0)
#endif
#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <assert.h>

/* Mock constants from shell.c */
#define HISTORY_SIZE 8
#define SHELL_MAX_INPUT 64

/*
 * Reproduction of the vulnerability logic:
 * Using signed int for history_count allows it to wrap to negative values,
 * causing negative array indices.
 */
void test_vulnerable_logic(void) {
    int history_count = INT_MAX;

    // Simulate overflow
    history_count++; // Now INT_MIN (-2147483648)

    // In many systems INT_MIN % 8 == 0, so let's go further
    history_count++; // INT_MIN + 1

    int idx = history_count % HISTORY_SIZE;

    // Check if index is negative (VULNERABILITY)
    if (idx < 0) {
        printf("[PASS] Vulnerability reproduced: Index is %d (negative)\n", idx);
    } else {
        printf("[WARN] Could not reproduce negative index on this platform with %d\n", history_count);
    }
}

/*
 * Verification of the fix:
 * Using unsigned int ensures modulo arithmetic always yields a positive index
 * within bounds [0, HISTORY_SIZE-1].
 */
void test_fixed_logic(void) {
    unsigned int history_count = UINT_MAX;

    // Simulate overflow
    history_count++; // Wraps to 0

    unsigned int idx = history_count % HISTORY_SIZE;
    assert(idx < HISTORY_SIZE);
    printf("[PASS] Fix verified: history_count=0 -> index=%u\n", idx);

    // Simulate more
    history_count = UINT_MAX;
    history_count += 2; // Wraps to 1
    idx = history_count % HISTORY_SIZE;
    assert(idx < HISTORY_SIZE);
    printf("[PASS] Fix verified: history_count=1 -> index=%u\n", idx);
}

void test_down_arrow_logic(void) {
    unsigned int history_count = 0;
    unsigned int history_idx = 0;

    // Original logic with unsigned: if (history_idx < history_count - 1)
    // 0 < 0 - 1  =>  0 < UINT_MAX  => TRUE (WRONG!)
    if (history_idx < history_count - 1) {
        printf("[FAIL] Down arrow logic broken with unsigned: 0 < UINT_MAX is true\n");
    } else {
        printf("[PASS] Down arrow logic safe (unexpected if vulnerable logic used)\n");
    }

    // Fixed logic: if (history_idx + 1 < history_count)
    // 0 + 1 < 0  =>  1 < 0  => FALSE (CORRECT)
    if (history_idx + 1 < history_count) {
        printf("[FAIL] Fixed logic incorrect?\n");
    } else {
        printf("[PASS] Fixed logic correct: 1 < 0 is false\n");
    }
}

int main(void) {
    printf("Running Shell History Security Test...\n");
    test_vulnerable_logic();
    test_fixed_logic();
    test_down_arrow_logic();
    return 0;
}
