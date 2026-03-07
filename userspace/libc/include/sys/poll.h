#ifndef _SYS_POLL_H
#define _SYS_POLL_H
#include <stdint.h>
struct pollfd {
    int   fd;         /* file descriptor */
    short events;     /* requested events */
    short revents;    /* returned events */
};
#define POLLIN      0x0001
#define POLLPRI     0x0002
#define POLLOUT     0x0004
#define POLLERR     0x0008
#define POLLHUP     0x0010
#define POLLNVAL    0x0020
int poll(struct pollfd *fds, uint32_t nfds, int timeout);
#endif
