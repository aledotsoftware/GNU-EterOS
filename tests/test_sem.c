#define __ETEROS_HOST_TEST__ 1
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Use kernel headers */
#include "../include/types.h"
#include "../include/sem.h"
#include "../include/task.h"
#include "../include/hal.h"
#include "../include/lock.h"

/* --- Mocks --- */
void hal_interrupts_disable(void) {}
void hal_interrupts_enable(void) {}

/* We won't redefine spin_lock/unlock as they are inline in lock.h */
/* But wait, in lock.h they are inline. Wait, what about __sync_lock_test_and_set? It's a builtin. */

void schedule(void) {}
task_t* task_get_current(void) { return NULL; }
int task_get_max(void) { return 0; }
task_t* task_get_at(int i) { return NULL; }
void task_wakeup(task_t* t) {}
void task_yield(void) {}

/* Include source directly */
#include "../kernel/sem.c"

/* --- Tests --- */
void test_sem_init(void) {
    printf("Test: sem_init...\n");

    sem_t sem;
    sem.count = 999;
    sem.lock = 1;

    /* Test null pointer doesn't crash */
    sem_init(NULL, 5);

    /* Test valid initialization */
    sem_init(&sem, 5);
    assert(sem.count == 5);
    assert(sem.lock == 0);

    /* Test mutex-like initialization */
    sem.count = 999; sem.lock = 1;
    sem_init(&sem, 1);
    assert(sem.count == 1);
    assert(sem.lock == 0);

    /* Test blocking initialization */
    sem.count = 999; sem.lock = 1;
    sem_init(&sem, 0);
    assert(sem.count == 0);
    assert(sem.lock == 0);

    printf("PASS\n");
}

int main(void) {
    test_sem_init();
    printf("All sem tests passed.\n");
    return 0;
}
