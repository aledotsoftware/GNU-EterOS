#include "lwip/sys.h"
#include "timer.h"
#include <task.h>
#include <sem.h>
#include <mm.h>
#include <lwip/timeouts.h>
#include <string.h>

u32_t sys_now(void) {
    /* 1 tick = 10ms (100 Hz). Result in ms. */
    return (u32_t)(timer_get_ticks() * 10);
}

void sys_init(void) {
    /* Timer already initialized in HAL */
}

/* Mutexes (mapped to EterOS semaphores) */
err_t sys_mutex_new(sys_mutex_t *mutex) {
    sem_t* s = (sem_t*)kmalloc(sizeof(sem_t));
    if(!s) return ERR_MEM;
    sem_init(s, 1);
    *mutex = (sys_mutex_t)s;
    return ERR_OK;
}
void sys_mutex_lock(sys_mutex_t *mutex) {
    if(mutex && *mutex) sem_wait((sem_t*)*mutex);
}
void sys_mutex_unlock(sys_mutex_t *mutex) {
    if(mutex && *mutex) sem_signal((sem_t*)*mutex);
}
void sys_mutex_free(sys_mutex_t *mutex) {
    if(mutex && *mutex) {
        kfree((void*)*mutex);
        *mutex = (sys_mutex_t)NULL;
    }
}
int sys_mutex_valid(sys_mutex_t *mutex) {
    return (mutex != NULL && *mutex != (sys_mutex_t)NULL);
}
void sys_mutex_set_invalid(sys_mutex_t *mutex) {
    if(mutex) *mutex = (sys_mutex_t)NULL;
}

/* Semaphores */
err_t sys_sem_new(sys_sem_t *sem, u8_t count) {
    sem_t* s = (sem_t*)kmalloc(sizeof(sem_t));
    if(!s) return ERR_MEM;
    sem_init(s, count);
    *sem = (sys_sem_t)s;
    return ERR_OK;
}
void sys_sem_free(sys_sem_t *sem) {
    if(sem && *sem) {
        kfree((void*)*sem);
        *sem = (sys_sem_t)NULL;
    }
}
void sys_sem_signal(sys_sem_t *sem) {
    if(sem && *sem) sem_signal((sem_t*)*sem);
}
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout) {
    if(!sem || !*sem) return SYS_ARCH_TIMEOUT;
    sem_t* s = (sem_t*)*sem;
    if(timeout == 0) {
        sem_wait(s);
        return 0;
    } else {
        uint64_t start_time = sys_now();
        /* Simple polling for timeout since we don't have sem_wait_timeout */
        while(1) {
            hal_interrupts_disable();
            spin_lock(&(s->lock));
            if(s->count > 0) {
                s->count--;
                spin_unlock(&(s->lock));
                hal_interrupts_enable();
                return sys_now() - start_time;
            }
            spin_unlock(&(s->lock));
            hal_interrupts_enable();

            if(sys_now() - start_time >= timeout) return SYS_ARCH_TIMEOUT;
            sys_check_timeouts();
            task_sleep(10);
        }
    }
}
int sys_sem_valid(sys_sem_t *sem) {
    return (sem != NULL && *sem != (sys_sem_t)NULL);
}
void sys_sem_set_invalid(sys_sem_t *sem) {
    if(sem) *sem = (sys_sem_t)NULL;
}

/* Mailboxes */
typedef struct sys_mbox_s {
    sem_t sem;
    sem_t mutex;
    void** messages;
    int size;
    int head;
    int tail;
} sys_mbox_s_t;

