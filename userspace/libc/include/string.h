#ifndef _ETEROS_STRING_H
#define _ETEROS_STRING_H

#include <stdint.h>

#ifdef __ETEROS_HOST_TEST__
#include <string.h>

#define memcpy eteros_memcpy
#define memset eteros_memset
#define memmove eteros_memmove
#define memcmp eteros_memcmp
#define strlen eteros_strlen
#define strcmp eteros_strcmp
#define strncmp eteros_strncmp
#define strnlen eteros_strnlen
#define strncpy eteros_strncpy
#define strlcpy eteros_strlcpy
#define strlcat eteros_strlcat
#define strchr eteros_strchr
#define strrchr eteros_strrchr
#define strstr eteros_strstr
#define strtok eteros_strtok
#endif

#ifndef __ETEROS_HOST_TEST__
typedef uint64_t size_t;
#endif

void  *memcpy(void *dest, const void *src, size_t n);
void  *memset(void *s, int c, size_t n);
void  *memmove(void *dest, const void *src, size_t n);
int    memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
int    strcmp(const char *s1, const char *s2);
int    strncmp(const char *s1, const char *s2, size_t n);
char  *strncpy(char *dest, const char *src, size_t n);
size_t strlcpy(char *dest, const char *src, size_t size);
size_t strlcat(char *dest, const char *src, size_t size);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *haystack, const char *needle);
char  *strtok(char *str, const char *delim);

#endif /* _ETEROS_STRING_H */
