#ifndef _ETEROS_STRING_H
#define _ETEROS_STRING_H

#include <stdint.h>

#ifdef __ETEROS_HOST_TEST__
#include_next <string.h>

#undef memcpy
#define memcpy eteros_memcpy
#undef memset
#define memset eteros_memset
#undef memmove
#define memmove eteros_memmove
#undef memcmp
#define memcmp eteros_memcmp
#undef strlen
#define strlen eteros_strlen
#undef strcmp
#define strcmp eteros_strcmp
#undef strncmp
#define strncmp eteros_strncmp
#undef strnlen
#define strnlen eteros_strnlen
#undef strncpy
#define strncpy eteros_strncpy
#undef strlcpy
#define strlcpy eteros_strlcpy
#undef strlcat
#define strlcat eteros_strlcat
#undef strchr
#define strchr eteros_strchr
#undef strrchr
#define strrchr eteros_strrchr
#undef strstr
#define strstr eteros_strstr
#undef strtok
#define strtok eteros_strtok
#undef strcpy
#define strcpy eteros_strcpy

#undef strdup
#define strdup eteros_strdup
#undef strndup
#define strndup eteros_strndup
#undef strspn
#define strspn eteros_strspn
#undef strcspn
#define strcspn eteros_strcspn
#undef strpbrk
#define strpbrk eteros_strpbrk
#undef strerror
#define strerror eteros_strerror
#undef strcoll
#define strcoll eteros_strcoll
#undef strxfrm
#define strxfrm eteros_strxfrm
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
char  *strtok_r(char *str, const char *delim, char **saveptr);
char  *strcpy(char *dest, const char *src);

char  *strdup(const char *s);
char  *strndup(const char *s, size_t n);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char  *strpbrk(const char *s, const char *accept);
char  *strsep(char **stringp, const char *delim);
int    strncasecmp(const char *s1, const char *s2, size_t n);
void  *memrchr(const void *s, int c, size_t n);
char  *strerror(int errnum);
int    strcoll(const char *s1, const char *s2);
size_t strxfrm(char *dest, const char *src, size_t n);

#endif /* _ETEROS_STRING_H */
