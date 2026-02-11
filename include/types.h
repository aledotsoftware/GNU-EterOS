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
/* Tipos de tamaño de plataforma                                             */
/* ========================================================================= */
typedef uint64_t            size_t;
typedef int64_t             ssize_t;
typedef uint64_t            uintptr_t;
typedef int64_t             intptr_t;

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
