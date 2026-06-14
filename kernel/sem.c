#include <sem.h>
#include <task.h>
#include <hal.h>
#include <lock.h>

void sem_init(sem_t* sem, int value) {
    if (!sem) return;
    sem->count = value;
    sem->lock = 0; /* Init spinlock */
}

void sem_wait(sem_t* sem) {
    if (!sem) return;

    task_t* current = task_get_current();
    if (!current) return; /* Should not happen if scheduling is active */

    while (1) {
        /* Disable interrupts to protect task state and schedule */
        uint64_t irq_flags = task_irq_save();

        spin_lock(&sem->lock);

        if (sem->count > 0) {
            sem->count--;
            spin_unlock(&sem->lock);
            task_irq_restore(irq_flags);
            return;
        }

        /* Block current task */
        current->waiting_sem = (struct semaphore*)sem;
        task_block();

        spin_unlock(&sem->lock);
        task_irq_restore(irq_flags);

        /* Yield CPU */
        schedule();

        /* When we wake up, interrupts are enabled. Loop again to try acquire. */
    }
}

void sem_signal(sem_t* sem) {
    if (!sem) return;

    /* Save RFLAGS to restore interrupt state later */
    uint64_t rflags = task_irq_save();

    spin_lock(&sem->lock);

    sem->count++;

    /* Wake up ONE waiting task */
    int max_tasks = task_get_max();
    for (int i = 0; i < max_tasks; i++) {
        task_t* t = task_get_at(i);
        /* Check if task exists, is blocked, and waiting on THIS sem */
        if (t && t->state == TASK_BLOCKED && t->waiting_sem == (struct semaphore*)sem) {
            t->waiting_sem = 0;
            task_wakeup(t);
            break; /* Wake up only one task per signal */
        }
    }

    spin_unlock(&sem->lock);

    /* Restore interrupts if they were enabled */
    if (rflags & 0x200) { /* IF bit is bit 9 (0x200) */
        hal_interrupts_enable();
    }
}
