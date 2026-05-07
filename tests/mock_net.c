#include <sys/types.h>
#include <sys/socket.h>
#include <net/lwip_socket.h>

int sys_lwip_socket(int domain, int type, int protocol) { return -1; }
int sys_lwip_connect(int fd, const struct sockaddr *name, socklen_t namelen) { return -1; }
int sys_lwip_bind(int fd, const struct sockaddr *name, socklen_t namelen) { return -1; }
int sys_lwip_accept(int fd, struct sockaddr *addr, socklen_t *addrlen) { return -1; }
int sys_lwip_listen(int fd, int backlog) { return -1; }
ssize_t sys_lwip_sendto(int fd, const void *data, size_t size, int flags, const struct sockaddr *to, socklen_t tolen) { return -1; }
ssize_t sys_lwip_recvfrom(int fd, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen) { return -1; }
ssize_t sys_lwip_sendmsg(int fd, const struct msghdr *msg, int flags) { return -1; }
ssize_t sys_lwip_recvmsg(int fd, struct msghdr *msg, int flags) { return -1; }
int sys_lwip_shutdown(int fd, int how) { return -1; }
int sys_lwip_getsockname(int fd, struct sockaddr *name, socklen_t *namelen) { return -1; }
int sys_lwip_getpeername(int fd, struct sockaddr *name, socklen_t *namelen) { return -1; }
int sys_lwip_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen) { return -1; }
int sys_lwip_getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen) { return -1; }
int net_gethostbyname(const char* hostname, uint32_t* out_ip) { return -1; }
