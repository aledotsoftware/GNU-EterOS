#ifndef _NET_LWIP_SOCKET_H
#define _NET_LWIP_SOCKET_H

#include <types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/select.h>

int sys_lwip_socket(int domain, int type, int protocol);
int sys_lwip_close(int fd);
int sys_lwip_bind(int fd, const struct sockaddr *name, socklen_t namelen);
int sys_lwip_listen(int fd, int backlog);
int sys_lwip_accept(int fd, struct sockaddr *addr, socklen_t *addrlen);
int sys_lwip_connect(int fd, const struct sockaddr *name, socklen_t namelen);
ssize_t sys_lwip_sendto(int fd, const void *data, size_t size, int flags, const struct sockaddr *to, socklen_t tolen);
ssize_t sys_lwip_recvfrom(int fd, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
int sys_lwip_poll(struct pollfd *fds, uint32_t nfds, int timeout);
int sys_lwip_select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout);
int sys_lwip_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen);
int sys_lwip_getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen);
int sys_lwip_getpeername(int fd, struct sockaddr *name, socklen_t *namelen);
int sys_lwip_getsockname(int fd, struct sockaddr *name, socklen_t *namelen);
int sys_lwip_shutdown(int fd, int how);
ssize_t sys_lwip_sendmsg(int fd, const struct msghdr *msg, int flags);
ssize_t sys_lwip_recvmsg(int fd, struct msghdr *msg, int flags);
ssize_t sys_lwip_send(int fd, const void *buf, size_t len, int flags);
ssize_t sys_lwip_recv(int fd, void *buf, size_t len, int flags);

#endif /* _NET_LWIP_SOCKET_H */
