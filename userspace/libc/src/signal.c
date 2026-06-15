/**
 * éterOS Mini-LibC - Signal Functions
 * POSIX signal handling wrappers
 */

#include <signal.h>
#include <sys/syscall.h>
#include <errno.h>



static inline long syscall2(long n, long a1, long a2) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall4_sig(long n, long a1, long a2, long a3, long a4) {
    long ret;
    register long r10 __asm__("r10") = a4;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "rcx", "r11", "memory");
    return ret;
}



extern void __restore_rt(void) __attribute__((visibility("hidden")));

sighandler_t signal(int sig, sighandler_t handler) {
    struct sigaction act, oldact;
    act.sa_handler = handler;
    act.sa_flags = SA_RESTORER;
    act.sa_restorer = __restore_rt;
    act.sa_mask = 0;

    /* SYS_rt_sigaction expects 8 as sigsetsize (sizeof(uint64_t)) */
    long ret = syscall4_sig(SYS_rt_sigaction, sig, (long)&act, (long)&oldact, 8);
    if ((unsigned long)ret >= (unsigned long)-4095) {
        errno = (int)(-ret);
        return SIG_ERR;
    }
    return oldact.sa_handler;
}

int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact) {
    struct sigaction kact;
    const struct sigaction *pact = act;

    if (act) {
        kact = *act;
        /* Ensure SA_RESTORER is set */
        if (!(kact.sa_flags & SA_RESTORER)) {
            kact.sa_flags |= SA_RESTORER;
            kact.sa_restorer = __restore_rt;
        }
        pact = &kact;
    }

    long ret = syscall4_sig(SYS_rt_sigaction, sig, (long)pact, (long)oldact, 8);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return 0;
}

int sigemptyset(sigset_t *set) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    *set = 0;
    return 0;
}

int sigfillset(sigset_t *set) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    *set = ~0ULL;
    return 0;
}

int sigaddset(sigset_t *set, int signum) {
    if (!set || signum < 1 || signum > 64) {
        errno = EINVAL;
        return -1;
    }
    *set |= (1ULL << (signum - 1));
    return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    long ret = syscall4_sig(SYS_rt_sigprocmask, how, (long)set, (long)oldset, 8);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return 0;
}

int sigpending(sigset_t *set) {
    long ret = syscall2(SYS_rt_sigpending, (long)set, 8);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return 0;
}

int sigsuspend(const sigset_t *mask) {
    long ret = syscall2(SYS_rt_sigsuspend, (long)mask, 8);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return 0;
}

int sigaltstack(const stack_t *ss, stack_t *old_ss) {
    long ret = syscall2(SYS_sigaltstack, (long)ss, (long)old_ss);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return 0;
}

int raise(int sig) {
    /* Get our own PID */
    long ret_pid;
    __asm__ volatile ("syscall" : "=a"(ret_pid) : "a"((long)SYS_getpid) : "rcx", "r11", "memory");
    long ret = syscall2(SYS_kill, ret_pid, sig);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return 0;
}
