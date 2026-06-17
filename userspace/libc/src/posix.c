/**
 * eterOS Mini-LibC - POSIX extras
 * Wrappers and helpers focused on shell/process compatibility.
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>


extern char **environ;

/* Syscall primitives (self-contained to avoid cross-TU dependencies). */
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

static inline long _syscall5(long n, long a1, long a2, long a3, long a4, long a5) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8) : "rcx", "r11", "memory");
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

static int _set_errno(long ret) {
    if ((unsigned long)ret >= (unsigned long)-4095) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

static const char* _find_path_in_envp(char *const envp[]) {
    if (!envp) return NULL;
    for (int i = 0; envp[i]; ++i) {
        if (strncmp(envp[i], "PATH=", 5) == 0) {
            return envp[i] + 5;
        }
    }
    return NULL;
}

static int _join_path(char *dst, size_t dst_sz, const char *dir, const char *file) {
    size_t i = 0;
    size_t j = 0;

    if (!dst || dst_sz == 0 || !dir || !file) return -1;

    while (dir[i] && i + 1 < dst_sz) {
        dst[i] = dir[i];
        i++;
    }
    if (dir[i] != '\0' && i + 1 >= dst_sz) return -1;

    if (i == 0 || dst[i - 1] != '/') {
        if (i + 1 >= dst_sz) return -1;
        dst[i++] = '/';
    }

    while (file[j] && i + 1 < dst_sz) {
        dst[i++] = file[j++];
    }
    if (file[j] != '\0') return -1;

    dst[i] = '\0';
    return 0;
}

/* Process */
int fork(void) {
    long ret = _syscall0(SYS_fork);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return (int)ret;
}

int execve(const char *pathname, char *const argv[], char *const envp[]) {
    long ret = _syscall3(SYS_execve, (long)pathname, (long)argv, (long)envp);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return (int)ret;
}

int execv(const char *pathname, char *const argv[]) {
    return execve(pathname, argv, environ);
}

int execvpe(const char *file, char *const argv[], char *const envp[]) {
    const char *path_env;
    const char *path;
    int saw_eacces = 0;
    char candidate[512];

    if (!file || !*file) {
        errno = ENOENT;
        return -1;
    }

    if (strchr(file, '/')) {
        return execve(file, argv, envp ? envp : environ);
    }

    path_env = _find_path_in_envp(envp);
    if (!path_env) {
        path_env = getenv("PATH");
    }
    if (!path_env || !*path_env) {
        path_env = "/gnu/bin:/bin:/";
    }

    path = path_env;
    while (*path) {
        const char *sep = strchr(path, ':');
        char dir[256];
        size_t len = sep ? (size_t)(sep - path) : strlen(path);

        if (len == 0) {
            dir[0] = '.';
            dir[1] = '\0';
        } else {
            if (len >= sizeof(dir)) {
                if (!sep) break;
                path = sep + 1;
                continue;
            }
            memcpy(dir, path, len);
            dir[len] = '\0';
        }

        if (_join_path(candidate, sizeof(candidate), dir, file) == 0) {
            execve(candidate, argv, envp ? envp : environ);
            if (errno == EACCES) saw_eacces = 1;
            if (errno != ENOENT && errno != ENOTDIR && errno != EACCES) return -1;
        }

        if (!sep) break;
        path = sep + 1;
    }

    {
        static const char *bb_paths[] = { "/gnu/bin/busybox", "/bin/busybox", "/busybox" };
        for (size_t i = 0; i < sizeof(bb_paths) / sizeof(bb_paths[0]); ++i) {
            execve(bb_paths[i], argv, envp ? envp : environ);
            if (errno == EACCES) {
                saw_eacces = 1;
            } else if (errno != ENOENT && errno != ENOTDIR) {
                return -1;
            }
        }
    }

    errno = saw_eacces ? EACCES : ENOENT;
    return -1;
}

int execvp(const char *file, char *const argv[]) {
    return execvpe(file, argv, environ);
}

pid_t waitpid(pid_t pid, int *status, int options) {
    long ret = _syscall4(SYS_wait4, pid, (long)status, options, 0);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return (pid_t)ret;
}

int waitid(idtype_t idtype, pid_t id, siginfo_t *infop, int options) {
    long ret = _syscall5(SYS_waitid, idtype, id, (long)infop, options, 0);
    return _set_errno(ret);
}

/* File descriptors and filesystem */
int pipe(int pipefd[2]) {
    long ret = _syscall1(SYS_pipe, (long)pipefd);
    return _set_errno(ret);
}

int pipe2(int pipefd[2], int flags) {
    long ret = _syscall2(SYS_pipe2, (long)pipefd, flags);
    return _set_errno(ret);
}

int dup(int oldfd) {
    long ret = _syscall1(SYS_dup, oldfd);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return (int)ret;
}

int dup2(int oldfd, int newfd) {
    long ret = _syscall2(SYS_dup2, oldfd, newfd);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return (int)ret;
}

int dup3(int oldfd, int newfd, int flags) {
    long ret = _syscall3(SYS_dup3, oldfd, newfd, flags);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return (int)ret;
}

int openat(int dirfd, const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    long ret = _syscall4(SYS_openat, dirfd, (long)pathname, flags, mode);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return (int)ret;
}

int fcntl(int fd, int cmd, ...) {
    long arg = 0;
    va_list ap;
    va_start(ap, cmd);
    arg = va_arg(ap, long);
    va_end(ap);

    long ret = _syscall3(SYS_fcntl, fd, cmd, arg);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return (int)ret;
}

int access(const char *pathname, int mode) {
    long ret = _syscall2(SYS_access, (long)pathname, mode);
    return _set_errno(ret);
}

int chmod(const char *pathname, mode_t mode) {
    long ret = _syscall2(SYS_chmod, (long)pathname, mode);
    return _set_errno(ret);
}

int faccessat(int dirfd, const char *pathname, int mode, int flags) {
    long ret = _syscall4(SYS_faccessat, dirfd, (long)pathname, mode, flags);
    return _set_errno(ret);
}

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) {
    long ret = _syscall3(SYS_readlink, (long)pathname, (long)buf, (long)bufsiz);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return (ssize_t)ret;
}

