#define __ETEROS_HOST_TEST__ 1
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#undef assert
#define assert(x) if(!(x)) { printf("Assertion failed: %s\n", #x); exit(1); }

/* --- Mocks for Kernel Environment --- */

/* Definitions from types.h / task.h */
#include "../include/types.h"
#include "../include/lock.h"
#include "../include/task.h"
#include "../include/string.h"
#include "../include/errno.h"

/* Define timespec */
#define TIMER_HZ 1000
#ifndef _TIMESPEC_DEFINED
#define _TIMESPEC_DEFINED
struct timespec {
    long tv_sec;
    long tv_nsec;
};
#endif

/* Global state for test */
task_t task1;
task_t *current_task = &task1;

uint64_t current_ticks = 1000;
uint64_t ticks_increment_on_yield = 0;

void init_tasks() {
    memset(&task1, 0, sizeof(task_t));
    task1.id = 1;
    task1.state = TASK_RUNNING;
}

/* Mock Implementations */

void *kmalloc(size_t size) {
    return malloc(size);
}

void kfree(void *ptr) {
    free(ptr);
}

task_t *task_get_current(void) {
    return current_task;
}

uint64_t timer_get_ticks(void) {
    return current_ticks;
}

void task_yield(void) {
    /* Simulate time passing */
    if (ticks_increment_on_yield > 0) {
        current_ticks += ticks_increment_on_yield;
    }
}

void task_block_with_timeout(uint64_t wake_tick) {
    if (current_task) {
        current_task->wake_tick = wake_tick;
        current_task->state = TASK_BLOCKED;
    }
}

void task_block(void) {
    if (current_task) {
        current_task->wake_tick = 0;
        current_task->state = TASK_BLOCKED;
    }
}

void task_wakeup(task_t* t) {
    if (t) {
        t->state = TASK_READY;
    }
}

/* Include the source file under test */
#include "../kernel/futex.c"

/* --- Tests --- */

void test_futex_timeout(void) {
    printf("Test: futex_wait with timeout...\n");
    uint32_t uaddr = 200;

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 500 * 1000000; /* 500ms */

    current_ticks = 1000;
    ticks_increment_on_yield = 600; /* 600ms passed */

    /* Expected wake_tick = 1000 + 500 = 1500 */
    /* Actual ticks after yield = 1000 + 600 = 1600 */
    /* 1600 >= 1500 -> Timeout */

    int ret = futex_wait(&uaddr, 200, &ts, 0, FUTEX_BITSET_MATCH_ANY);

    assert(ret == -ETIMEDOUT);
    assert(task1.wake_tick == 1500);

    /* Clean up manually */
    int idx = futex_hash(&uaddr, 0);
    buckets[idx].head = NULL;

    printf("PASS\n");
}

void test_futex_no_timeout(void) {
    printf("Test: futex_wait without timeout...\n");
    uint32_t uaddr = 300;

    current_ticks = 2000;
    ticks_increment_on_yield = 100;

    /* No timeout provided */
    /* Should return EINTR (spurious wakeup) because we simulate yield return without explicit wake */

    int ret = futex_wait(&uaddr, 300, NULL, 0, FUTEX_BITSET_MATCH_ANY);

    assert(ret == -EINTR);
    assert(task1.wake_tick == 0);

    int idx = futex_hash(&uaddr, 0);
    buckets[idx].head = NULL;

    printf("PASS\n");
}

void test_futex_spurious_wake(void) {
    printf("Test: futex_wait spurious wakeup before timeout...\n");
    uint32_t uaddr = 400;

    struct timespec ts;
    ts.tv_sec = 1; /* 1000ms */
    ts.tv_nsec = 0;

    current_ticks = 3000;
    ticks_increment_on_yield = 500; /* Only 500ms passed */

    /* Expected wake_tick = 3000 + 1000 = 4000 */
    /* Actual ticks after yield = 3500 */
    /* 3500 < 4000 -> Not timed out */

    int ret = futex_wait(&uaddr, 400, &ts, 0, FUTEX_BITSET_MATCH_ANY);

    assert(ret == -EINTR);
    assert(task1.wake_tick == 4000);

    int idx = futex_hash(&uaddr, 0);
    buckets[idx].head = NULL;

    printf("PASS\n");
}

int main(void) {
    init_tasks();
    futex_init();

    test_futex_timeout();
    test_futex_no_timeout();
    test_futex_spurious_wake();

    printf("All tests passed.\n");
    return 0;
}
