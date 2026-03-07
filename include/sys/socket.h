#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <types.h>

/* Address Families */
#define AF_INET     2
#define AF_INET6    10

/* Socket Types */
#define SOCK_STREAM 1
#define SOCK_DGRAM  2

/* Protocols */
#define IPPROTO_IP  0
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

/* Socket Options / Flags */
#define MSG_DONTWAIT 0x40

typedef int socket_t;
typedef uint32_t socklen_t;

struct sockaddr {
    uint16_t sa_family;
    char     sa_data[14];
};

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t  sin_zero[8];
};

struct iovec { void *iov_base; size_t iov_len; };

struct msghdr {
    void         *msg_name;       /* optional address */
    socklen_t     msg_namelen;    /* size of address */
    struct iovec *msg_iov;        /* scatter/gather array */
    size_t        msg_iovlen;     /* # elements in msg_iov */
    void         *msg_control;    /* ancillary data, see below */
    size_t        msg_controllen; /* ancillary data buffer len */
    int           msg_flags;      /* flags on received message */
};

/* Prototypes */
int socket(int domain, int type, int protocol);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

#endif /* _SYS_SOCKET_H */
