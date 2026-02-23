#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* --- Mocks --- */

/* Types needed by futex.c */
typedef int spinlock_t;

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
} task_t;

/* Global state for test */
task_t task1 = {1, TASK_RUNNING, 0};
task_t task2 = {2, TASK_RUNNING, 0};
task_t *current_task = &task1;

/* Mock Implementations */

void spin_lock(spinlock_t *lock) {
    *lock = 1;
}

void spin_unlock(spinlock_t *lock) {
    *lock = 0;
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

uint64_t timer_get_ticks(void) {
    return 0;
}

#define TIMER_HZ 1000

void task_yield(void) {
    /* No-op in single-threaded test */
}

/* Constants */
#define EAGAIN 11
#define ENOMEM 12
#define EFAULT 14
#define EINTR 4
#define ENOSYS 38
#define ETIMEDOUT 110

/* Include the source file under test */
/* We rely on -Itests/mocks to provide empty headers for the includes in futex.c */
#include "../kernel/futex.c"

/* --- Tests --- */

void test_futex_wait_mismatch(void) {
    printf("Test: futex_wait mismatch...\n");
    uint32_t uaddr = 100;
    int ret = futex_wait(&uaddr, 101, NULL); /* Expected 101, but is 100 */
    assert(ret == -EAGAIN);
    printf("PASS\n");
}

void test_futex_wait_success(void) {
    printf("Test: futex_wait success...\n");

    uint32_t uaddr = 200;

    /* Simulate wait. Since task_yield returns immediately,
       we check if we are still in queue (EINTR). */

    int ret = futex_wait(&uaddr, 200, NULL);
    assert(ret == -EINTR); /* Because not woken */
    assert(task1.state == TASK_BLOCKED);

    /* Clean up: manually remove from bucket to avoid mem leak in test */
    int idx = futex_hash(&uaddr);
    buckets[idx].head = NULL;

    printf("PASS\n");
}

void test_futex_wake_logic(void) {
    printf("Test: futex_wake logic...\n");

    uint32_t uaddr = 300;

    /* Manually insert node */
    int idx = futex_hash(&uaddr);

    futex_node_t *node = malloc(sizeof(futex_node_t));
    node->task = &task2;
    node->uaddr = &uaddr;
    node->next = NULL;

    buckets[idx].head = node;
    task2.state = TASK_BLOCKED;

    /* Wake */
    int count = futex_wake(&uaddr, 1);

    assert(count == 1);
    assert(task2.state == TASK_READY);
    assert(buckets[idx].head == NULL);

    free(node); /* In real code, waiter frees it. Here we do it. */
    printf("PASS\n");
}

int main(void) {
    futex_init();

    test_futex_wait_mismatch();
    test_futex_wait_success();
    test_futex_wake_logic();

    printf("All tests passed.\n");
    return 0;
}
