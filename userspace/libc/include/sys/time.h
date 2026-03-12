#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <stdint.h>
#include <sys/types.h>

#ifndef _STRUCT_TIMEVAL
#define _STRUCT_TIMEVAL
struct timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};
#endif

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int gettimeofday(struct timeval *tv, struct timezone *tz);

#endif /* _SYS_TIME_H */
