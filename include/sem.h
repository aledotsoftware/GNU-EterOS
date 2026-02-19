#ifndef ETEROS_SEM_H
#define ETEROS_SEM_H

#include <types.h>
#include <lock.h>

typedef struct semaphore {
    volatile int count;
    spinlock_t lock;
} sem_t;

/**
 * Initializes a semaphore.
 * @param sem Pointer to semaphore.
 * @param value Initial value (0 for blocking, 1 for mutex-like).
 */
void sem_init(sem_t* sem, int value);

/**
 * Waits on a semaphore (P operation).
 * Decrements the count. If count < 0, blocks the current task.
 */
void sem_wait(sem_t* sem);

/**
 * Signals a semaphore (V operation).
 * Increments the count. Wakes up a blocked task if any.
 */
void sem_signal(sem_t* sem);

#endif /* ETEROS_SEM_H */
