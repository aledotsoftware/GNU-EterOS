/**
 * éterOS - Funciones de Cadena y Memoria
 * 
 * Implementaciones básicas de funciones de manipulación de
 * cadenas y memoria para un entorno freestanding.
 */

#ifndef ETEROS_STRING_H
#define ETEROS_STRING_H

#include "types.h"

#ifdef __ETEROS_HOST_TEST__
#include_next <string.h>
/* Rename functions to avoid conflict with standard library during testing */
#define memcpy eteros_memcpy
#define memset eteros_memset
#define memmove eteros_memmove
#define memcmp eteros_memcmp
#define strlen eteros_strlen
#define strncpy eteros_strncpy
#define strlcpy eteros_strlcpy
#define strlcat eteros_strlcat
#define strnlen eteros_strnlen
#define strcmp eteros_strcmp
#define strchr eteros_strchr
#define strncmp eteros_strncmp
#define explicit_bzero eteros_explicit_bzero
#endif

/* ========================================================================= */
/* Funciones de Memoria                                                      */
/* ========================================================================= */

/**
 * Copia n bytes de src a dest.
 */
void* memcpy(void* dest, const void* src, size_t n);

/**
 * Establece n bytes de dest al valor c.
 */
void* memset(void* dest, int c, size_t n);

/**
 * Establece n palabras de 16 bits de dest al valor c.
 */
void* memset16(void* dest, uint16_t c, size_t n);

/**
 * Establece n dwords de 32 bits de dest al valor c.
 */
void* memset32(void* dest, uint32_t c, size_t n);

/**
 * Limpia n bytes de memoria de forma segura (previene optimización del compilador).
 */
void explicit_bzero(void* s, size_t n);

/**
 * Mueve n bytes de src a dest (soporta solapamiento).
 */
void* memmove(void* dest, const void* src, size_t n);

/**
 * Compara n bytes de dos bloques de memoria.
 * Retorna 0 si son iguales.
 */
int memcmp(const void* s1, const void* s2, size_t n);

/* ========================================================================= */
/* Funciones de Cadena                                                       */
/* ========================================================================= */

/**
 * Retorna la longitud de una cadena terminada en null.
 */
size_t strlen(const char* str);

/**
 * Retorna la longitud de una cadena hasta maxlen.
 */
size_t strnlen(const char* str, size_t maxlen);

/**
 * Copia hasta n caracteres de src a dest.
 */
char* strncpy(char* dest, const char* src, size_t n);

/**
 * Copia una cadena a un buffer de tamaño limitado, asegurando terminación null.
 */
size_t strlcpy(char* dest, const char* src, size_t size);

/**
 * Concatena src al final de dest, asegurando terminación null.
 */
size_t strlcat(char* dest, const char* src, size_t size);

/**
 * Compara dos cadenas.
 * Retorna 0 si son iguales.
 */
int strcmp(const char* s1, const char* s2);

/**
 * Encuentra la primera ocurrencia de c en s.
 */
char* strchr(const char *s, int c);

/**
 * Compara hasta n caracteres de dos cadenas.
 */
int strncmp(const char* s1, const char* s2, size_t n);

/**
 * Convierte un entero a su representación en cadena.
 *
 * @param buffer_size Tamaño del buffer de destino.
 */
int atoi(const char* str);

/**
 * Safe conversion of string to 32-bit signed integer.
 * Checks for overflow and invalid characters.
 * Returns 0 on success, -1 on invalid input, -2 on overflow/underflow.
 *
 * @param str The string to convert.
 * @param out Pointer to store the result.
 */
int atoi_s(const char* str, int32_t* out);

void itoa_s(int64_t value, char* buffer, size_t buffer_size, int base);

/**
 * Convierte un entero sin signo a su representación en cadena hexadecimal.
 *
 * @param buffer_size Tamaño del buffer de destino.
 */
void utoa_hex_s(uint64_t value, char* buffer, size_t buffer_size);

#endif /* ETEROS_STRING_H */
