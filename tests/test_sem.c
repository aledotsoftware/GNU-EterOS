#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* --- Mocks --- */

typedef int spinlock_t;

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

struct semaphore;

typedef struct task {
    uint32_t id;
    task_state_t state;
    uint64_t wake_tick;
    struct semaphore* waiting_sem;
} task_t;

typedef struct semaphore {
    int count;
    spinlock_t lock;
} sem_t;

/* Global state for test */
task_t task1 = {1, TASK_RUNNING, 0, NULL};
task_t task2 = {2, TASK_READY, 0, NULL};
task_t task3 = {3, TASK_READY, 0, NULL};

task_t *current_task = &task1;

task_t* tasks[] = { &task1, &task2, &task3 };

/* Mock Implementations */

void spin_lock(spinlock_t *lock) {
    *lock = 1;
}

void spin_unlock(spinlock_t *lock) {
    *lock = 0;
}

task_t *task_get_current(void) {
    return current_task;
}

int task_get_max(void) {
    return 3;
}

task_t* task_get_at(int i) {
    if (i >= 0 && i < 3) return tasks[i];
    return NULL;
}

void hal_interrupts_disable(void) {
}

void hal_interrupts_enable(void) {
}

uint64_t task_irq_save(void) { return 0x200; }
void task_irq_restore(uint64_t f) { (void)f; }

void task_wakeup(task_t* t) {
    if (t->state == TASK_BLOCKED) {
        t->state = TASK_READY;
    }
}

void task_block(void) {
    if (current_task) {
        current_task->state = TASK_BLOCKED;
    }
}

int schedule_call_count = 0;
sem_t* current_sem = NULL;

void schedule(void) {
    schedule_call_count++;
    /* Simulate being woken up by another task signaling the semaphore */
    if (current_sem && schedule_call_count == 1) {
        /* On first schedule, let's simulate that another task signals the sem */
        current_sem->count++;
        current_task->state = TASK_READY;
    } else if (schedule_call_count >= 10) {
        /* Avoid infinite loops if something goes wrong */
        assert(0 && "Infinite loop in sem_wait");
    }
}

/* Include the source file under test */
#include "../kernel/sem.c"

/* --- Tests --- */

void test_sem_init(void) {
    printf("Test: sem_init...\n");
    sem_t sem;
    sem_init(&sem, 5);
    assert(sem.count == 5);
    assert(sem.lock == 0);
    printf("PASS\n");
}

void test_sem_wait_immediate(void) {
    printf("Test: sem_wait immediate...\n");
    sem_t sem;
    sem_init(&sem, 1);

    current_task = &task1;
    task1.state = TASK_RUNNING;
    schedule_call_count = 0;

    sem_wait(&sem);

    assert(sem.count == 0);
    assert(schedule_call_count == 0);
    printf("PASS\n");
}

void test_sem_wait_blocks(void) {
    printf("Test: sem_wait blocks...\n");
    sem_t sem;
    sem_init(&sem, 0);

    current_task = &task1;
    task1.state = TASK_RUNNING;
    schedule_call_count = 0;
    current_sem = &sem;

    /* This will call schedule(), and our mock schedule will increment sem.count to break the loop */
    sem_wait(&sem);

    assert(sem.count == 0); /* count was 0, our mock schedule incrs to 1, then sem_wait decrs to 0 */
    assert(schedule_call_count == 1);
    assert(task1.waiting_sem == (struct semaphore*)&sem); /* Actually, waiting_sem should remain set since current implementation doesn't clear it in wait */
    printf("PASS\n");
}

void test_sem_signal(void) {
    printf("Test: sem_signal...\n");
    sem_t sem;
    sem_init(&sem, 0);

    task2.state = TASK_BLOCKED;
    task2.waiting_sem = (struct semaphore*)&sem;
    task3.state = TASK_BLOCKED;
    task3.waiting_sem = (struct semaphore*)&sem;

    sem_signal(&sem);

    assert(sem.count == 1);
    assert(task2.state == TASK_READY);
    assert(task2.waiting_sem == NULL);
    /* task3 should still be blocked since we only wake ONE task */
    assert(task3.state == TASK_BLOCKED);
    assert(task3.waiting_sem == (struct semaphore*)&sem);

    printf("PASS\n");
}

int main(void) {
    test_sem_init();
    test_sem_wait_immediate();
    test_sem_wait_blocks();
    test_sem_signal();

    printf("All tests passed.\n");
    return 0;
}
