#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#include <sys/types.h>

#define TCGETS 0x5401
#define TCSETS 0x5402
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define FIONREAD 0x541B
#define FIONBIO 0x5421

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

int ioctl(int fd, int request, void *arg);

#endif
