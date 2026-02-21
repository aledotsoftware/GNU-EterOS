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

/* Dedicated Mouse Queue */
static input_event_t mouse_queue[INPUT_QUEUE_SIZE];
static uint32_t mouse_head = 0;
static uint32_t mouse_tail = 0;
static sem_t mouse_sem;
static spinlock_t mouse_lock = 0;

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

    /* Initialize Mouse Queue */
    sem_init(&mouse_sem, 0);
    mouse_head = 0;
    mouse_tail = 0;
    mouse_lock = 0;
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

void input_mouse_push(uint16_t type, uint16_t code, int32_t value) {
    /* 1. Push to dedicated mouse queue */
    uint64_t flags = save_irq();
    spin_lock(&mouse_lock);

    uint32_t next = (mouse_head + 1) % INPUT_QUEUE_SIZE;
    if (next != mouse_tail) {
        mouse_queue[mouse_head].type = type;
        mouse_queue[mouse_head].code = code;
        mouse_queue[mouse_head].value = value;
        mouse_head = next;

        spin_unlock(&mouse_lock);
        restore_irq(flags);
        sem_signal(&mouse_sem);
    } else {
        spin_unlock(&mouse_lock);
        restore_irq(flags);
    }

    /* 2. Also push to global queue for aggregation */
    input_push(type, code, value);
}

int input_read_mouse(input_event_t* buffer, int count) {
    if (count <= 0) return 0;

    /* Blocking wait for mouse data */
    sem_wait(&mouse_sem);

    int events_read = 0;
    uint64_t flags = save_irq();
    spin_lock(&mouse_lock);

    if (mouse_head != mouse_tail) {
        *buffer = mouse_queue[mouse_tail];
        mouse_tail = (mouse_tail + 1) % INPUT_QUEUE_SIZE;
        events_read = 1;
    }

    spin_unlock(&mouse_lock);
    restore_irq(flags);

    return events_read;
}
