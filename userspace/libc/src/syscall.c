#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <stdarg.h>

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

static inline long syscall4(long n, long a1, long a2, long a3, long a4) {
    long ret;
    register long r10 __asm__("r10") = a4;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall5(long n, long a1, long a2, long a3, long a4, long a5) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return ret;
}

/* Wrappers */

long syscall(long nr, ...) {
    long ret;
    long arg1, arg2, arg3, arg4, arg5, arg6;
    __builtin_va_list ap;
    __builtin_va_start(ap, nr);

    arg1 = __builtin_va_arg(ap, long);
    arg2 = __builtin_va_arg(ap, long);
    arg3 = __builtin_va_arg(ap, long);
    arg4 = __builtin_va_arg(ap, long);
    arg5 = __builtin_va_arg(ap, long);
    arg6 = __builtin_va_arg(ap, long);

    __builtin_va_end(ap);

    register long r10 __asm__("r10") = arg4;
    register long r8  __asm__("r8")  = arg5;
    register long r9  __asm__("r9")  = arg6;

    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );

    if (ret > -4096 && ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

void exit(int status) {
    syscall1(SYS_exit, status);
    for(;;);
}

int open(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    long ret = syscall3(SYS_open, (long)pathname, flags, mode);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int creat(const char *pathname, mode_t mode) {
    return open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}


int wait(int *wstatus) {
    long ret = syscall3(SYS_wait4, -1, (long)wstatus, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int ioctl(int fd, int request, void *arg) {
    long ret = syscall3(SYS_ioctl, fd, request, (long)arg);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    long ret = syscall3(SYS_poll, (long)fds, (long)nfds, timeout);
    if (ret >= 0) {
        return (int)ret;
    }

    /* Fallback for early kernels where SYS_poll is still incomplete.
       We only need POLLIN readiness for tty/input devices right now. */
    if (-ret == ENOSYS || -ret == EINVAL) {
        int ready = 0;

        for (nfds_t i = 0; i < nfds; ++i) {
            int available = 0;
            fds[i].revents = 0;

            if ((fds[i].events & POLLIN) && ioctl(fds[i].fd, FIONREAD, &available) == 0 && available > 0) {
                fds[i].revents |= POLLIN;
                ready++;
            }
        }

        return ready;
    }

    errno = (int)-ret;
    return -1;
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

int stat(const char *pathname, struct stat *buf) {
    long ret = syscall2(SYS_stat, (long)pathname, (long)buf);
    if (ret < 0) { errno = -ret; return -1; }
    return 0;
}

int fstat(int fd, struct stat *buf) {
    long ret = syscall2(SYS_fstat, fd, (long)buf);
    if (ret < 0) { errno = -ret; return -1; }
    return 0;
}

int mkdir(const char *pathname, mode_t mode) {
    long ret = syscall2(SYS_mkdir, (long)pathname, mode);
    if (ret < 0) { errno = -ret; return -1; }
    return 0;
}

int unlink(const char *pathname) {
    long ret = syscall1(SYS_unlink, (long)pathname);
    if (ret < 0) { errno = -ret; return -1; }
    return 0;
}

#include <sys/sysinfo.h>

int sysinfo(struct sysinfo *info) {
    long ret = syscall1(SYS_sysinfo, (long)info);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

int rmdir(const char *pathname) {
    long ret = syscall1(SYS_rmdir, (long)pathname);
    if (ret < 0) { errno = -ret; return -1; }
    return 0;
}

int chdir(const char *pathname) {
    long ret = syscall1(SYS_chdir, (long)pathname);
    if (ret < 0) { errno = -ret; return -1; }
    return 0;
}

int getpid(void) {
    return (int)syscall0(SYS_getpid);
}

int setuid(int uid) {
    long ret = syscall1(SYS_setuid, uid);
    if (ret < 0) { errno = -ret; return -1; }
    return 0;
}

int setgid(int gid) {
    long ret = syscall1(SYS_setgid, gid);
    if (ret < 0) { errno = -ret; return -1; }
    return 0;
}

int kill(int pid, int sig) {
    long ret = syscall2(SYS_kill, pid, sig);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

/* Network Wrappers */

int socket(int domain, int type, int protocol) {
    long ret = syscall3(SYS_socket, domain, type, protocol);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    long ret = syscall3(SYS_connect, sockfd, (long)addr, addrlen);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    long ret = syscall3(SYS_bind, sockfd, (long)addr, addrlen);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int listen(int sockfd, int backlog) {
    long ret = syscall2(SYS_listen, sockfd, backlog);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    long ret = syscall3(SYS_accept, sockfd, (long)addr, (long)addrlen);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    /* Use SYS_sendto with NULL dest address and 0 addrlen */
    long ret = syscall6(SYS_sendto, sockfd, (long)buf, len, flags, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (ssize_t)ret;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    /* Use SYS_recvfrom with NULL src address and NULL addrlen */
    long ret = syscall6(SYS_recvfrom, sockfd, (long)buf, len, flags, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (ssize_t)ret;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    long ret = syscall6(SYS_sendto, sockfd, (long)buf, len, flags, (long)dest_addr, addrlen);
    if (ret < 0) { errno = -ret; return -1; }
    return (ssize_t)ret;
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    long ret = syscall6(SYS_recvfrom, sockfd, (long)buf, len, flags, (long)src_addr, (long)addrlen);
    if (ret < 0) { errno = -ret; return -1; }
    return (ssize_t)ret;
}
