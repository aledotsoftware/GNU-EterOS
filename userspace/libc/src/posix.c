/**
 * éterOS Mini-LibC - POSIX Extras
 * Additional syscall wrappers for musl libc compatibility
 */

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <errno.h>

extern int errno;

/* Syscall primitives (shared with syscall.c but self-contained here) */
static inline long _syscall0(long n) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n) : "rcx", "r11", "memory");
    return ret;
}

static inline long _syscall1(long n, long a1) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static inline long _syscall2(long n, long a1, long a2) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

static inline long _syscall3(long n, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
    return ret;
}

static inline long _syscall4(long n, long a1, long a2, long a3, long a4) {
    long ret;
    register long r10 __asm__("r10") = a4;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "rcx", "r11", "memory");
    return ret;
}

static inline long _syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3),
                      "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return ret;
}

/* fork() */
int fork(void) {
    long ret = _syscall0(SYS_fork);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return (int)ret;
}

/* execve() */
int execve(const char *pathname, char *const argv[], char *const envp[]) {
    long ret = _syscall3(SYS_execve, (long)pathname, (long)argv, (long)envp);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return (int)ret;
}

/* waitpid() */
pid_t waitpid(pid_t pid, int *status, int options) {
    long ret = _syscall4(SYS_wait4, pid, (long)status, options, 0);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return (pid_t)ret;
}

/* pipe() */
int pipe(int pipefd[2]) {
    long ret = _syscall1(SYS_pipe, (long)pipefd);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

/* dup2() */
int dup2(int oldfd, int newfd) {
    long ret = _syscall2(SYS_dup2, oldfd, newfd);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return (int)ret;
}

/* sched_yield() */
int sched_yield(void) {
    long ret = _syscall0(SYS_sched_yield);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

/* nanosleep() */
int nanosleep(const void *req, void *rem) {
    long ret = _syscall2(SYS_nanosleep, (long)req, (long)rem);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

/* mmap() */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, int64_t offset) {
    long ret = _syscall6(SYS_mmap, (long)addr, (long)length, prot, flags, fd, offset);
    if (ret < 0) {
        errno = (int)(-ret);
        return (void*)-1; /* MAP_FAILED */
    }
    return (void*)ret;
}

/* munmap() */
int munmap(void *addr, size_t length) {
    long ret = _syscall2(SYS_munmap, (long)addr, (long)length);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

/* uname() */
int uname(void *buf) {
    long ret = _syscall1(SYS_uname, (long)buf);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

/* brk() */
void *brk(void *addr) {
    long ret = _syscall1(SYS_brk, (long)addr);
    return (void*)ret;
}

/* sbrk() */
void *sbrk(int64_t increment) {
    long current = _syscall1(SYS_brk, 0);
    if (increment == 0) return (void*)current;
    long requested = current + increment;
    long new_brk = _syscall1(SYS_brk, requested);
    if (new_brk != requested) {
        errno = ENOMEM;
        return (void*)-1;
    }
    return (void*)current;
}

/* sleep() — implemented via nanosleep */
unsigned int sleep(unsigned int seconds) {
    struct { int64_t sec; int64_t nsec; } req = {seconds, 0};
    struct { int64_t sec; int64_t nsec; } rem = {0, 0};
    long ret = _syscall2(SYS_nanosleep, (long)&req, (long)&rem);
    if (ret < 0) return (unsigned int)rem.sec;
    return 0;
}

/* usleep() */
int usleep(unsigned int usec) {
    struct { int64_t sec; int64_t nsec; } req;
    req.sec = usec / 1000000;
    req.nsec = (usec % 1000000) * 1000;
    long ret = _syscall2(SYS_nanosleep, (long)&req, (long)0);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

/* getppid() */
int getppid(void) {
    return (int)_syscall0(110); /* SYS_getppid */
}

/* access() */
int access(const char *pathname, int mode) {
    long ret = _syscall2(SYS_access, (long)pathname, mode);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

/* getcwd() */
char *getcwd(char *buf, size_t size) {
    long ret = _syscall2(SYS_getcwd, (long)buf, (long)size);
    if (ret < 0) { errno = (int)(-ret); return (void*)0; }
    return buf;
}
