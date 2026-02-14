/**
 * éterOS - Definiciones de Tipos Básicos
 * 
 * Tipos de datos fundamentales para un entorno freestanding
 * donde no están disponibles las cabeceras estándar de C.
 */

#ifndef ETEROS_TYPES_H
#define ETEROS_TYPES_H

#ifdef __ETEROS_HOST_TEST__
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#else

/* ========================================================================= */
/* Tipos enteros con tamaño fijo                                             */
/* ========================================================================= */
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;

/* ========================================================================= */
/* Límites de tipos enteros                                                  */
/* ========================================================================= */
#define INT8_MIN   (-128)
#define INT16_MIN  (-32768)
#define INT32_MIN  (-2147483648)
#define INT64_MIN  (-9223372036854775807LL - 1)

#define INT8_MAX   127
#define INT16_MAX  32767
#define INT32_MAX  2147483647
#define INT64_MAX  9223372036854775807LL

#define UINT8_MAX  255
#define UINT16_MAX 65535
#define UINT32_MAX 4294967295U
#define UINT64_MAX 18446744073709551615ULL

#ifdef __x86_64__
#define SIZE_MAX  UINT64_MAX
#define SSIZE_MAX INT64_MAX
#else
#define SIZE_MAX  UINT32_MAX
#define SSIZE_MAX INT32_MAX
#endif

/* ========================================================================= */
/* Tipos de tamaño de plataforma                                             */
/* ========================================================================= */
#ifdef __x86_64__
typedef uint64_t            size_t;
typedef int64_t             ssize_t;
typedef uint64_t            uintptr_t;
typedef int64_t             intptr_t;
#else
typedef uint32_t            size_t;
typedef int32_t             ssize_t;
typedef uint32_t            uintptr_t;
typedef int32_t             intptr_t;
#endif

/* ========================================================================= */
/* Tipo booleano                                                             */
/* ========================================================================= */
/* GCC 15+ / C23: bool, true, false son keywords nativas.                    */
/* Para versiones anteriores, definimos manualmente.                          */
#if !defined(__bool_true_false_are_defined) && !defined(bool)
#  if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
     /* C23: ya están definidos por el compilador */
#  else
     typedef _Bool bool;
#    define true  1
#    define false 0
#  endif
#  define __bool_true_false_are_defined 1
#endif

/* ========================================================================= */
/* Constante NULL                                                            */
/* ========================================================================= */
#define NULL ((void*)0)

#endif /* !__ETEROS_HOST_TEST__ */

#endif /* ETEROS_TYPES_H */
