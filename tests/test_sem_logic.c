#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <setjmp.h>

#define __ETEROS_HOST_TEST__

/* Mock definitions */
#include "mocks/task.h"
#include "mocks/hal.h"
#include "mocks/sem.h"

/* State for tests */
task_t main_task = {1, TASK_RUNNING, NULL};
task_t waiter_task = {2, TASK_BLOCKED, NULL};
task_t* current_task = &main_task;

static int interrupt_state = 1;
static int schedule_called = 0;
static int yield_loop_break = 0;
jmp_buf schedule_jmp;

/* Mock implementations */
task_t* task_get_current(void) {
    return current_task;
}

int task_get_max(void) {
    return 2;
}

task_t* task_get_at(int i) {
    if (i == 0) return &main_task;
    if (i == 1) return &waiter_task;
    return NULL;
}

void task_wakeup(task_t* t) {
    t->state = TASK_READY;
}

void hal_interrupts_disable(void) {
    interrupt_state = 0;
}

void hal_interrupts_enable(void) {
    interrupt_state = 1;
}

void spin_lock(spinlock_t* lock) {
    *lock = 1;
}

void spin_unlock(spinlock_t* lock) {
    *lock = 0;
}

/* Include source under test */
#include "../kernel/sem.c"

/* Mock schedule - this is tricky because sem_wait is a while(1) loop */
void schedule(void) {
    schedule_called++;
    interrupt_state = 1; // Scheduler re-enables interrupts

    if (yield_loop_break) {
        /* Jump back to test to avoid infinite loop */
        longjmp(schedule_jmp, 1);
    }
}

void test_sem_init() {
    printf("Test: sem_init...\n");
    sem_t sem;
    sem_init(&sem, 5);
    assert(sem.count == 5);
    assert(sem.lock == 0);
    printf("PASS\n");
}

void test_sem_wait_no_block() {
    printf("Test: sem_wait without blocking...\n");
    sem_t sem;
    sem_init(&sem, 1);

    sem_wait(&sem);

    assert(sem.count == 0);
    assert(interrupt_state == 1);
    assert(current_task->state == TASK_RUNNING);
    assert(current_task->waiting_sem == NULL);
    printf("PASS\n");
}

void test_sem_wait_block() {
    printf("Test: sem_wait with blocking...\n");
    sem_t sem;
    sem_init(&sem, 0);

    schedule_called = 0;
    yield_loop_break = 1;

    if (setjmp(schedule_jmp) == 0) {
        sem_wait(&sem);
        /* Should not reach here */
        assert(0);
    }

    assert(sem.count == 0);
    assert(current_task->state == TASK_BLOCKED);
    assert(current_task->waiting_sem == &sem);
    assert(schedule_called == 1);

    /* Reset state */
    current_task->state = TASK_RUNNING;
    current_task->waiting_sem = NULL;
    yield_loop_break = 0;

    printf("PASS\n");
}

void test_sem_signal_no_waiters() {
    printf("Test: sem_signal no waiters...\n");
    sem_t sem;
    sem_init(&sem, 0);

    sem_signal(&sem);

    assert(sem.count == 1);
    printf("PASS\n");
}

void test_sem_signal_with_waiter() {
    printf("Test: sem_signal with waiter...\n");
    sem_t sem;
    sem_init(&sem, 0);

    /* Set up waiter task */
    waiter_task.state = TASK_BLOCKED;
    waiter_task.waiting_sem = &sem;

    sem_signal(&sem);

    assert(sem.count == 1);
    assert(waiter_task.state == TASK_READY);
    assert(waiter_task.waiting_sem == NULL);
    printf("PASS\n");
}

int main() {
    test_sem_init();
    test_sem_wait_no_block();
    test_sem_wait_block();
    test_sem_signal_no_waiters();
    test_sem_signal_with_waiter();

    printf("All tests passed.\n");
    return 0;
}
