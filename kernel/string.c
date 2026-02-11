/**
 * éterOS - Implementación de Funciones de Cadena y Memoria
 * 
 * Implementaciones freestanding de funciones estándar de
 * manipulación de memoria y cadenas.
 */

#include "../include/string.h"

/* ========================================================================= */
/* Funciones de Memoria                                                      */
/* ========================================================================= */

void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    while (n--) {
        *d++ = *s++;
    }
    
    return dest;
}

void* memset(void* dest, int c, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    
    while (n--) {
        *d++ = (uint8_t)c;
    }
    
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    if (d < s) {
        /* Copiar hacia adelante */
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        /* Copiar hacia atrás para manejar solapamiento */
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* a = (const uint8_t*)s1;
    const uint8_t* b = (const uint8_t*)s2;
    
    while (n--) {
        if (*a != *b) {
            return *a - *b;
        }
        a++;
        b++;
    }
    
    return 0;
}

/* ========================================================================= */
/* Funciones de Cadena                                                       */
/* ========================================================================= */

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

size_t strlcpy(char* dest, const char* src, size_t size) {
    size_t i;
    if (size > 0) {
        for (i = 0; i < size - 1 && src[i] != '\0'; i++) {
            dest[i] = src[i];
        }
        dest[i] = '\0';
    }
    return strlen(src);
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/* ========================================================================= */
/* Funciones de Conversión                                                   */
/* ========================================================================= */

void itoa_s(int64_t value, char* buffer, size_t buffer_size, int base) {
    if (buffer_size == 0) return;

    char temp[65];
    int i = 0;
    int is_negative = 0;

    if (value == 0) {
        if (buffer_size > 1) {
            buffer[0] = '0';
            buffer[1] = '\0';
        } else {
            buffer[0] = '\0';
        }
        return;
    }

    /* Manejar números negativos solo en base 10 */
    if (value < 0 && base == 10) {
        is_negative = 1;
        value = -value;
    }

    uint64_t uvalue = (uint64_t)value;
    
    while (uvalue != 0) {
        int remainder = (int)(uvalue % base);
        temp[i++] = (remainder > 9) ? (remainder - 10 + 'A') : (remainder + '0');
        uvalue /= base;
    }

    /* Agregar signo negativo si es necesario */
    if (is_negative) {
        temp[i++] = '-';
    }

    /* Invertir la cadena y copiar con límite */
    int j = 0;
    size_t chars_to_copy = (size_t)i;

    if (chars_to_copy >= buffer_size) {
        chars_to_copy = buffer_size - 1;
    }

    while (chars_to_copy > 0) {
        buffer[j++] = temp[--i];
        chars_to_copy--;
    }
    buffer[j] = '\0';
}

void utoa_hex_s(uint64_t value, char* buffer, size_t buffer_size) {
    /* Verify buffer size to prevent overflow */
    if (buffer_size == 0) return;

    if (buffer_size == 1) {
        buffer[0] = '\0';
        return;
    }

    const char hex_chars[] = "0123456789ABCDEF";
    size_t i = 0;
    
    /* Escribir prefijo "0x" */
    if (i < buffer_size - 1) buffer[i++] = '0';
    if (i < buffer_size - 1) buffer[i++] = 'x';
    
    /* Escribir 16 dígitos hexadecimales */
    for (int shift = 60; shift >= 0; shift -= 4) {
        if (i < buffer_size - 1) {
            buffer[i++] = hex_chars[(value >> shift) & 0xF];
        } else {
            break;
        }
    }
    
    buffer[i] = '\0';
}
