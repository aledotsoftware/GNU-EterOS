/**
 * éterOS - Fast Userspace Mutex (Futex) Implementation
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 */

#include <futex.h>
#include <task.h>
#include <mm.h>
#include <lock.h>
#include <errno.h>
#include <string.h>
#include <hal.h>
#include <time.h>
#include <timer.h>

#define FUTEX_BUCKETS 16

/* Internal node structure */
typedef struct futex_node {
    task_t *task;
    uint32_t *uaddr;
    struct futex_node *next;
} futex_node_t;

/* Bucket structure */
typedef struct {
    spinlock_t lock;
    futex_node_t *head;
} futex_bucket_t;

static futex_bucket_t buckets[FUTEX_BUCKETS];

static int futex_hash(uint32_t *uaddr, int is_private) {
    task_t* current = task_get_current();
    uint64_t addr = (uint64_t)uaddr;
    /* Mix CR3 into the hash to isolate processes, but threads sharing VM will match */
    uint64_t cr3 = (current && !is_private) ? current->cr3 : 0;
    return ((addr >> 2) ^ (cr3 >> 12)) % FUTEX_BUCKETS;
}

void futex_init(void) {
    for (int i = 0; i < FUTEX_BUCKETS; i++) {
        buckets[i].lock = 0;
        buckets[i].head = NULL;
    }
}

/* Helper to validate user address */
static int validate_uaddr(uint32_t *uaddr) {
    uint64_t addr = (uint64_t)uaddr;
    /* Check alignment */
    if (addr & 3) return 0;
    /* Check for NULL */
    if (addr == 0) return 0;
    /* Basic user space check (assuming kernel is high half or similar,
       but for now just check non-null and alignment) */
    return 1;
}

int futex_wait(uint32_t *uaddr, uint32_t val, const void *timeout, int op) {
    const struct timespec *ts = (const struct timespec *)timeout;
    uint64_t target_tick = 0;
    int has_timeout = 0;

    if (ts) {
        uint64_t ticks = (uint64_t)ts->tv_sec * TIMER_HZ;
        /* TIMER_HZ is 1000, so each tick is 1ms = 1,000,000 ns */
        ticks += (uint64_t)ts->tv_nsec / 1000000;

        target_tick = timer_get_ticks() + ticks;
        if (target_tick == 0) target_tick = 1; /* Ensure non-zero */
        has_timeout = 1;
    }

    if (!validate_uaddr(uaddr)) return -EFAULT;

    /* 1. Atomically check value (optimistic check) */
    /* Note: In a real kernel, we need safe user access here (get_user).
       For now, we assume direct access is possible but unsafe. */
    if (*uaddr != val) return -EAGAIN;

    int is_private = (op & FUTEX_PRIVATE_FLAG) ? 1 : 0;
    int bucket_idx = futex_hash(uaddr, is_private);
    futex_bucket_t *b = &buckets[bucket_idx];

    /* 2. Allocate node before locking to minimize critical section */
    futex_node_t *node = (futex_node_t*)kmalloc(sizeof(futex_node_t));
    if (!node) return -ENOMEM;

    spin_lock(&b->lock);

    /* 3. Re-check value under lock to avoid race */
    if (*uaddr != val) {
        spin_unlock(&b->lock);
        kfree(node);
        return -EAGAIN;
    }

    node->task = task_get_current();
    node->uaddr = uaddr;
    node->next = b->head;
    b->head = node;

    /* 4. Block task */
    if (has_timeout) {
        task_block_with_timeout(target_tick);
    } else {
        task_block();
    }

    spin_unlock(&b->lock);

    /* 5. Yield CPU */
    schedule();

    /* 6. We are back. Check why. */
    int timed_out = 0;
    if (has_timeout && timer_get_ticks() >= target_tick) {
        timed_out = 1;
    }

    spin_lock(&b->lock);

    /* Search for our node */
    futex_node_t **pp = &b->head;
    futex_node_t *curr = b->head;
    int found = 0;

    while (curr) {
        if (curr == node) {
            /* Still in queue! This means spurious wakeup, signal, or timeout. */
            found = 1;
            /* Remove from list */
            *pp = curr->next;
            break;
        }
        pp = &curr->next;
        curr = curr->next;
    }

    spin_unlock(&b->lock);

    /* Free the node that we allocated */
    kfree(node);

    if (found) {
        /* If we were found in the queue, we weren't woken by futex_wake. */
        if (timed_out) return -ETIMEDOUT;
        return -EINTR;
    }

    return 0;
}

int futex_wake(uint32_t *uaddr, int count, int op) {
    if (!validate_uaddr(uaddr)) return -EFAULT;

    int is_private = (op & FUTEX_PRIVATE_FLAG) ? 1 : 0;
    int bucket_idx = futex_hash(uaddr, is_private);
    futex_bucket_t *b = &buckets[bucket_idx];

    spin_lock(&b->lock);

    futex_node_t **pp = &b->head;
    futex_node_t *curr = b->head;
    int woken = 0;
    task_t* current = task_get_current();

    while (curr && woken < count) {
        if (curr->uaddr == uaddr && (is_private || curr->task->cr3 == current->cr3)) {
            /* Wake up this task */
            if (curr->task->state == TASK_BLOCKED) {
                task_wakeup(curr->task);
            }

            /* Remove from list */
            futex_node_t *to_remove = curr; /* Kept for logic, but we don't free it */
            (void)to_remove;

            *pp = curr->next;
            curr = curr->next;

            /* We do NOT free the node here. The waiter does it. */
            woken++;
        } else {
            pp = &curr->next;
            curr = curr->next;
        }
    }

    spin_unlock(&b->lock);

    return woken;
}
