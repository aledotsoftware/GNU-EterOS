#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define __ETEROS_HOST_TEST__ 1

#undef assert
#define assert(x) if (!(x)) { printf("ASSERTION FAILED: %s\n", #x); exit(1); }
#define eteros_memset memset

/* Types from types.h / task.h */
#include "../include/types.h"
#include "../include/task.h"

#undef spin_lock
#undef spin_unlock
#define spin_lock(x)
#define spin_unlock(x)

#include "../include/errno.h"

struct timespec {
    time_t tv_sec;  /* Segundos */
    long   tv_nsec; /* Nanosegundos [0, 999999999] */
};

task_t task1;
task_t task2;
task_t *current_task = &task1;

uint64_t current_ticks = 1000;
uint64_t ticks_increment_on_yield = 0;

void *kmalloc(size_t size) { return malloc(size); }
void kfree(void *ptr) { free(ptr); }
task_t *task_get_current(void) { return current_task; }
uint64_t timer_get_ticks(void) { return current_ticks; }
#define TIMER_HZ 1000
void task_yield(void) {
    if (ticks_increment_on_yield > 0) current_ticks += ticks_increment_on_yield;
}
void task_block_with_timeout(uint64_t target_tick) { task1.wake_tick = target_tick; task1.state = TASK_SLEEPING; }
void task_wakeup(task_t* t) { t->state = TASK_READY; }

/* Include futex implementation */
#include "../kernel/futex.c"

void test_futex_wait_mismatch(void) {
    printf("Test: futex_wait mismatch...\n");
    uint32_t uaddr = 100;
    int ret = futex_wait(&uaddr, 101, NULL, 0);
    assert(ret == -EAGAIN);
    printf("PASS\n");
}

void test_futex_wait_success_no_timeout(void) {
    printf("Test: futex_wait success (no timeout)...\n");
    uint32_t uaddr = 200;
    current_ticks = 1000;
    ticks_increment_on_yield = 0;
    int ret = futex_wait(&uaddr, 200, NULL, 0);
    assert(ret == -EINTR); /* We just yielded and returned */
    assert(task1.state == TASK_BLOCKED);
    int idx = futex_hash(&uaddr, 0);
    buckets[idx].head = NULL; /* Cleanup */
    printf("PASS\n");
}

void test_futex_wait_timeout(void) {
    printf("Test: futex_wait timeout...\n");
    uint32_t uaddr = 300;
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 500 * 1000000; /* 500ms */
    current_ticks = 1000;
    ticks_increment_on_yield = 600; /* Simulated 600ms pass on yield */
    int ret = futex_wait(&uaddr, 300, &ts, 0);
    assert(ret == -ETIMEDOUT);
    assert(task1.state == TASK_BLOCKED); /* We blocked and then timeout checked out */
    int idx = futex_hash(&uaddr, 0);
    buckets[idx].head = NULL;
    printf("PASS\n");
}

void test_futex_wake_test(void) {
    printf("Test: futex_wake logic...\n");
    uint32_t uaddr = 400;
    int idx = futex_hash(&uaddr, 0);

    futex_node_t *node = malloc(sizeof(futex_node_t));
    node->task = &task2;
    node->uaddr = &uaddr;
    node->next = NULL;
    buckets[idx].head = node;
    task2.state = TASK_BLOCKED;

    int count = futex_wake(&uaddr, 1, 0);
    assert(count == 1);
    assert(task2.state == TASK_READY);
    assert(buckets[idx].head == NULL);

    free(node); /* Simulating what waiter does */
    printf("PASS\n");
}

int main(void) {
    memset(&task1, 0, sizeof(task_t));
    memset(&task2, 0, sizeof(task_t));

    task1.id = 1; task1.state = TASK_RUNNING; task1.wake_tick = 0; task1.cr3 = 0;
    task2.id = 2; task2.state = TASK_RUNNING; task2.wake_tick = 0; task2.cr3 = 0;

    futex_init();
    test_futex_wait_mismatch();
    test_futex_wait_success_no_timeout();
    test_futex_wait_timeout();
    test_futex_wake_test();
    printf("All futex tests passed.\n");
    return 0;
}
