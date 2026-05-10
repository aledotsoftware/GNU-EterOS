#ifndef ETEROS_IOCTL_H
#define ETEROS_IOCTL_H

/* Common IOCTL Commands */

/* Terminal IOCTLs (termios) */
#define TCGETS  0x5401
#define TCSETS  0x5402

/* Window Size */
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define TIOCGPGRP  0x540F
#define TIOCSPGRP  0x5410
#define TIOCSCTTY  0x540E
#define TIOCNOTTY  0x5422
#define TIOCGPTN   0x80045430
#define TIOCSPTLCK 0x40045431

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

/* Standard File IOCTLs */
#define FIONREAD 0x541B
#define FIONBIO 0x5421

#endif /* ETEROS_IOCTL_H */
