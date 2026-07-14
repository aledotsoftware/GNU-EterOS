#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <stdarg.h>

long syscall0(long n) {
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

#define SYSCALL_RETURN(ret) \
    if ((unsigned long)(ret) >= (unsigned long)-4095) { \
        errno = (int)(-(ret)); \
        return -1; \
    } \
    return (int)(ret);

#define SYSCALL_RETURN_PTR(ret) \
    if ((unsigned long)(ret) >= (unsigned long)-4095) { \
        errno = (int)(-(ret)); \
        return (void*)-1; \
    } \
    return (void*)(ret);

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

    if ((unsigned long)ret >= (unsigned long)-4095) {
        errno = (int)(-ret);
        return -1;
    }
    return ret;
}

extern void __run_atexit(void);

void exit(int status) {
    __run_atexit();
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
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int creat(const char *pathname, mode_t mode) {
    return open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}


int wait(int *wstatus) {
    long ret = syscall3(SYS_wait4, -1, (long)wstatus, 0);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int ioctl(int fd, int request, void *arg) {
    long ret = syscall3(SYS_ioctl, fd, request, (long)arg);
    SYSCALL_RETURN(ret);
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
    SYSCALL_RETURN(ret);
    return (int)ret;
}

ssize_t read(int fd, void *buf, size_t count) {
    long ret = syscall3(SYS_read, fd, (long)buf, count);
    SYSCALL_RETURN(ret);
    return (ssize_t)ret;
}

ssize_t write(int fd, const void *buf, size_t count) {
    long ret = syscall3(SYS_write, fd, (long)buf, count);
    SYSCALL_RETURN(ret);
    return (ssize_t)ret;
}

int64_t lseek(int fd, int64_t offset, int whence) {
    long ret = syscall3(SYS_lseek, fd, offset, whence);
    SYSCALL_RETURN(ret);
    return (int64_t)ret;
}

int stat(const char *pathname, struct stat *buf) {
    long ret = syscall2(SYS_stat, (long)pathname, (long)buf);
    SYSCALL_RETURN(ret);
    return 0;
}

int fstat(int fd, struct stat *buf) {
    long ret = syscall2(SYS_fstat, fd, (long)buf);
    SYSCALL_RETURN(ret);
    return 0;
}

int mkdir(const char *pathname, mode_t mode) {
    long ret = syscall2(SYS_mkdir, (long)pathname, mode);
    SYSCALL_RETURN(ret);
    return 0;
}

int unlink(const char *pathname) {
    long ret = syscall1(SYS_unlink, (long)pathname);
    SYSCALL_RETURN(ret);
    return 0;
}

#include <sys/sysinfo.h>
#include <string.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/random.h>
#include <grp.h>
#include <syslog.h>

int sysinfo(struct sysinfo *info) {
    long ret = syscall1(SYS_sysinfo, (long)info);
    if ((unsigned long)ret >= (unsigned long)-4095) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int rmdir(const char *pathname) {
    long ret = syscall1(SYS_rmdir, (long)pathname);
    SYSCALL_RETURN(ret);
    return 0;
}

int chdir(const char *pathname) {
    long ret = syscall1(SYS_chdir, (long)pathname);
    SYSCALL_RETURN(ret);
    return 0;
}

int getpid(void) {
    return (int)syscall0(SYS_getpid);
}

int setuid(uid_t uid) {
    long ret = syscall1(SYS_setuid, uid);
    SYSCALL_RETURN(ret);
    return 0;
}

int setgid(gid_t gid) {
    long ret = syscall1(SYS_setgid, gid);
    SYSCALL_RETURN(ret);
    return 0;
}

int kill(int pid, int sig) {
    long ret = syscall2(SYS_kill, pid, sig);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

/* Network Wrappers */

int socket(int domain, int type, int protocol) {
    long ret = syscall3(SYS_socket, domain, type, protocol);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    long ret = syscall3(SYS_connect, sockfd, (long)addr, addrlen);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    long ret = syscall3(SYS_bind, sockfd, (long)addr, addrlen);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int listen(int sockfd, int backlog) {
    long ret = syscall2(SYS_listen, sockfd, backlog);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    long ret = syscall3(SYS_accept, sockfd, (long)addr, (long)addrlen);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    /* Use SYS_sendto with NULL dest address and 0 addrlen */
    long ret = syscall6(SYS_sendto, sockfd, (long)buf, len, flags, 0, 0);
    SYSCALL_RETURN(ret);
    return (ssize_t)ret;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    /* Use SYS_recvfrom with NULL src address and NULL addrlen */
    long ret = syscall6(SYS_recvfrom, sockfd, (long)buf, len, flags, 0, 0);
    SYSCALL_RETURN(ret);
    return (ssize_t)ret;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    long ret = syscall6(SYS_sendto, sockfd, (long)buf, len, flags, (long)dest_addr, addrlen);
    SYSCALL_RETURN(ret);
    return (ssize_t)ret;
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    long ret = syscall6(SYS_recvfrom, sockfd, (long)buf, len, flags, (long)src_addr, (long)addrlen);
    SYSCALL_RETURN(ret);
    return (ssize_t)ret;
}


int fsync(int fd) {
    long ret = syscall1(SYS_fsync, fd);
    SYSCALL_RETURN(ret);
    return 0;
}

int fdatasync(int fd) {
    long ret = syscall1(SYS_fdatasync, fd);
    SYSCALL_RETURN(ret);
    return 0;
}

int chown(const char *pathname, uid_t owner, gid_t group) {
    long ret = syscall3(SYS_chown, (long)pathname, owner, group);
    SYSCALL_RETURN(ret);
    return 0;
}

int fchown(int fd, uid_t owner, gid_t group) {
    long ret = syscall3(SYS_fchown, fd, owner, group);
    SYSCALL_RETURN(ret);
    return 0;
}

int lchown(const char *pathname, uid_t owner, gid_t group) {
    long ret = syscall3(SYS_lchown, (long)pathname, owner, group);
    SYSCALL_RETURN(ret);
    return 0;
}



int fchdir(int fd) {
    long ret = syscall1(SYS_fchdir, fd);
    SYSCALL_RETURN(ret);
    return 0;
}

int truncate(const char *path, int64_t length) {
    long ret = syscall2(SYS_truncate, (long)path, length);
    SYSCALL_RETURN(ret);
    return 0;
}

ssize_t pread(int fd, void *buf, size_t count, int64_t offset) {
    long ret = syscall4(SYS_pread64, fd, (long)buf, count, offset);
    SYSCALL_RETURN(ret);
    return (ssize_t)ret;
}

ssize_t pwrite(int fd, const void *buf, size_t count, int64_t offset) {
    long ret = syscall4(SYS_pwrite64, fd, (long)buf, count, offset);
    SYSCALL_RETURN(ret);
    return (ssize_t)ret;
}

ssize_t preadv(int fd, const struct iovec *iov, int iovcnt, int64_t offset) {
    long ret = syscall4(SYS_preadv, fd, (long)iov, iovcnt, offset);
    SYSCALL_RETURN(ret);
    return (ssize_t)ret;
}

ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, int64_t offset) {
    long ret = syscall4(SYS_pwritev, fd, (long)iov, iovcnt, offset);
    SYSCALL_RETURN(ret);
    return (ssize_t)ret;
}

pid_t vfork(void) {
    long ret = syscall0(SYS_vfork);
    SYSCALL_RETURN(ret);
    return (pid_t)ret;
}

int reboot(int magic, int magic2, int cmd, void *arg) {
    long ret = syscall4(SYS_reboot, magic, magic2, cmd, (long)arg);
    SYSCALL_RETURN(ret);
    return 0;
}

int execveat(int dirfd, const char *pathname, char *const argv[], char *const envp[], int flags) {
    long ret = syscall5(SYS_execveat, dirfd, (long)pathname, (long)argv, (long)envp, flags);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return (int)ret;
}

int preadv2(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags) {
    long ret = syscall5(SYS_preadv2, fd, (long)iov, iovcnt, offset, flags);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return (int)ret;
}
int getrusage(int who, struct rusage *usage) {
    long ret = syscall2(SYS_getrusage, who, (long)usage);
    SYSCALL_RETURN(ret);
    return 0;
}
int getrandom(void *buf, size_t buflen, unsigned int flags) {
    long ret = syscall3(SYS_getrandom, (long)buf, buflen, flags);
    SYSCALL_RETURN(ret);
    return (int)ret;
}
int getgroups(int size, gid_t list[]) {
    long ret = syscall2(SYS_getgroups, size, (long)list);
    SYSCALL_RETURN(ret);
    return (int)ret;
}
int setgroups(size_t size, const gid_t *list) {
    long ret = syscall2(SYS_setgroups, size, (long)list);
    SYSCALL_RETURN(ret);
    return (int)ret;
}
void syslog(int priority, const char *format, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    syscall3(SYS_syslog, priority, (long)buf, strlen(buf));
}

int msync(void *addr, size_t length, int flags) {
    long ret = syscall3(SYS_msync, (long)addr, length, flags);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int mincore(void *addr, size_t length, unsigned char *vec) {
    long ret = syscall3(SYS_mincore, (long)addr, length, (long)vec);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int shmget(int key, size_t size, int shmflg) {
    long ret = syscall3(SYS_shmget, key, size, shmflg);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

void *shmat(int shmid, const void *shmaddr, int shmflg) {
    long ret = syscall3(SYS_shmat, shmid, (long)shmaddr, shmflg);
    if ((unsigned long)ret >= (unsigned long)-4095) {
        errno = -ret;
        return (void *)-1;
    }
    return (void *)ret;
}

int shmctl(int shmid, int cmd, void *buf) {
    long ret = syscall3(SYS_shmctl, shmid, cmd, (long)buf);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int getitimer(int which, void *curr_value) {
    long ret = syscall2(SYS_getitimer, which, (long)curr_value);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int semop(int semid, void *sops, size_t nsops) {
    long ret = syscall3(SYS_semop, semid, (long)sops, nsops);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int semctl(int semid, int semnum, int cmd, ...) {
    long arg;
    va_list ap;
    va_start(ap, cmd);
    arg = va_arg(ap, long);
    va_end(ap);
    long ret = syscall4(SYS_semctl, semid, semnum, cmd, arg);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int shmdt(const void *shmaddr) {
    long ret = syscall1(SYS_shmdt, (long)shmaddr);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int msgget(int key, int msgflg) {
    long ret = syscall2(SYS_msgget, key, msgflg);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg) {
    long ret = syscall4(SYS_msgsnd, msqid, (long)msgp, msgsz, msgflg);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg) {
    long ret = syscall5(SYS_msgrcv, msqid, (long)msgp, msgsz, msgtyp, msgflg);
    SYSCALL_RETURN(ret);
    return (ssize_t)ret;
}

int msgctl(int msqid, int cmd, void *buf) {
    long ret = syscall3(SYS_msgctl, msqid, cmd, (long)buf);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int flock(int fd, int operation) {
    long ret = syscall2(SYS_flock, fd, operation);
    SYSCALL_RETURN(ret);
    return (int)ret;
}


int ptrace(long request, long pid, unsigned long addr, unsigned long data) {
    long ret = syscall4(SYS_ptrace, request, pid, addr, data);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int setfsuid(uid_t fsuid) {
    long ret = syscall1(SYS_setfsuid, fsuid);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int setfsgid(gid_t fsgid) {
    long ret = syscall1(SYS_setfsgid, fsgid);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int uselib(const char *library) {
    long ret = syscall1(SYS_uselib, (long)library);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int personality(unsigned long persona) {
    long ret = syscall1(SYS_personality, persona);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int getpriority(int which, int who) {
    long ret = syscall2(SYS_getpriority, which, who);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int sched_setparam(int pid, const void *param) {
    long ret = syscall2(SYS_sched_setparam, pid, (long)param);
    SYSCALL_RETURN(ret);
    return (int)ret;
}

int sched_getparam(int pid, void *param) {
    long ret = syscall2(SYS_sched_getparam, pid, (long)param);
    SYSCALL_RETURN(ret);
    return (int)ret;
}
