#include <time.h>
#include <errno.h>
#include <sys/syscall.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>

#ifdef __ETEROS_HOST_TEST__
// In host test mode, we might want to just call host functions or mock them.
// But we are wrapping them.
// The issue is that the compiler sees standard function signatures in time.h (from host)
// and our implementation here conflicts or uses types that are slightly different.
// Ideally, for host test, we should rely on host libc for time functions, OR
// completely reimplement them but ensuring types match.
// Given we used include_next <time.h>, we have host types.
// Our implementation uses struct tm fields, which should exist in host struct tm.

// However, we are getting "undefined type struct tm" errors because in time.h we only forward declared it or
// the include_next didn't expose the definition when we are in __ETEROS_HOST_TEST__.

// Actually, in userspace/libc/include/time.h, when __ETEROS_HOST_TEST__ is defined,
// we include_next <time.h>. That should bring in struct tm.
// But we also #undef gmtime etc.
#endif

#ifndef SYS_clock_gettime
#define SYS_clock_gettime 228
#endif

/* Syscall primitives - Renamed to avoid conflict if included with stdio.c */
static inline long _time_syscall1(long n, long a1) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static inline long _time_syscall2(long n, long a1, long a2) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

time_t time(time_t *tloc) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) < 0) return -1;
    if (tloc) *tloc = ts.tv_sec;
    return ts.tv_sec;
}

int clock_gettime(int clock_id, struct timespec *tp) {
    long ret = _time_syscall2(SYS_clock_gettime, clock_id, (long)tp);
    if ((unsigned long)ret >= (unsigned long)-4095) {
        // Fallback to gettimeofday if clock_gettime fails
        if (ret == -ENOSYS && clock_id == CLOCK_REALTIME) {
             struct {
                long tv_sec;
                long tv_usec;
             } tv;
             long ret2 = _time_syscall2(96, (long)&tv, 0); // gettimeofday
             if ((unsigned long)ret2 >= (unsigned long)-4095) { errno = (int)(-ret2); return -1; }
             tp->tv_sec = tv.tv_sec;
             tp->tv_nsec = tv.tv_usec * 1000;
             return 0;
        }
        errno = (int)(-ret); return -1;
    }
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    long ret = _time_syscall2(SYS_nanosleep, (long)req, (long)rem);
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return 0;
}

// Helpers for date/time conversion
#define SECS_PER_MIN  60
#define SECS_PER_HOUR 3600
#define SECS_PER_DAY  86400
#define EPOCH_YEAR 1970

static int is_leap(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

static const int mdays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

struct tm *gmtime_r(const time_t *timep, struct tm *result) {
    time_t t = *timep;
    long days;
    int rem, y;
    const int *ip;

    days = t / SECS_PER_DAY;
    rem = t % SECS_PER_DAY;
    if (rem < 0) {
        rem += SECS_PER_DAY;
        days--;
    }

    result->tm_hour = rem / SECS_PER_HOUR;
    rem %= SECS_PER_HOUR;
    result->tm_min = rem / SECS_PER_MIN;
    result->tm_sec = rem % SECS_PER_MIN;

    result->tm_wday = (days + 4) % 7; // 1970-01-01 was Thursday (4)
    if (result->tm_wday < 0) result->tm_wday += 7;

    y = EPOCH_YEAR;
    while (days < 0 || days >= (is_leap(y) ? 366 : 365)) {
        long yd = is_leap(y) ? 366 : 365;
        if (days >= yd) {
            days -= yd;
            y++;
        } else {
            y--;
            days += is_leap(y) ? 366 : 365;
        }
    }
    result->tm_year = y - 1900;
    result->tm_yday = days;

    ip = mdays;
    int leap = is_leap(y);
    for (int m = 1; m < 12; m++) {
        int d = ip[m];
        if (m == 2 && leap) d++;
        if (days < d) {
            result->tm_mon = m - 1;
            result->tm_mday = days + 1;
            break;
        }
        days -= d;
        if (m == 11) {
             result->tm_mon = 11;
             result->tm_mday = days + 1;
        }
    }

    result->tm_isdst = 0;
    result->tm_gmtoff = 0;
    result->tm_zone = "UTC";

    return result;
}

struct tm *localtime(const time_t *timep) {
    // For now, assume UTC. A full implementation would check TZ environment variable
    return gmtime(timep);
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
    long ret = _time_syscall2(96, (long)tv, (long)tz); // SYS_gettimeofday
    if ((unsigned long)ret >= (unsigned long)-4095) { errno = (int)(-ret); return -1; }
    return 0;
}

struct tm *gmtime(const time_t *timep) {
    static struct tm tm_buf;
    return gmtime_r(timep, &tm_buf);
}

time_t mktime(struct tm *tm) {
    time_t t = 0;
    int year = tm->tm_year + 1900;
    int mon = tm->tm_mon;

    while (mon < 0) { year--; mon += 12; }
    while (mon > 11) { year++; mon -= 12; }

    for (int y = 1970; y < year; y++) t += is_leap(y) ? 366 : 365;

    int leap = is_leap(year);
    for (int m = 0; m < mon; m++) {
        int d = mdays[m+1];
        if (m == 1 && leap) d++;
        t += d;
    }

    t += tm->tm_mday - 1;
    t *= 24; t += tm->tm_hour;
    t *= 60; t += tm->tm_min;
    t *= 60; t += tm->tm_sec;

    return t;
}

char *asctime(const struct tm *tm) {
    static char buf[26];
    const char *wday_name[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char *mon_name[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    snprintf(buf, sizeof(buf), "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
        wday_name[tm->tm_wday % 7],
        mon_name[tm->tm_mon % 12],
        tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec,
        tm->tm_year + 1900);
    return buf;
}

char *ctime(const time_t *timep) {
    return asctime(gmtime(timep));
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    (void)format;
    (void)tm;
    if (max > 0) *s = '\0';
    return 0;
}

double difftime(time_t time1, time_t time0) {
    return (double)(time1 - time0);
}
