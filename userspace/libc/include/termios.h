#ifndef ETEROS_USER_TERMIOS_H
#define ETEROS_USER_TERMIOS_H

#include <sys/types.h>

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;

#define NCCS 32

#define ISIG   00000001
#define ICANON 00000002
#define ECHO   00000010

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
    unsigned int c_ispeed;
    unsigned int c_ospeed;
};

#endif