ssize_t readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz) {
    long ret = _syscall4(SYS_readlinkat, dirfd, (long)pathname, (long)buf, (long)bufsiz);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return (ssize_t)ret;
}

int lstat(const char *pathname, struct stat *buf) {
    long ret = _syscall4(SYS_newfstatat, AT_FDCWD, (long)pathname, (long)buf, AT_SYMLINK_NOFOLLOW);
    return _set_errno(ret);
}

int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags) {
    long ret = _syscall4(SYS_newfstatat, dirfd, (long)pathname, (long)buf, flags);
    return _set_errno(ret);
}

char *getcwd(char *buf, size_t size) {
    long ret = _syscall2(SYS_getcwd, (long)buf, (long)size);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return (void*)0; }
    return buf;
}

/* IDs and process groups */
int getppid(void) {
    return (int)_syscall0(SYS_getppid);
}

uid_t getuid(void) {
    return (uid_t)_syscall0(SYS_getuid);
}

gid_t getgid(void) {
    return (gid_t)_syscall0(SYS_getgid);
}

uid_t geteuid(void) {
    return (uid_t)_syscall0(SYS_geteuid);
}

gid_t getegid(void) {
    return (gid_t)_syscall0(SYS_getegid);
}

pid_t setsid(void) {
    long ret = _syscall0(SYS_setsid);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return (pid_t)ret;
}

int setpgid(pid_t pid, pid_t pgid) {
    long ret = _syscall2(SYS_setpgid, pid, pgid);
    return _set_errno(ret);
}

pid_t getpgrp(void) {
    long ret = _syscall0(SYS_getpgrp);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return (pid_t)ret;
}

