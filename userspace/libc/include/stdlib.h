#ifndef _ETEROS_STDLIB_H
#define _ETEROS_STDLIB_H

#include <stdint.h>

#ifdef __ETEROS_HOST_TEST__
#include <stdlib.h>
#define atoi eteros_atoi
#define atol eteros_atol
#define rand eteros_rand
#define srand eteros_srand
#define abort eteros_abort
#define malloc eteros_malloc
#define free eteros_free
#define calloc eteros_calloc
#define realloc eteros_realloc
#endif

#ifndef __ETEROS_HOST_TEST__
typedef uint64_t size_t;
#endif

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
#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* _ETEROS_STDLIB_H */
