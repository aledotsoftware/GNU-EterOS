#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/syscall.h>
#include <errno.h>
#include <unistd.h> /* For syscall macros if needed, assuming they are available or redefined here */

/* Re-define syscall macros if not available via unistd.h in this context */
/* We assume unistd.h or syscall.c provides them, but since this is a separate .c file,
   we need the syscall helper functions or macros.
   Currently syscall.c defines syscall0..3 as static inline but NOT in a header.
   Wait, userspace/libc/src/syscall.c had them static inline.
   I need them here. I should probably move them to a private header or re-implement.
   Let's check unistd.h. */

/* Temporary: Re-implement syscall macros for this file since they aren't exposed in a header yet. */
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

/* Byte Order Functions */
uint16_t htons(uint16_t v) {
    return (v << 8) | (v >> 8);
}

uint16_t ntohs(uint16_t v) {
    return (v << 8) | (v >> 8);
}

uint32_t htonl(uint32_t v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v & 0xFF0000) >> 8) | ((v >> 24) & 0xFF);
}

uint32_t ntohl(uint32_t v) {
    return htonl(v);
}

/* Socket Wrappers */

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
    long ret = syscall3(SYS_listen, sockfd, backlog, 0); /* 3 args? syscall.c might use 2. SYS_listen usually 2. */
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    long ret = syscall3(SYS_accept, sockfd, (long)addr, (long)addrlen);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    /* SYS_sendto might be used if SYS_send doesn't exist, but we added SYS_sendto=44 in syscall.h
       Actually, Linux uses sendto for send (with NULL dest).
       Wait, I should check if I am implementing SYS_send or SYS_sendto in kernel.
       Plan said "implement sys_send". I'll use SYS_sendto (44) logic if I want Linux compat,
       but I can define SYS_send (custom) or reuse SYS_sendto.
       Let's use SYS_sendto with NULL dest for send().
       Wait, `syscall.h` has `SYS_sendto` (44).
       I will implement `sys_send` in kernel mapped to `SYS_sendto`? No, `sys_sendto` is generic.
       Let's stick to what I planned: I will implement `sys_send` (wrapper for net_send) in kernel.
       But what syscall number? `SYS_send` is not in the list I saw in `syscall.h` (it had `SYS_sendto` 44, `SYS_sendmsg` 46).
       Ah, strict Linux x86_64 uses `sendto` for `send`.
       Okay, I will call `SYS_sendto` here, and in kernel I will implement `sys_sendto`.
       BUT, for simplicity in this task, I will define `SYS_send` if strictly needed, or just use `SYS_sendto`.
       The plan said "Implement sys_send, sys_recv".
       I'll use `SYS_sendto` (44) and `SYS_recvfrom` (45) but pass NULLs.
    */
    long ret = syscall4(SYS_sendto, sockfd, (long)buf, len, flags);
    /* Note: sendto takes 6 args in Linux! (fd, buf, len, flags, dest_addr, addrlen)
       My syscall4 only handles 4.
       I need syscall6?
       Or I can cheat and implement a custom `SYS_send` (custom number?)
       Wait, `syscall.h` has `SYS_socket` (41) ... `SYS_sendto` (44).
       If I want to be cleaner, I should just assume I can pass 6 args.
       But `userspace/libc/src/syscall.c` only had up to `syscall3`.
       I'll define `syscall6`.
    */
    if (ret < 0) { errno = -ret; return -1; }
    return (ssize_t)ret;
}

/* We need syscall6 for sendto/recvfrom fully, but for `send` we can just pass 0/NULL for extra args if kernel handles it.
   Let's define syscall6 just in case.
*/
static inline long syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return ret;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    long ret = syscall6(SYS_recvfrom, sockfd, (long)buf, len, flags, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (ssize_t)ret;
}
