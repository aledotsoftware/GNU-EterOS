#ifndef _ETEROS_TIME_H
#define _ETEROS_TIME_H

#include <stdint.h>
#include <stddef.h>

#ifdef __ETEROS_HOST_TEST__
#include_next <time.h>

#undef nanosleep
#define nanosleep eteros_nanosleep
#undef clock_gettime
#define clock_gettime eteros_clock_gettime
#undef time
#define time eteros_time
#undef gmtime
#define gmtime eteros_gmtime
#undef gmtime_r
#define gmtime_r eteros_gmtime_r
#undef mktime
#define mktime eteros_mktime
#undef strftime
#define strftime eteros_strftime
#undef asctime
#define asctime eteros_asctime
#undef ctime
#define ctime eteros_ctime
#undef difftime
#define difftime eteros_difftime
#endif

#ifndef __ETEROS_HOST_TEST__
typedef int64_t time_t;
typedef int clockid_t;

struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
    long tm_gmtoff;
    const char *tm_zone;
};

/* Clock IDs */
#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3
#endif

#ifdef __ETEROS_HOST_TEST__
int eteros_nanosleep(const struct timespec *req, struct timespec *rem);
int eteros_clock_gettime(int clock_id, struct timespec *tp);
time_t eteros_time(time_t *tloc);
struct tm *eteros_gmtime(const time_t *timep);
struct tm *eteros_gmtime_r(const time_t *timep, struct tm *result);
time_t eteros_mktime(struct tm *tm);
size_t eteros_strftime(char *s, size_t max, const char *format, const struct tm *tm);
char *eteros_asctime(const struct tm *tm);
char *eteros_ctime(const time_t *timep);
double eteros_difftime(time_t time1, time_t time0);
#else
int nanosleep(const struct timespec *req, struct timespec *rem);
int clock_gettime(int clock_id, struct timespec *tp);
time_t time(time_t *tloc);
struct tm *gmtime(const time_t *timep);
struct tm *gmtime_r(const time_t *timep, struct tm *result);
time_t mktime(struct tm *tm);
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
char *asctime(const struct tm *tm);
char *ctime(const time_t *timep);
double difftime(time_t time1, time_t time0);
#endif

#endif /* _ETEROS_TIME_H */
