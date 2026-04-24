#include <assert.h>
#define __ETEROS_HOST_TEST__ 1

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

/* Use kernel headers */
#include "../include/types.h"
#include "../include/lock.h"
#include "../include/task.h"
#include "../include/sem.h"

/* Global mock state */
#define MAX_TASKS_MOCK 4
task_t tasks[MAX_TASKS_MOCK];
task_t* current_task = &tasks[0];
int task_wakeup_called = 0;
task_t* last_woken_task = NULL;
int interrupts_enabled = 1;

/* Mocks */

task_t* task_get_current(void) {
    return current_task;
}

void schedule(void) {
    /* No-op for testing */
}

void hal_interrupts_disable(void) {
    interrupts_enabled = 0;
}

void hal_interrupts_enable(void) {
    interrupts_enabled = 1;
}

int task_get_max(void) {
    return MAX_TASKS_MOCK;
}

task_t* task_get_at(int index) {
    if (index >= 0 && index < MAX_TASKS_MOCK) {
        return &tasks[index];
    }
    return NULL;
}

void task_wakeup(task_t* t) {
    task_wakeup_called++;
    last_woken_task = t;
    if (t) {
        t->state = TASK_READY;
    }
}

void task_block(void) {
    if (current_task) {
        current_task->state = TASK_BLOCKED;
    }
}

/* Include source under test */
#include "../kernel/sem.c"

/* Tests */
void test_sem_signal_null(void) {
    printf("Test: sem_signal with NULL semaphore...\n");
    task_wakeup_called = 0;
    sem_signal(NULL);
    ASSERT(task_wakeup_called == 0);
    printf("PASS\n");
}

void test_sem_signal_no_waiters(void) {
    printf("Test: sem_signal with no waiting tasks...\n");
    sem_t sem;
    sem_init(&sem, 0);

    task_wakeup_called = 0;

    /* Ensure no tasks are waiting on this sem */
    for (int i = 0; i < MAX_TASKS_MOCK; i++) {
        tasks[i].state = TASK_RUNNING;
        tasks[i].waiting_sem = NULL;
    }

    sem_signal(&sem);

    ASSERT(sem.count == 1);
    ASSERT(task_wakeup_called == 0);
    printf("PASS\n");
}

void test_sem_signal_with_waiters(void) {
    printf("Test: sem_signal with waiting tasks...\n");
    sem_t sem;
    sem_init(&sem, 0);

    task_wakeup_called = 0;
    last_woken_task = NULL;

    /* Set up tasks[1] and tasks[2] to be blocked on 'sem' */
    tasks[1].state = TASK_BLOCKED;
    tasks[1].waiting_sem = (struct semaphore*)&sem;
    tasks[1].id = 1;

    tasks[2].state = TASK_BLOCKED;
    tasks[2].waiting_sem = (struct semaphore*)&sem;
    tasks[2].id = 2;

    /* First signal */
    sem_signal(&sem);

    /* Should increment count */
    ASSERT(sem.count == 1);

    /* Should wake up exactly ONE task */
    ASSERT(task_wakeup_called == 1);
    ASSERT(last_woken_task != NULL);
    ASSERT(last_woken_task->id == 1 || last_woken_task->id == 2);

    /* The woken task should no longer be waiting on 'sem' */
    ASSERT(last_woken_task->waiting_sem == NULL);
    ASSERT(last_woken_task->state == TASK_READY);

    /* The other task should still be waiting */
    task_t* other_task = (last_woken_task->id == 1) ? &tasks[2] : &tasks[1];
    ASSERT(other_task->waiting_sem == (struct semaphore*)&sem);
    ASSERT(other_task->state == TASK_BLOCKED);

    /* Second signal */
    sem_signal(&sem);

    ASSERT(sem.count == 2);
    ASSERT(task_wakeup_called == 2);
    ASSERT(last_woken_task == other_task);
    ASSERT(last_woken_task->waiting_sem == NULL);
    ASSERT(last_woken_task->state == TASK_READY);

    printf("PASS\n");
}

int main(void) {
    printf("Running sem_logic tests...\n");
    test_sem_signal_null();
    test_sem_signal_no_waiters();
    test_sem_signal_with_waiters();
    printf("All sem_logic tests passed!\n");
    return 0;
}
void serial_write_string(const char* s) {}
