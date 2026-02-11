/**
 * éterOS - libgcc compatibility functions for 32-bit builds
 * 
 * Provides 64-bit arithmetic primitives required by GCC when compiling
 * for 32-bit targets without standard library support.
 */

#include "../include/types.h"

#ifndef __x86_64__

/* unsigned 64-bit division */
uint64_t __udivdi3(uint64_t n, uint64_t d) {
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r <<= 1;
        if ((n >> i) & 1) r |= 1;
        if (r >= d) {
            r -= d;
            q |= (1ULL << i);
        }
    }
    return q;
}

/* unsigned 64-bit modulo */
uint64_t __umoddi3(uint64_t n, uint64_t d) {
    uint64_t r = 0;
    for (int i = 63; i >= 0; i--) {
        r <<= 1;
        if ((n >> i) & 1) r |= 1;
        if (r >= d) r -= d;
    }
    return r;
}

/* unsigned 64-bit division with remainder */
uint64_t __udivmoddi4(uint64_t n, uint64_t d, uint64_t *rem) {
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r <<= 1;
        if ((n >> i) & 1) r |= 1;
        if (r >= d) {
            r -= d;
            q |= (1ULL << i);
        }
    }
    if (rem) *rem = r;
    return q;
}

/* signed 64-bit division */
int64_t __divdi3(int64_t n, int64_t d) {
    int neg = (n < 0) ^ (d < 0);
    uint64_t q = __udivdi3(n < 0 ? -n : n, d < 0 ? -d : d);
    return neg ? -q : q;
}

/* signed 64-bit modulo */
int64_t __moddi3(int64_t n, int64_t d) {
    int neg = (n < 0);
    uint64_t r = __umoddi3(n < 0 ? -n : n, d < 0 ? -d : d);
    return neg ? -r : r;
}

#endif
