#ifndef ETEROS_IOCTL_H
#define ETEROS_IOCTL_H

/* Common IOCTL Commands */

/* Terminal IOCTLs (termios) */
#define TCGETS  0x5401
#define TCSETS  0x5402

/* Window Size */
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414

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
