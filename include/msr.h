#ifndef ETEROS_MSR_H
#define ETEROS_MSR_H

#include "types.h"

/* Model Specific Registers */
#define MSR_EFER            0xC0000080
#define MSR_STAR            0xC0000081
#define MSR_LSTAR           0xC0000082
#define MSR_CSTAR           0xC0000083
#define MSR_SFMASK          0xC0000084
#define MSR_FS_BASE         0xC0000100
#define MSR_GS_BASE         0xC0000101
#define MSR_KERNEL_GS_BASE  0xC0000102

/* EFER Bits */
#define EFER_SCE            0x00000001 /* System Call Extensions */

/* Funciones helper para leer/escribir MSRs */

#ifndef __ETEROS_HOST_TEST__
static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}
#else
/* Declaraciones para tests */
void wrmsr(uint32_t msr, uint64_t value);
uint64_t rdmsr(uint32_t msr);
#endif

#endif /* ETEROS_MSR_H */
