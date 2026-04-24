#include "lwip/sys.h"
#include "timer.h"

u32_t sys_now(void) {
    /* 1 tick = 10ms (100 Hz). Result in ms. */
    return (u32_t)(timer_get_ticks() * 10);
}

void sys_init(void) {
    /* Timer already initialized in HAL */
}

// Mutexes (stubs for now, or just map to spinlocks/semaphores)
#include <lock.h>
err_t sys_mutex_new(sys_mutex_t *mutex) {
    *mutex = 1;
    return ERR_OK;
}
void sys_mutex_lock(sys_mutex_t *mutex) {
    (void)mutex;
}
void sys_mutex_unlock(sys_mutex_t *mutex) {
    (void)mutex;
}
void sys_mutex_free(sys_mutex_t *mutex) {
    (void)mutex;
}

err_t sys_sem_new(sys_sem_t *sem, u8_t count) { *sem = count; return ERR_OK; }
void sys_sem_free(sys_sem_t *sem) { (void)sem; }
void sys_sem_signal(sys_sem_t *sem) { (void)sem; }
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout) { (void)sem; (void)timeout; return 0; }
int sys_sem_valid(sys_sem_t *sem) { return *sem != 0; }
void sys_sem_set_invalid(sys_sem_t *sem) { *sem = 0; }

err_t sys_mbox_new(sys_mbox_t *mbox, int size) { (void)size; *mbox = 1; return ERR_OK; }
void sys_mbox_free(sys_mbox_t *mbox) { (void)mbox; }
void sys_mbox_post(sys_mbox_t *mbox, void *msg) { (void)mbox; (void)msg; }
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg) { (void)mbox; (void)msg; return ERR_OK; }
err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg) { return sys_mbox_trypost(mbox, msg); }
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout) { (void)mbox; (void)msg; (void)timeout; return 0; }
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg) { (void)mbox; (void)msg; return SYS_MBOX_EMPTY; }
int sys_mbox_valid(sys_mbox_t *mbox) { return *mbox != 0; }
void sys_mbox_set_invalid(sys_mbox_t *mbox) { *mbox = 0; }

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio) {
    (void)name; (void)thread; (void)arg; (void)stacksize; (void)prio;
    return 1;
}

int errno;
