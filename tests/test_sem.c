#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

/* --- Mocks --- */

typedef int spinlock_t;

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

/* Forward declaration for sem_t since it's used in task_t */
struct semaphore;

typedef struct task {
    uint32_t id;
    task_state_t state;
    struct semaphore* waiting_sem;
} task_t;

typedef struct semaphore {
    volatile int count;
    spinlock_t lock;
} sem_t;

/* Global state for test */
task_t task_main = {1, TASK_RUNNING, NULL};
task_t task_blocked = {2, TASK_BLOCKED, NULL}; /* Used for signal test */
task_t* current_task = &task_main;

/* Helper to simulate task list */
#define MAX_TASKS 2
task_t* tasks[MAX_TASKS] = {&task_main, &task_blocked};

/* Mock Implementations */

void spin_lock(spinlock_t *lock) {
    *lock = 1;
}

void spin_unlock(spinlock_t *lock) {
    *lock = 0;
}

void hal_interrupts_disable(void) {
    /* No-op for test */
}

void hal_interrupts_enable(void) {
    /* No-op for test */
}

task_t* task_get_current(void) {
    return current_task;
}

int task_get_max(void) {
    return MAX_TASKS;
}

task_t* task_get_at(int i) {
    if (i >= 0 && i < MAX_TASKS) return tasks[i];
    return NULL;
}

/* Schedule Mock logic */
int schedule_called = 0;
void schedule(void) {
    schedule_called++;

    /* Verify we are blocked */
    assert(current_task->state == TASK_BLOCKED);
    assert(current_task->waiting_sem != NULL);

    /* Simulate wake up or signal */
    /* If waiting on a semaphore, increment its count so we can return */
    if (current_task->waiting_sem) {
        sem_t* s = (sem_t*)current_task->waiting_sem;
        s->count++; /* Simulate another task signaling */
    }

    /* Set state back to READY/RUNNING so loop can continue */
    current_task->state = TASK_RUNNING;
    /* We don't clear waiting_sem here typically, the waker does, or wait logic does.
       In sem.c logic: waker clears it. But wait logic sets it.
       If we simulate signal by just incrementing count, we pretend waker did it.
       So we should probably clear waiting_sem too to mimic sem_signal behavior. */
    current_task->waiting_sem = NULL;
}

/* Include source under test */
/* We rely on -Itests/mocks to provide empty headers */
#include "../kernel/sem.c"

/* --- Tests --- */

void test_sem_init(void) {
    printf("Test: sem_init...\n");
    sem_t s;
    sem_init(&s, 5);
    assert(s.count == 5);
    assert(s.lock == 0);
    printf("PASS\n");
}

void test_sem_wait_no_block(void) {
    printf("Test: sem_wait (no block)...\n");
    sem_t s;
    sem_init(&s, 1);

    sem_wait(&s);
    assert(s.count == 0);

    /* Reset */
    sem_init(&s, 2);
    sem_wait(&s);
    assert(s.count == 1);
    sem_wait(&s);
    assert(s.count == 0);

    printf("PASS\n");
}

void test_sem_wait_block(void) {
    printf("Test: sem_wait (block)...\n");
    sem_t s;
    sem_init(&s, 0);

    schedule_called = 0;
    current_task = &task_main;
    task_main.state = TASK_RUNNING;

    sem_wait(&s);

    assert(schedule_called == 1);
    assert(s.count == 0); /* Because schedule incremented it to 1, then sem_wait decremented it to 0 */

    printf("PASS\n");
}

void test_sem_signal(void) {
    printf("Test: sem_signal...\n");
    sem_t s;
    sem_init(&s, 0);

    /* Setup a blocked task */
    task_blocked.state = TASK_BLOCKED;
    task_blocked.waiting_sem = (struct semaphore*)&s;

    sem_signal(&s);

    /* sem_signal increments count FIRST */
    assert(s.count == 1);

    /* Verify task was woken up */
    assert(task_blocked.state == TASK_READY);
    assert(task_blocked.waiting_sem == 0);

    printf("PASS\n");
}

void test_sem_signal_no_waiters(void) {
    printf("Test: sem_signal (no waiters)...\n");
    sem_t s;
    sem_init(&s, 0);

    /* Ensure no task is waiting on this sem */
    task_main.waiting_sem = NULL;
    task_blocked.waiting_sem = NULL;

    sem_signal(&s);

    assert(s.count == 1);

    sem_signal(&s);
    assert(s.count == 2);

    printf("PASS\n");
}

int main(void) {
    test_sem_init();
    test_sem_wait_no_block();
    test_sem_wait_block();
    test_sem_signal();
    test_sem_signal_no_waiters();

    printf("All semaphore tests passed.\n");
    return 0;
}
