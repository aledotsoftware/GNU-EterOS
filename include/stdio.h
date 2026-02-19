#ifndef ETEROS_STDIO_H
#define ETEROS_STDIO_H

#include <stdarg.h>
#include <types.h>

#ifdef __ETEROS_HOST_TEST__
/* Include standard stdio.h to avoid shadowing it completely when using -Iinclude */
#if defined(__GNUC__) || defined(__clang__)
#include_next <stdio.h>
#endif

/* Rename functions to avoid conflict with standard library during testing */
#define vsnprintf eteros_vsnprintf
#define snprintf eteros_snprintf
#define kprintf eteros_kprintf
#endif

/*
 * Formats a string and prints it to the kernel console.
 */
int kprintf(const char* format, ...);

/*
 * Formats a string into a buffer with a size limit.
 * Guaranteed to null-terminate the buffer if size > 0.
 * Returns the number of characters that would have been written if buffer was large enough.
 */
int vsnprintf(char* str, size_t size, const char* format, va_list ap);

/*
 * Formats a string into a buffer with a size limit.
 * Guaranteed to null-terminate the buffer if size > 0.
 * Returns the number of characters that would have been written if buffer was large enough.
 */
int snprintf(char* str, size_t size, const char* format, ...);

#endif /* ETEROS_STDIO_H */
