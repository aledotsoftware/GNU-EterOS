
#ifndef _SEM_H
#define _SEM_H
#include "lock.h"
typedef struct semaphore {
    int count;
    spinlock_t lock;
} sem_t;
void sem_init(sem_t* sem, int value);
void sem_wait(sem_t* sem);
void sem_signal(sem_t* sem);
#endif
