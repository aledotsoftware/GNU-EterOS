#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

int errno = 0;

static inline long syscall0(long n) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall1(long n, long a1) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall2(long n, long a1, long a2) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall3(long n, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
    return ret;
}

/* Wrappers */

void exit(int status) {
    syscall1(SYS_exit, status);
    for(;;);
}

int open(const char *pathname, int flags) {
    long ret = syscall3(SYS_open, (long)pathname, flags, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int close(int fd) {
    long ret = syscall1(SYS_close, fd);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

ssize_t read(int fd, void *buf, size_t count) {
    long ret = syscall3(SYS_read, fd, (long)buf, count);
    if (ret < 0) { errno = -ret; return -1; }
    return (ssize_t)ret;
}

ssize_t write(int fd, const void *buf, size_t count) {
    long ret = syscall3(SYS_write, fd, (long)buf, count);
    if (ret < 0) { errno = -ret; return -1; }
    return (ssize_t)ret;
}

int64_t lseek(int fd, int64_t offset, int whence) {
    long ret = syscall3(SYS_lseek, fd, offset, whence);
    if (ret < 0) { errno = -ret; return -1; }
    return (int64_t)ret;
}

int getpid(void) {
    return (int)syscall0(SYS_getpid);
}

int kill(int pid, int sig) {
    long ret = syscall2(SYS_kill, pid, sig);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

unsigned int sleep(unsigned int seconds) {
    /* For now, busy loop or implementing nanosleep syscall?
       Using loop for simplicity or assume nanosleep exists?
       The memory/syscall.h has SYS_nanosleep (35).
       But kernel implementation is missing SYS_nanosleep in syscall.c.
       I will just loop in userspace or ignore.
       Actually, I can implement sys_sleep in kernel if I want, but plan didn't say so.
       I'll omit sleep implementation or make it a stub. */
    return 0;
}