err_t sys_mbox_new(sys_mbox_t *mbox, int size) {
    if(size <= 0) size = 32;
    sys_mbox_s_t* m = (sys_mbox_s_t*)kmalloc(sizeof(sys_mbox_s_t));
    if(!m) return ERR_MEM;
    m->messages = (void**)kmalloc(sizeof(void*) * size);
    if(!m->messages) { kfree(m); return ERR_MEM; }
    m->size = size;
    m->head = m->tail = 0;
    sem_init(&(m->sem), 0);
    sem_init(&(m->mutex), 1);
    *mbox = (sys_mbox_t)m;
    return ERR_OK;
}
void sys_mbox_free(sys_mbox_t *mbox) {
    if(mbox && *mbox) {
        sys_mbox_s_t* m = (sys_mbox_s_t*)*mbox;
        if(m->messages) kfree(m->messages);
        kfree(m);
        *mbox = (sys_mbox_t)NULL;
    }
}
void sys_mbox_post(sys_mbox_t *mbox, void *msg) {
    if(!mbox || !*mbox) return;
    sys_mbox_s_t* m = (sys_mbox_s_t*)*mbox;
    while(1) {
        sem_wait(&(m->mutex));
        int next_head = (m->head + 1) % m->size;
        if(next_head != m->tail) {
            m->messages[m->head] = msg;
            m->head = next_head;
            sem_signal(&(m->mutex));
            sem_signal(&(m->sem));
            return;
        }
        sem_signal(&(m->mutex));
        task_sleep(10);
    }
}
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg) {
    if(!mbox || !*mbox) return ERR_VAL;
    sys_mbox_s_t* m = (sys_mbox_s_t*)*mbox;
    sem_wait(&(m->mutex));
    int next_head = (m->head + 1) % m->size;
    if(next_head != m->tail) {
        m->messages[m->head] = msg;
        m->head = next_head;
        sem_signal(&(m->mutex));
        sem_signal(&(m->sem));
        return ERR_OK;
    }
    sem_signal(&(m->mutex));
    return ERR_MEM;
}
err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg) {
    return sys_mbox_trypost(mbox, msg);
}
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout) {
    if(!mbox || !*mbox) return SYS_ARCH_TIMEOUT;
    sys_mbox_s_t* m = (sys_mbox_s_t*)*mbox;

    u32_t start_time = sys_now();

    if(timeout == 0) {
        sem_wait(&(m->sem));
    } else {
        while(1) {
            hal_interrupts_disable();
            spin_lock(&(m->sem.lock));
            if(m->sem.count > 0) {
                m->sem.count--;
                spin_unlock(&(m->sem.lock));
                hal_interrupts_enable();
                break;
            }
            spin_unlock(&(m->sem.lock));
            hal_interrupts_enable();

            if(sys_now() - start_time >= timeout) return SYS_ARCH_TIMEOUT;
            sys_check_timeouts();
            task_sleep(10);
        }
    }

    sem_wait(&(m->mutex));
    if(msg) *msg = m->messages[m->tail];
    m->tail = (m->tail + 1) % m->size;
    sem_signal(&(m->mutex));

    return sys_now() - start_time;
}
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg) {
    if(!mbox || !*mbox) return SYS_MBOX_EMPTY;
    sys_mbox_s_t* m = (sys_mbox_s_t*)*mbox;

    hal_interrupts_disable();
    spin_lock(&(m->sem.lock));
    if(m->sem.count > 0) {
        m->sem.count--;
        spin_unlock(&(m->sem.lock));
        hal_interrupts_enable();

        sem_wait(&(m->mutex));
        if(msg) *msg = m->messages[m->tail];
        m->tail = (m->tail + 1) % m->size;
        sem_signal(&(m->mutex));
        return 0;
    }
    spin_unlock(&(m->sem.lock));
    hal_interrupts_enable();
    return SYS_MBOX_EMPTY;
}
int sys_mbox_valid(sys_mbox_t *mbox) {
    return (mbox != NULL && *mbox != (sys_mbox_t)NULL);
}
void sys_mbox_set_invalid(sys_mbox_t *mbox) {
    if(mbox) *mbox = (sys_mbox_t)NULL;
}

/* Threads */
/* In EterOS task_create only takes void (*)(void) without arguments.
   We need a trampoline mechanism since sys_thread_new expects args.
   Since this is kernel space, we can dynamically allocate a struct
   and map it based on thread ID, or just use global arrays since we
   have few threads (e.g. tcpip_thread) */

#define MAX_LWIP_THREADS 8
typedef struct {
    lwip_thread_fn thread_func;
    void *arg;
    uint32_t task_id;
    int active;
} thread_data_t;
static thread_data_t thread_data[MAX_LWIP_THREADS];
static sem_t thread_data_mutex;
static int thread_data_inited = 0;

static void sys_thread_trampoline(void) {
    task_t* current = task_get_current();
    if(!current) return;

    lwip_thread_fn fn = NULL;
    void* arg = NULL;

    sem_wait(&thread_data_mutex);
    for(int i=0; i<MAX_LWIP_THREADS; i++) {
        if(thread_data[i].active && thread_data[i].task_id == current->id) {
            fn = thread_data[i].thread_func;
            arg = thread_data[i].arg;
            break;
        }
    }
    sem_signal(&thread_data_mutex);

    if(fn) {
        fn(arg);
    }
    task_exit(0);
}

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio) {
    (void)stacksize; (void)prio;

    if(!thread_data_inited) {
        sem_init(&thread_data_mutex, 1);
        for(int i=0; i<MAX_LWIP_THREADS; i++) thread_data[i].active = 0;
        thread_data_inited = 1;
    }

    sem_wait(&thread_data_mutex);
    int idx = -1;
    for(int i=0; i<MAX_LWIP_THREADS; i++) {
        if(!thread_data[i].active) {
            idx = i;
            break;
        }
    }
    if(idx == -1) {
        sem_signal(&thread_data_mutex);
        return 0; /* No slots */
    }

    thread_data[idx].thread_func = thread;
    thread_data[idx].arg = arg;
    thread_data[idx].active = 1;

    hal_interrupts_disable();
    int tid = task_create(name, sys_thread_trampoline);
    if(tid < 0) {
        hal_interrupts_enable();
        thread_data[idx].active = 0;
        sem_signal(&thread_data_mutex);
        return 0;
    }
    thread_data[idx].task_id = (uint32_t)tid;
    hal_interrupts_enable();
    sem_signal(&thread_data_mutex);

    return tid;
}

int errno;
