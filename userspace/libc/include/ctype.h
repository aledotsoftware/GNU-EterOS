#ifndef _ETEROS_CTYPE_H
#define _ETEROS_CTYPE_H

#ifdef __ETEROS_HOST_TEST__
#include_next <ctype.h>

#undef isalnum
#define isalnum eteros_isalnum
#undef isalpha
#define isalpha eteros_isalpha
#undef iscntrl
#define iscntrl eteros_iscntrl
#undef isdigit
#define isdigit eteros_isdigit
#undef isgraph
#define isgraph eteros_isgraph
#undef islower
#define islower eteros_islower
#undef isprint
#define isprint eteros_isprint
#undef ispunct
#define ispunct eteros_ispunct
#undef isspace
#define isspace eteros_isspace
#undef isupper
#define isupper eteros_isupper
#undef isxdigit
#define isxdigit eteros_isxdigit
#undef tolower
#define tolower eteros_tolower
#undef toupper
#define toupper eteros_toupper
#endif

int isalnum(int c);
int isalpha(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);
int tolower(int c);
int toupper(int c);

#endif /* _ETEROS_CTYPE_H */