int tcgetpgrp(int fd) {
    int pgid = 0;
    if (ioctl(fd, TIOCGPGRP, &pgid) < 0) return -1;
    return pgid;
}

int tcsetpgrp(int fd, pid_t pgrp) {
    int pg = (int)pgrp;
    return ioctl(fd, TIOCSPGRP, &pg);
}

/* Scheduling */
int sched_yield(void) {
    long ret = _syscall0(SYS_sched_yield);
    return _set_errno(ret);
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    struct timespec ts;
    struct timespec* tsp = NULL;
    long ret;

    if (timeout) {
        ts.tv_sec = timeout->tv_sec;
        ts.tv_nsec = timeout->tv_usec * 1000;
        tsp = &ts;
    }

    ret = _syscall5(SYS_select, nfds, (long)readfds, (long)writefds, (long)exceptfds, (long)tsp);
    if ((unsigned long)ret >= (unsigned long)-4095) {
        errno = (int)(-ret);
        return -1;
    }
    return (int)ret;
}

int epoll_create1(int flags) {
    long ret = _syscall1(SYS_epoll_create1, flags);
    return _set_errno(ret) == 0 ? (int)ret : -1;
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    long ret = _syscall4(SYS_epoll_ctl, epfd, op, fd, (long)event);
    return _set_errno(ret);
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    long ret = _syscall4(SYS_epoll_wait, epfd, (long)events, maxevents, timeout);
    return _set_errno(ret) == 0 ? (int)ret : -1;
}

/* Memory */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, int64_t offset) {
    long ret = _syscall6(SYS_mmap, (long)addr, (long)length, prot, flags, fd, offset);
    if ((unsigned long)ret >= (unsigned long)-4095) {
        errno = (int)(-ret);
        return (void*)-1;
    }
    return (void*)ret;
}

int munmap(void *addr, size_t length) {
    long ret = _syscall2(SYS_munmap, (long)addr, (long)length);
    return _set_errno(ret);
}

int mprotect(void *addr, size_t len, int prot) {
    long ret = _syscall3(SYS_mprotect, (long)addr, (long)len, prot);
    return _set_errno(ret);
}

void *mremap(void *old_addr, size_t old_size, size_t new_size, int flags, ...) {
    void* new_addr = 0;
    if (flags & MREMAP_FIXED) {
        va_list ap;
        va_start(ap, flags);
        new_addr = va_arg(ap, void*);
        va_end(ap);
    }

    {
        long ret = _syscall5(SYS_mremap, (long)old_addr, (long)old_size, (long)new_size, flags, (long)new_addr);
        if ((unsigned long)ret >= (unsigned long)-4095) {
            errno = (int)(-ret);
            return (void*)-1;
        }
        return (void*)ret;
    }
}

void *brk(void *addr) {
    long ret = _syscall1(SYS_brk, (long)addr);
    return (void*)ret;
}

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

/* Time */
unsigned int sleep(unsigned int seconds) {
    struct { int64_t sec; int64_t nsec; } req = {seconds, 0};
    struct { int64_t sec; int64_t nsec; } rem = {0, 0};
    long ret = _syscall2(SYS_nanosleep, (long)&req, (long)&rem);
    if ((unsigned long)ret >= (unsigned long)-4095) return (unsigned int)rem.sec;
    return 0;
}

int usleep(unsigned int usec) {
    struct { int64_t sec; int64_t nsec; } req;
    req.sec = usec / 1000000;
    req.nsec = (usec % 1000000) * 1000;
    long ret = _syscall2(SYS_nanosleep, (long)&req, 0);
    return _set_errno(ret);
}

/* Terminal */
int tcgetattr(int fd, struct termios *termios_p) {
    long ret = _syscall3(SYS_ioctl, fd, TCGETS, (long)termios_p);
    return _set_errno(ret);
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    (void)optional_actions; /* Only TCSANOW is supported by kernel TTY currently. */
    long ret = _syscall3(SYS_ioctl, fd, TCSETS, (long)termios_p);
    return _set_errno(ret);
}

