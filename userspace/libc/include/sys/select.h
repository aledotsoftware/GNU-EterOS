#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H
#include <stdint.h>

#define FD_SETSIZE 1024

typedef struct {
    uint64_t fds_bits[FD_SETSIZE / 64];
} fd_set;

#define FD_ZERO(set)    do { for(int __i = 0; __i < FD_SETSIZE / 64; __i++) ((fd_set *)(set))->fds_bits[__i] = 0; } while(0)
#define FD_SET(fd, set) do { ((fd_set *)(set))->fds_bits[(fd) / 64] |= (1ULL << ((fd) % 64)); } while(0)
#define FD_CLR(fd, set) do { ((fd_set *)(set))->fds_bits[(fd) / 64] &= ~(1ULL << ((fd) % 64)); } while(0)
#define FD_ISSET(fd, set) (((fd_set *)(set))->fds_bits[(fd) / 64] & (1ULL << ((fd) % 64)))

struct timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
#endif
