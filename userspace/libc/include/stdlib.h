#ifndef _ETEROS_STDLIB_H
#define _ETEROS_STDLIB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __ETEROS_HOST_TEST__
#include_next <stdlib.h>

#undef atoi
#define atoi eteros_atoi
#undef atol
#define atol eteros_atol
#undef strtol
#define strtol eteros_strtol
#undef strtoul
#define strtoul eteros_strtoul
#undef strtoll
#define strtoll eteros_strtoll
#undef strtoull
#define strtoull eteros_strtoull

#undef rand
#define rand eteros_rand
#undef srand
#define srand eteros_srand
#undef abort
#define abort eteros_abort
#undef malloc
#define malloc eteros_malloc
#undef free
#define free eteros_free
#undef calloc
#define calloc eteros_calloc
#undef realloc
#define realloc eteros_realloc
#undef exit
#define exit eteros_exit

#undef abs
#define abs eteros_abs
#undef labs
#define labs eteros_labs
#undef llabs
#define llabs eteros_llabs
#undef div
#define div eteros_div
#undef ldiv
#define ldiv eteros_ldiv
#undef lldiv
#define lldiv eteros_lldiv

#undef bsearch
#define bsearch eteros_bsearch
#undef qsort
#define qsort eteros_qsort

#undef getenv
#define getenv eteros_getenv
#undef setenv
#define setenv eteros_setenv
#undef unsetenv
#define unsetenv eteros_unsetenv
#undef putenv
#define putenv eteros_putenv
#endif

#ifndef __ETEROS_HOST_TEST__
typedef uint64_t size_t;

typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;
#endif

extern char **environ;

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
long  strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
long long strtoll(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);

/* Math */
int abs(int j);
long labs(long j);
long long llabs(long long j);
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);

/* Search and Sort */
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

/* Pseudo-random */
#ifndef RAND_MAX
#define RAND_MAX 0x7fffffff
#endif
int   rand(void);
void  srand(unsigned int seed);

/* Environment */
char *getenv(const char *name);
int   setenv(const char *name, const char *value, int overwrite);
int   unsetenv(const char *name);
int   putenv(char *string);

void  abort(void) __attribute__((noreturn));

int   system(const char *command);
int   atexit(void (*func)(void));

/* Misc */
#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* _ETEROS_STDLIB_H */
