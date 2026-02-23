#ifndef ETEROS_TIME_H
#define ETEROS_TIME_H

#include "types.h"

#ifndef __ETEROS_HOST_TEST__
/**
 * Representa un tiempo en segundos y nanosegundos (POSIX compliance).
 * time_t es 64-bit por diseño en éterOS.
 */
struct timespec {
    time_t tv_sec;  /* Segundos */
    long   tv_nsec; /* Nanosegundos [0, 999999999] */
};

/**
 * Representa un tiempo en segundos y microsegundos.
 */
struct timeval {
    time_t tv_sec;  /* Segundos */
    int32_t tv_usec; /* Microsegundos */
};
#endif

#endif /* ETEROS_TIME_H */
