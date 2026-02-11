/**
 * éterOS - Funciones de Cadena y Memoria
 * 
 * Implementaciones básicas de funciones de manipulación de
 * cadenas y memoria para un entorno freestanding.
 */

#ifndef ETEROS_STRING_H
#define ETEROS_STRING_H

#include "types.h"

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
 * Copia hasta n caracteres de src a dest.
 */
char* strncpy(char* dest, const char* src, size_t n);

/**
 * Copia una cadena a un buffer de tamaño limitado, asegurando terminación null.
 */
size_t strlcpy(char* dest, const char* src, size_t size);

/**
 * Compara dos cadenas.
 * Retorna 0 si son iguales.
 */
int strcmp(const char* s1, const char* s2);

/**
 * Convierte un entero a su representación en cadena.
 *
 * @param buffer_size Tamaño del buffer de destino.
 */
void itoa_s(int64_t value, char* buffer, size_t buffer_size, int base);

/**
 * Convierte un entero sin signo a su representación en cadena hexadecimal (sin comprobación de límites).
 */
void utoa_hex(uint64_t value, char* buffer);

/**
 * Convierte un entero sin signo a su representación en cadena hexadecimal.
 *
 * @param buffer_size Tamaño del buffer de destino.
 */
void utoa_hex_s(uint64_t value, char* buffer, size_t buffer_size);

#endif /* ETEROS_STRING_H */
