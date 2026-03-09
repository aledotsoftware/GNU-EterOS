#include <stdlib.h>
#include <assert.h>
#ifndef assert
#define assert(x) do { if (!(x)) { printf("ASSERT FAILED: %s\n", #x); exit(1); } } while(0)
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "../include/errno.h"

/* --- Mocks for Kernel Environment --- */

/* Definitions from types.h / task.h */
typedef int spinlock_t;

/* We rely on include/time.h for struct timespec */
#include <time.h>

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

typedef struct task {
    uint32_t id;
    task_state_t state;
    uint64_t wake_tick;
    uint32_t *uaddr; /* used in futex_node but node points to task */
} task_t;

/* Global state for test */
task_t task1 = {1, TASK_RUNNING, 0, NULL};
task_t *current_task = &task1;

uint64_t current_ticks = 1000;
uint64_t ticks_increment_on_yield = 0;

/* Mock Implementations */

void spin_lock(spinlock_t *lock) {
    (void)lock;
}

void spin_unlock(spinlock_t *lock) {
    (void)lock;
}

void *kmalloc(size_t size) {
    return malloc(size);
}

void kfree(void *ptr) {
    free(ptr);
}

task_t *task_get_current(void) {
    return current_task;
}

/* timer.h mocks */
uint64_t timer_get_ticks(void) {
    return current_ticks;
}

/* We don't define TIMER_HZ here because it's in timer.h which is included by futex.c */
/* But we can't rely on it being visible before including futex.c */
#define TEST_TIMER_HZ 1000

void task_yield(void) {
    /* Simulate time passing */
    if (ticks_increment_on_yield > 0) {
        current_ticks += ticks_increment_on_yield;
    }
}

/* Include the source file under test */
/* We define __ETEROS_HOST_TEST__ to handle includes */
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

    int ret = futex_wait(&uaddr, 200, &ts);

    assert(ret == -ETIMEDOUT);
    /* wake_tick should have been set, but might be cleared by schedule (which we mock implicitly via task_yield doing nothing but incrementing time) */
    /* In real kernel, schedule clears wake_tick. Here task_yield just increments time. So wake_tick remains set. */
    assert(task1.wake_tick == 1500);

    /* Clean up manually */
    int idx = futex_hash(&uaddr);
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

    int ret = futex_wait(&uaddr, 300, NULL);

    assert(ret == -EINTR);
    assert(task1.wake_tick == 0);

    int idx = futex_hash(&uaddr);
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

    int ret = futex_wait(&uaddr, 400, &ts);

    assert(ret == -EINTR);
    assert(task1.wake_tick == 4000);

    int idx = futex_hash(&uaddr);
    buckets[idx].head = NULL;

    printf("PASS\n");
}

int main(void) {
    futex_init();

    test_futex_timeout();
    test_futex_no_timeout();
    test_futex_spurious_wake();

    printf("All tests passed.\n");
    return 0;
}
