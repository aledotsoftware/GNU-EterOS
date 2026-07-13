#ifndef _SYS_RANDOM_H
#define _SYS_RANDOM_H

#include <stdint.h>
#include <sys/types.h>

#define GRND_NONBLOCK 0x01
#define GRND_RANDOM   0x02

ssize_t getrandom(void *buf, size_t buflen, unsigned int flags);

#endif
