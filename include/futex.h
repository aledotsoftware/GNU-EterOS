#ifndef ETEROS_FUTEX_H
#define ETEROS_FUTEX_H

#include <types.h>

/* Futex Operations */
#define FUTEX_WAIT      0
#define FUTEX_WAKE      1
#define FUTEX_FD        2
#define FUTEX_REQUEUE   3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAIT_BITSET 9
#define FUTEX_WAKE_BITSET 10

#define FUTEX_BITSET_MATCH_ANY 0xffffffff

#define FUTEX_PRIVATE_FLAG 128
#define FUTEX_CLOCK_REALTIME 256
#define FUTEX_CMD_MASK  ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)

/**
 * Initialize the futex subsystem (buckets).
 */
void futex_init(void);

/**
 * Wait on a futex variable.
 *
 * Atomically checks if *uaddr == val. If so, puts the current task to sleep.
 * Returns 0 on success (woken by futex_wake), -EAGAIN if *uaddr != val,
 * -ETIMEDOUT if timeout expired (not implemented yet), or -EINTR.
 *
 * @param uaddr User address of the futex word.
 * @param val Expected value of the futex word.
 * @param timeout Pointer to timespec (optional, currently ignored).
 * @return 0 on success, or negative error code.
 */
int futex_wait(uint32_t *uaddr, uint32_t val, const void *timeout, int op, uint32_t bitset);

/**
 * Wake up tasks waiting on a futex variable.
 *
 * @param uaddr User address of the futex word.
 * @param count Maximum number of tasks to wake up.
 * @return Number of tasks woken up.
 */
int futex_wake(uint32_t *uaddr, int count, int op, uint32_t bitset);

#endif /* ETEROS_FUTEX_H */
