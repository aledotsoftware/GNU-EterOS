#ifndef ETEROS_STDLIB_H
#define ETEROS_STDLIB_H

#ifdef __ETEROS_HOST_TEST__
#if defined(__GNUC__) || defined(__clang__)
#include_next <stdlib.h>
#endif
#endif

#include "types.h"

int atoi(const char* str);

#endif /* ETEROS_STDLIB_H */
