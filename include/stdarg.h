#ifndef ETEROS_STDARG_H
#define ETEROS_STDARG_H

#ifdef __ETEROS_HOST_TEST__
#if defined(__GNUC__) || defined(__clang__)
#include_next <stdarg.h>
#endif
#else

typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap)         __builtin_va_end(ap)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

#endif

#endif /* ETEROS_STDARG_H */
