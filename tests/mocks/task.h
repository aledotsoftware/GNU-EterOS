
#ifndef _TASK_H
#define _TASK_H
#include <stdint.h>
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
    struct semaphore* waiting_sem;
} task_t;
task_t* task_get_current(void);
void schedule(void);
int task_get_max(void);
task_t* task_get_at(int i);
void task_wakeup(task_t* t);
#endif
