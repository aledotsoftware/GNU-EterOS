#ifndef _ETEROS_ASSERT_H
#define _ETEROS_ASSERT_H

#ifdef __ETEROS_HOST_TEST__
#include_next <assert.h>
#undef assert
#endif

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) ((expr) ? (void)0 : eteros_assert_fail(#expr, __FILE__, __LINE__, __func__))
#endif

void eteros_assert_fail(const char *expr, const char *file, int line, const char *func) __attribute__((noreturn));

#endif /* _ETEROS_ASSERT_H */
