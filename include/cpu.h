#ifndef ETEROS_CPU_H
#define ETEROS_CPU_H

#include "types.h"

typedef struct {
    uint64_t kernel_stack; /* Top of kernel stack for syscall entry */
    uint64_t user_stack;   /* Scratch space for user RSP during syscall entry */
} per_cpu_t;

extern per_cpu_t per_cpu_data;

#endif /* ETEROS_CPU_H */
