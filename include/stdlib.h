#ifndef ETEROS_STDLIB_H
#define ETEROS_STDLIB_H

#ifdef __ETEROS_HOST_TEST__
#include_next <stdlib.h>
/* Rename conflicting functions if any */
#define atoi eteros_atoi
#endif

#include "types.h"

int atoi(const char* str);

#endif /* ETEROS_STDLIB_H */
