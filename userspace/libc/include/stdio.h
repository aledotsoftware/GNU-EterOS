#ifndef _STDIO_H
#define _STDIO_H

#include <stdint.h>

/* Very basic printf support */
int printf(const char *format, ...);
int dprintf(int fd, const char *format, ...);

#endif /* _STDIO_H */
