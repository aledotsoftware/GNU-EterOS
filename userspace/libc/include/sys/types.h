#ifndef _ETEROS_SYS_TYPES_H
#define _ETEROS_SYS_TYPES_H

#include <stdint.h>

#ifdef __ETEROS_HOST_TEST__
#include <sys/types.h>
#else
typedef int32_t ssize_t;
typedef uint64_t size_t;
typedef int pid_t;
typedef int64_t off_t;
typedef int mode_t;
#endif

#endif /* _ETEROS_SYS_TYPES_H */