int isatty(int fd) {
    struct termios term;
    int saved = errno;
    if (tcgetattr(fd, &term) == 0) {
        return 1;
    }
    if (errno == ENOTTY || errno == EINVAL) {
        errno = saved;
        return 0;
    }
    return 0;
}

/* Misc */
int uname(void *buf) {
    long ret = _syscall1(SYS_uname, (long)buf);
    return _set_errno(ret);
}

mode_t umask(mode_t mask) {
    long ret = _syscall1(SYS_umask, mask);
    if ((unsigned long)ret >= (unsigned long)-4095) {
        errno = (int)(-ret);
        return (mode_t)-1;
    }
    return (mode_t)ret;
}

/* POSIX shared memory helpers (minimal path translation). */
int shm_open(const char *name, int oflag, mode_t mode) {
    char path[256];
    const char* prefix = "/dev/shm/";
    const char* n_ptr;
    size_t i = 0;

    if (!name) {
        errno = EINVAL;
        return -1;
    }

    memset(path, 0, sizeof(path));
    while (prefix[i] && i < sizeof(path) - 1) {
        path[i] = prefix[i];
        i++;
    }

    n_ptr = (name[0] == '/') ? name + 1 : name;
    while (*n_ptr && i < sizeof(path) - 1) {
        path[i++] = *n_ptr++;
    }
    path[i] = '\0';

    return open(path, oflag, mode);
}

int ftruncate(int fd, int64_t length) {
    long ret = _syscall2(SYS_ftruncate, fd, length);
    return _set_errno(ret);
}

int shm_unlink(const char *name) {
    char path[256];
    const char* prefix = "/dev/shm/";
    const char* n_ptr;
    size_t i = 0;

    if (!name) {
        errno = EINVAL;
        return -1;
    }

    memset(path, 0, sizeof(path));
    while (prefix[i] && i < sizeof(path) - 1) {
        path[i] = prefix[i];
        i++;
    }

    n_ptr = (name[0] == '/') ? name + 1 : name;
    while (*n_ptr && i < sizeof(path) - 1) {
        path[i++] = *n_ptr++;
    }
    path[i] = '\0';

    return unlink(path);
}

/* -------------------------------------------------------------------------
 * getopt Implementation
 * ------------------------------------------------------------------------- */
char *optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = 0;

int getopt(int argc, char * const argv[], const char *optstring) {
    static char *nextchar = NULL;
    char c;
    char *cp;

    if (optind == 0) {
        optind = 1;
        nextchar = NULL;
    }

    if (nextchar == NULL || *nextchar == '\0') {
        if (optind >= argc || argv[optind] == NULL || argv[optind][0] != '-' || argv[optind][1] == '\0') {
            return -1;
        }
        if (argv[optind][1] == '-' && argv[optind][2] == '\0') {
            optind++;
            return -1;
        }
        nextchar = argv[optind] + 1;
    }

    c = *nextchar++;
    optopt = c;

    if (c == ':' || (cp = strchr(optstring, c)) == NULL) {
        if (opterr && *optstring != ':') {
            /* Minimal error reporting */
        }
        if (*nextchar == '\0') {
            optind++;
        }
        return '?';
    }

    if (cp[1] == ':') {
        if (*nextchar != '\0') {
            optarg = nextchar;
            optind++;
        } else {
            if (optind + 1 >= argc) {
                if (opterr && *optstring != ':') {
                    /* Missing argument */
                }
                optarg = NULL;
                optopt = c;
                if (*optstring == ':') return ':';
                return '?';
            } else {
                optarg = argv[++optind];
                optind++;
            }
        }
        nextchar = NULL;
    } else {
        if (*nextchar == '\0') {
            optind++;
        }
        optarg = NULL;
    }
    return c;
}
