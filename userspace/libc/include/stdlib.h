#ifndef _STDLIB_H
#define _STDLIB_H

#include <stdint.h>

typedef uint64_t size_t;

/* Process termination */
void exit(int status) __attribute__((noreturn));

/* Memory allocation */
void *malloc(size_t size);
void  free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

/* String conversion */
int   atoi(const char *nptr);
long  atol(const char *nptr);

/* Pseudo-random */
#define RAND_MAX 0x7fffffff
int   rand(void);
void  srand(unsigned int seed);

/* Environment */
void  abort(void) __attribute__((noreturn));

/* Misc */
#define NULL ((void *)0)

#endif /* _STDLIB_H */
