#include <input/event.h>
#include <sem.h>
#include <lock.h>
#include <hal.h>
#include <string.h>

#define INPUT_QUEUE_SIZE 1024

static input_event_t input_queue[INPUT_QUEUE_SIZE];
static uint32_t head = 0;
static uint32_t tail = 0;
static sem_t input_sem;
static spinlock_t input_lock = 0;

static inline uint64_t save_irq(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0" : "=r"(flags));
    hal_interrupts_disable();
    return flags;
}

static inline void restore_irq(uint64_t flags) {
    __asm__ volatile("push %0; popfq" :: "r"(flags));
}

void input_init(void) {
    sem_init(&input_sem, 0);
    head = 0;
    tail = 0;
    input_lock = 0;
}

void input_push(uint16_t type, uint16_t code, int32_t value) {
    uint64_t flags = save_irq();
    spin_lock(&input_lock);

    uint32_t next = (head + 1) % INPUT_QUEUE_SIZE;
    if (next != tail) {
        input_queue[head].type = type;
        input_queue[head].code = code;
        input_queue[head].value = value;
        head = next;

        spin_unlock(&input_lock);
        restore_irq(flags);

        /* Signal semaphore to wake up readers */
        sem_signal(&input_sem);
    } else {
        /* Drop event if full */
        spin_unlock(&input_lock);
        restore_irq(flags);
    }
}

int input_read(input_event_t* buffer, int count) {
    if (count <= 0) return 0;

    int events_read = 0;

    /* We only read 1 event at a time to match semaphore count consumption */
    /* and standard blocking behavior (return as soon as data available) */

    sem_wait(&input_sem);

    uint64_t flags = save_irq();
    spin_lock(&input_lock);

    if (head != tail) {
        *buffer = input_queue[tail];
        tail = (tail + 1) % INPUT_QUEUE_SIZE;
        events_read = 1;
    }

    spin_unlock(&input_lock);
    restore_irq(flags);

    return events_read;
}
