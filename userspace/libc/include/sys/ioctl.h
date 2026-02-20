#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#include <sys/types.h>

#define TCGETS 0x5401
#define TCSETS 0x5402

int ioctl(int fd, int request, void *arg);

#endif
