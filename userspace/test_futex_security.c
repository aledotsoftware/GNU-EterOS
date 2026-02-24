#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>

#define SYS_futex 202
#define FUTEX_WAIT 0

static inline long syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return ret;
}

int main() {
    printf("Security Test: Syscall Futex Timeout Validation\n");

    int futex_word = 0;
    struct timespec *kernel_timeout = (struct timespec *)0xFFFFFFFF80000000;

    /* sys_futex(uaddr, op, val, timeout, uaddr2, val3) */
    long res = syscall6(SYS_futex, (long)&futex_word, FUTEX_WAIT, 0, (long)kernel_timeout, 0, 0);

    if (res < 0) {
        /* In this libc, errno is set by the wrapper, but here we are calling raw syscall.
           The kernel returns -errno. */
        int err = (int)-res;
        if (err == EFAULT) {
            printf("[PASS] Futex with kernel timeout failed with EFAULT\n");
            return 0;
        } else {
            printf("[FAIL] Futex with kernel timeout failed with unexpected errno %d (expected EFAULT)\n", err);
            return 1;
        }
    } else {
        printf("[FAIL] Futex with kernel timeout Succeeded! (Res: %ld)\n", res);
        return 1;
    }

    return 0;
}
