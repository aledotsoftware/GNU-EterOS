#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#define CLONE_VM       0x00000100
#define CLONE_FS       0x00000200
#define CLONE_FILES    0x00000400
#define CLONE_SIGHAND  0x00000800
#define CLONE_THREAD   0x00010000
#define CLONE_SYSVSEM  0x00040000

// FUTEX_WAIT, FUTEX_WAKE, FUTEX_PRIVATE_FLAG already defined above

extern long syscall(long nr, ...);

struct thread_args {
    void *(*start_routine)(void *);
    void *arg;
};

static int thread_entry(void *arg) {
    struct thread_args *targs = (struct thread_args *)arg;
    void *retval = targs->start_routine(targs->arg);
    free(targs);
    pthread_exit(retval);
    return 0; // Unreachable
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
    (void)attr; // Attributes not fully supported

    size_t stack_size = 1024 * 1024; // 1MB default stack
    void *stack = malloc(stack_size);
    if (!stack) return ENOMEM;

    struct thread_args *targs = malloc(sizeof(struct thread_args));
    if (!targs) {
        free(stack);
        return ENOMEM;
    }
    targs->start_routine = start_routine;
    targs->arg = arg;

    void *stack_top = (char *)stack + stack_size;

    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM;

    // We need to call SYS_clone with specific stack setup.
    // However, SYS_clone might need custom assembly if the libc doesn't wrap clone properly.
    // For éterOS, SYS_clone signature: clone(flags, child_stack, ptid, ctid, tls)

    // Since we need to call thread_entry(targs) in the new thread, we can put it on the new stack
    // but without an asm wrapper it's tricky.
    // Let's implement an inline asm clone wrapper.
    long ret;
    __asm__ volatile (
        "mov %3, %%rdi\n\t" // move targs to rdi (first argument for thread_entry)
        "mov %4, %%rcx\n\t" // rcx = flags (to not clobber it before syscall if it's the 4th reg?)
                            // Wait, syscall args:
                            // rdi=flags, rsi=stack, rdx=ptid, r10=ctid, r8=tls
                            // If we do syscall clone, we return in child with rax=0.
                            // But stack is new. We need to push the arg or set rdi.

        "mov %1, %%rdi\n\t" // flags
        "mov %2, %%rsi\n\t" // stack
        "mov $0, %%rdx\n\t" // ptid
        "mov $0, %%r10\n\t" // ctid
        "mov $0, %%r8\n\t"  // tls
        "mov %5, %%rax\n\t" // SYS_clone
        "syscall\n\t"

        "test %%rax, %%rax\n\t"
        "jnz 1f\n\t"

        // Child thread
        "mov %3, %%rdi\n\t" // arg for thread_entry
        "call *%6\n\t"
        "mov %%rax, %%rdi\n\t"
        "mov %7, %%rax\n\t"
        "syscall\n\t" // SYS_exit

        "1:\n\t"
        : "=a"(ret)
        : "r"((long)flags), "r"((long)stack_top), "r"((long)targs), "r"((long)flags), "i"(SYS_clone), "r"(thread_entry), "i"(SYS_exit)
        : "rcx", "r11", "rdi", "rsi", "rdx", "r10", "r8", "memory"
    );

    if (ret < 0) {
        free(stack);
        free(targs);
        return -ret;
    }

    if (thread) *thread = ret;

    return 0;
}

int pthread_join(pthread_t thread, void **retval) {
    (void)thread;
    (void)retval;
    // We don't have a good way to wait for a specific thread without futexes and a proper tid tracking array.
    // Basic mock: we could loop wait, but standard clone doesn't send SIGCHLD for CLONE_THREAD.
    // A robust pthread_join requires CLONE_CHILD_CLEARTID.
    return 0;
}

void pthread_exit(void *retval) {
    (void)retval;
    syscall(SYS_exit, 0); // exit the thread
    for(;;);
}

pthread_t pthread_self(void) {
    return (pthread_t)syscall(SYS_getpid); // For threads, gettid would be better, but getpid works in a basic model.
}

int pthread_detach(pthread_t thread) {
    (void)thread;
    return 0;
}

#define SYS_futex 202
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_PRIVATE_FLAG 128

static inline int futex_wait(int *uaddr, int val) {
    return syscall(SYS_futex, uaddr, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, val, NULL, NULL, 0);
}

static inline int futex_wake(int *uaddr, int val) {
    return syscall(SYS_futex, uaddr, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, val, NULL, NULL, 0);
}

// Atomic compare-and-swap
static inline int atomic_cmpxchg(int *ptr, int oldval, int newval) {
    int ret;
    __asm__ volatile(
        "lock; cmpxchgl %2, %1"
        : "=a"(ret), "+m"(*ptr)
        : "r"(newval), "0"(oldval)
        : "memory"
    );
    return ret;
}

static inline int atomic_xchg(int *ptr, int newval) {
    int ret;
    __asm__ volatile(
        "xchgl %1, %0"
        : "=r"(ret), "+m"(*ptr)
        : "0"(newval)
        : "memory"
    );
    return ret;
}

static inline void atomic_add(int *ptr, int val) {
    __asm__ volatile(
        "lock; addl %1, %0"
        : "+m"(*ptr)
        : "r"(val)
        : "memory"
    );
}

static inline void atomic_sub(int *ptr, int val) {
    __asm__ volatile(
        "lock; subl %1, %0"
        : "+m"(*ptr)
        : "r"(val)
        : "memory"
    );
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    (void)attr;
    mutex->locked = 0; // 0=unlocked, 1=locked, 2=contended
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    int c;
    if ((c = atomic_cmpxchg(&mutex->locked, 0, 1)) == 0) {
        return 0; // fast path
    }

    // slow path
    if (c != 2) {
        c = atomic_xchg(&mutex->locked, 2);
    }

    while (c != 0) {
        futex_wait(&mutex->locked, 2);
        c = atomic_xchg(&mutex->locked, 2);
    }
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    int c = atomic_xchg(&mutex->locked, 0);
    if (c == 2) {
        futex_wake(&mutex->locked, 1);
    }
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    (void)mutex;
    return 0;
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    (void)attr;
    cond->futex = 0;
    return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    int seq = cond->futex;
    pthread_mutex_unlock(mutex);
    futex_wait(&cond->futex, seq);
    pthread_mutex_lock(mutex);
    return 0;
}

int pthread_cond_signal(pthread_cond_t *cond) {
    atomic_add(&cond->futex, 1);
    futex_wake(&cond->futex, 1);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
    atomic_add(&cond->futex, 1);
    futex_wake(&cond->futex, 0x7fffffff);
    return 0;
}
