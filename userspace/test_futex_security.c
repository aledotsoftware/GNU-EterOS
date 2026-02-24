#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/syscall.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef ETIMEDOUT
#define ETIMEDOUT 110
#endif

#define SYS_futex 202
#define FUTEX_WAIT 0
#define FUTEX_PRIVATE_FLAG 128

static inline long my_syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

int main() {
    uint32_t val = 0;

    printf("[TEST] Futex Security Test\n");

    /* 1. Test Valid Timeout */
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000;

    /* Syscall args: uaddr, op, val, timeout, uaddr2, val3 */
    int res = my_syscall6(SYS_futex, (long)&val, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, val, (long)&ts, 0, 0);
    if (res == 0 || errno == ETIMEDOUT || errno == EAGAIN || errno == EINTR) {
        printf("[PASS] Valid Timeout: OK (res=%d, errno=%d)\n", res, errno);
    } else {
        printf("[FAIL] Valid Timeout: Unexpected error %d\n", errno);
    }

    /* 2. Test Invalid Timeout (Unmapped) */
    struct timespec *bad_ts = (struct timespec *)0xdeadbeef;
    res = my_syscall6(SYS_futex, (long)&val, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, val, (long)bad_ts, 0, 0);

    if (res == -1 && errno == EFAULT) {
        printf("[PASS] Invalid Timeout (Unmapped): EFAULT received\n");
    } else {
        printf("[FAIL] Invalid Timeout (Unmapped): Expected EFAULT, got res=%d errno=%d\n", res, errno);
    }

    /* 3. Test Invalid Timeout (Kernel Address) */
    struct timespec *kernel_ts = (struct timespec *)0xFFFFFFFF81000000;
    res = my_syscall6(SYS_futex, (long)&val, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, val, (long)kernel_ts, 0, 0);

    if (res == -1 && errno == EFAULT) {
        printf("[PASS] Invalid Timeout (Kernel Addr): EFAULT received\n");
    } else {
        printf("[FAIL] Invalid Timeout (Kernel Addr): Expected EFAULT, got res=%d errno=%d\n", res, errno);
    }

    return 0;
}
