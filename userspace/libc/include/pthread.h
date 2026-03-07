#ifndef _PTHREAD_H
#define _PTHREAD_H

#include <stdint.h>

typedef int pthread_t;

typedef struct {
    int locked;
} pthread_mutex_t;

typedef struct {
    int _dummy;
} pthread_mutexattr_t;

typedef struct {
    int attr;
} pthread_attr_t;

typedef struct {
    int futex;
} pthread_cond_t;

typedef struct {
    int attr;
} pthread_condattr_t;

/* pthreads over clone + futex */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg);
int pthread_join(pthread_t thread, void **retval);
void pthread_exit(void *retval);
pthread_t pthread_self(void);
int pthread_detach(pthread_t thread);

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_destroy(pthread_mutex_t *mutex);

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);

#endif /* _PTHREAD_H */
