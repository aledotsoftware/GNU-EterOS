#define __ETEROS_HOST_TEST__ 1
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#undef assert
#define assert(x) if(!(x)) { printf("Assertion failed: %s\n", #x); exit(1); }

/* Use kernel headers instead of mocking to avoid conflicts */
#include "../include/types.h"
#include "../include/lock.h"
#include "../include/task.h"
#include "../include/string.h"

/* Define timespec because the host <time.h> might conflict or missing fields in our context. */
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
task_t task2;
task_t *current_task = &task1;

void init_tasks() {
    memset(&task1, 0, sizeof(task_t));
    task1.id = 1;
    task1.state = TASK_RUNNING;

    memset(&task2, 0, sizeof(task_t));
    task2.id = 2;
    task2.state = TASK_RUNNING;
}

/* Mock Implementations */

void hal_interrupts_disable(void) {}
void hal_interrupts_enable(void) {}

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
    return 0;
}

void schedule(void) {
    /* No-op in single-threaded test */
}

void task_yield(void) {
}

void task_block_with_timeout(uint64_t wake_tick) {
    (void)wake_tick;
    if (current_task) {
        current_task->state = TASK_BLOCKED;
    }
}

void task_block(void) {
    if (current_task) {
        current_task->state = TASK_BLOCKED;
    }
}

void task_wakeup(task_t* t) {
    if (t) {
        t->state = TASK_READY;
    }
}

/* Constants */
#define EAGAIN 11
#define ENOMEM 12
#define EFAULT 14
#define EINTR 4
#define ENOSYS 38
#define ETIMEDOUT 110

/* Include the source file under test */
#include "../kernel/futex.c"

/* --- Tests --- */

void test_futex_wait_mismatch(void) {
    printf("Test: futex_wait mismatch...\n");
    uint32_t uaddr = 100;
    int ret = futex_wait(&uaddr, 101, NULL, 0, 0xffffffff); /* Expected 101, but is 100 */
    assert(ret == -EAGAIN);
    printf("PASS\n");
}

void test_futex_wait_success(void) {
    printf("Test: futex_wait success...\n");

    uint32_t uaddr = 200;

    int ret = futex_wait(&uaddr, 200, NULL, 0, 0xffffffff);
    assert(ret == -EINTR); /* Because not woken */
    assert(task1.state == TASK_BLOCKED);

    int idx = futex_hash(&uaddr, 0);
    buckets[idx].head = NULL;

    printf("PASS\n");
}

void test_futex_wake_logic(void) {
    printf("Test: futex_wake logic...\n");

    uint32_t uaddr = 300;

    int idx = futex_hash(&uaddr, 0);

    futex_node_t *node = malloc(sizeof(futex_node_t));
    node->task = &task2;
    node->uaddr = &uaddr;
    node->next = NULL;

    buckets[idx].head = node;
    task2.state = TASK_BLOCKED;

    int count = futex_wake(&uaddr, 1, 0, 0xffffffff);

    assert(count == 1);
    assert(task2.state == TASK_READY);
    assert(buckets[idx].head == NULL);

    free(node); /* Waiter frees it */
    printf("PASS\n");
}

int main(void) {
    init_tasks();
    futex_init();

    test_futex_wait_mismatch();
    test_futex_wait_success();
    test_futex_wake_logic();

    printf("All tests passed.\n");
    return 0;
}
