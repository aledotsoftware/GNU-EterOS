/**
 * éterOS - Implementación de Funciones de Cadena y Memoria
 * 
 * Implementaciones freestanding de funciones estándar de
 * manipulación de memoria y cadenas.
 */

#include "../include/string.h"
#include "../include/stdlib.h"

/* ========================================================================= */
/* Funciones de Memoria                                                      */
/* ========================================================================= */

void* memcpy(void* dest, const void* src, size_t n) {
#ifdef __x86_64__
    void* original_dest = dest;
    size_t qwords = n / 8;
    size_t remainder = n % 8;

    __asm__ volatile (
        "cld; rep movsq"
        : "+D"(dest), "+S"(src), "+c"(qwords)
        :
        : "memory"
    );

    __asm__ volatile (
        "rep movsb"
        : "+D"(dest), "+S"(src), "+c"(remainder)
        :
        : "memory"
    );
    return original_dest;
#else
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
    return dest;
#endif
}

void* memset(void* dest, int c, size_t n) {
#ifdef __x86_64__
    void* original_dest = dest;
    uint64_t val = (uint8_t)c;
    /* ⚡ BOLT Optimization: Use multiplication for pattern generation (faster/smaller than 7 shifts/ORs) */
    uint64_t pattern = val * 0x0101010101010101ULL;
    
    size_t qwords = n / 8;
    size_t remainder = n % 8;

    __asm__ volatile (
        "cld; rep stosq"
        : "+D"(dest), "+c"(qwords)
        : "a"(pattern)
        : "memory"
    );

    __asm__ volatile (
        "rep stosb"
        : "+D"(dest), "+c"(remainder)
        : "a"(val)
        : "memory"
    );
    return original_dest;
#else
    uint8_t* d = (uint8_t*)dest;
    while (n--) *d++ = (uint8_t)c;
    return dest;
#endif
}

void* memset16(void* dest, uint16_t c, size_t n) {
#ifdef __x86_64__
    void* original_dest = dest;
    __asm__ volatile (
        "cld; rep stosw"
        : "+D"(dest), "+c"(n)
        : "a"(c)
        : "memory"
    );
    return original_dest;
#else
    uint16_t* d = (uint16_t*)dest;
    while (n--) *d++ = c;
    return dest;
#endif
}

void* memset32(void* dest, uint32_t c, size_t n) {
#ifdef __x86_64__
    void* original_dest = dest;
    /* ⚡ BOLT Optimization: Use rep stosq (64-bit) for bulk of memset32. */
    /* This processes 8 bytes per instruction instead of 4. */
    uint64_t pattern = c | ((uint64_t)c << 32);
    size_t qwords = n / 2;

    __asm__ volatile (
        "cld; rep stosq"
        : "+D"(dest), "+c"(qwords)
        : "a"(pattern)
        : "memory"
    );

    /* Handle remainder if n is odd */
    if (n & 1) {
        __asm__ volatile (
            "stosl"
            : "+D"(dest)
            : "a"(c)
            : "memory"
        );
    }
    return original_dest;
#else
    uint32_t* d = (uint32_t*)dest;
    while (n--) *d++ = c;
    return dest;
#endif
}

void explicit_bzero(void* s, size_t n) {
    memset(s, 0, n);
    /* Barrier to prevent compiler from optimizing away the memset */
    __asm__ volatile("" : : "r"(s) : "memory");
}

void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    if (d == s || n == 0) return dest;

#ifdef __x86_64__
    if (d < s) {
        size_t qwords = n / 8;
        size_t remainder = n % 8;

        __asm__ volatile (
            "cld; rep movsq"
            : "+S"(s), "+D"(d), "+c"(qwords)
            : : "memory"
        );

        __asm__ volatile (
            "rep movsb"
            : "+S"(s), "+D"(d), "+c"(remainder)
            : : "memory"
        );
    } else {
        s += n - 1;
        d += n - 1;

        size_t qwords = n / 8;
        size_t remainder = n % 8;

        /* ⚡ BOLT Optimization: Use rep movsq (64-bit) for backward copy.
           This provides ~8x speedup for large overlapping moves (e.g. scrolling). */
        __asm__ volatile (
            "std\n\t"
            "rep movsb\n\t"       /* Copy remainder bytes (high to low) */
            "sub $7, %%rdi\n\t"   /* Adjust pointers to start of QWORD */
            "sub $7, %%rsi\n\t"
            "mov %3, %%rcx\n\t"   /* Load QWORD count */
            "rep movsq\n\t"       /* Copy QWORDS (high to low) */
            "cld"                 /* Restore DF */
            : "+S"(s), "+D"(d), "+c"(remainder)
            : "r"(qwords)
            : "memory"
        );

        /* Copy 8-byte chunks (high to low) using qword copy */
        if (qwords > 0) {
            /* Adjust pointers: standard 'movsq' with DF=1 reads 8 bytes starting at [RSI]
               (i.e., [RSI]...[RSI+7]) then decrements RSI by 8.
               Current s/d point to the highest byte of the next block. We need to point
               to the lowest byte of that 8-byte block. */
            s -= 7;
            d -= 7;
            __asm__ volatile (
                "std; rep movsq"
                : "+S"(s), "+D"(d), "+c"(qwords)
                : : "memory"
            );
        }

        /* Restore Direction Flag (required by ABI) */
        __asm__ volatile ("cld" ::: "memory");
    }
    return dest;
#else
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
#endif
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* a = (const uint8_t*)s1;
    const uint8_t* b = (const uint8_t*)s2;

#ifdef __x86_64__
    // Optimization for x86_64: Compare 8-byte blocks
    // This significantly reduces the number of iterations for large blocks.
    // We use uint64_t to compare 8 bytes at a time.
    while (n >= 8) {
        if (*(const uint64_t*)a != *(const uint64_t*)b) {
            // If there is a difference, break the fast loop
            // and let the byte-by-byte loop find the exact position
            // to return the correct value (sign).
            break;
        }
        a += 8;
        b += 8;
        n -= 8;
    }
#endif
    
#ifndef __x86_64__
    /* Optimization: Compare 8 bytes at a time if pointers are aligned */
    /* On x86_64, the unaligned loop above handles this, so we skip it to avoid redundancy. */
    if (((uintptr_t)a & 7) == 0 && ((uintptr_t)b & 7) == 0) {
        while (n >= 8) {
            if (*(const uint64_t*)a != *(const uint64_t*)b) {
                break;
            }
            a += 8;
            b += 8;
            n -= 8;
        }
    }
#endif

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

/**
 * NOTA DE SEGURIDAD:
 * La función estándar `strcpy` ha sido eliminada intencionalmente de este
 * codebase debido a su vulnerabilidad inherente a desbordamientos de búfer.
 * Se debe utilizar `strlcpy` en su lugar, la cual garantiza la terminación
 * nula y respeta el tamaño del búfer de destino.
 */

size_t strlen(const char* str) {
#ifdef __x86_64__
    const char* char_ptr;
    const uint64_t* longword_ptr;
    uint64_t longword, himagic, lomagic;

    /* Handle unaligned bytes */
    for (char_ptr = str; ((uintptr_t)char_ptr & 7) != 0; ++char_ptr) {
        if (*char_ptr == '\0')
            return char_ptr - str;
    }

    /* Process 8 bytes at a time */
    longword_ptr = (const uint64_t*)char_ptr;
    himagic = 0x8080808080808080ULL;
    lomagic = 0x0101010101010101ULL;

    while (1) {
        longword = *longword_ptr++;

        if (((longword - lomagic) & ~longword & himagic) != 0) {
            const char* cp = (const char*)(longword_ptr - 1);
            if (cp[0] == 0) return cp - str;
            if (cp[1] == 0) return cp - str + 1;
            if (cp[2] == 0) return cp - str + 2;
            if (cp[3] == 0) return cp - str + 3;
            if (cp[4] == 0) return cp - str + 4;
            if (cp[5] == 0) return cp - str + 5;
            if (cp[6] == 0) return cp - str + 6;
            if (cp[7] == 0) return cp - str + 7;
        }
    }
#else
    const char *s = str;

#ifdef __x86_64__
    /*
     * Optimized SWAR (SIMD Within A Register) implementation for x86_64.
     * Processes 8 bytes at a time using 64-bit operations.
     */

    /* 1. Align to 8 bytes boundary */
    while ((uintptr_t)s & 7) {
        if (!*s) return s - str;
        s++;
    }

    /* 2. Process 8 bytes at a time */
    const uint64_t *ls = (const uint64_t *)s;
    uint64_t v;

    while (1) {
        v = *ls++;
        /* Check for null byte using bit magic:
           (v - 0x01...) & ~v & 0x80... detects if any byte is 0 */
        if (((v - 0x0101010101010101UL) & ~v & 0x8080808080808080UL)) {
            const char *p = (const char *)(ls - 1);
            if (!p[0]) return p - str;
            if (!p[1]) return p - str + 1;
            if (!p[2]) return p - str + 2;
            if (!p[3]) return p - str + 3;
            if (!p[4]) return p - str + 4;
            if (!p[5]) return p - str + 5;
            if (!p[6]) return p - str + 6;
            return p - str + 7;
        }
    }
#else
    /*
     * Unroll loop (4x) to reduce branching overhead.
     * This is faster than byte-by-byte and safer/simpler than word-at-a-time (SWAR)
     * which requires strict aliasing care.
     */
    while (1) {
        if (!s[0]) return s - str;
        if (!s[1]) return s - str + 1;
        if (!s[2]) return s - str + 2;
        if (!s[3]) return s - str + 3;
        s += 4;
    }
#endif
#endif
}

size_t strnlen(const char* s, size_t maxlen) {
    const char* p;
    for (p = s; maxlen-- && *p; p++)
        ;
    return p - s;
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
    /* ⚡ BOLT Optimization: Use optimized strlen (SWAR) + memcpy (rep movsq)
       instead of slow byte-by-byte copy loop. */
    size_t len = strlen(src);
    if (size > 0) {
        size_t to_copy = (len >= size) ? size - 1 : len;
        memcpy(dest, src, to_copy);
        dest[to_copy] = '\0';
    }
    return len;
}

size_t strlcat(char* dest, const char* src, size_t size) {
    size_t dlen = strlen(dest);
    if (dlen >= size) {
        return size + strlen(src);
    }
    return dlen + strlcpy(dest + dlen, src, size - dlen);
}

int strcmp(const char* s1, const char* s2) {
#ifdef __x86_64__
    /* Optimization: Compare 8 bytes at a time if pointers are aligned */
    typedef uint64_t __attribute__((__may_alias__)) u64_alias;

    /* Align s1 to 8 bytes */
    while (((uintptr_t)s1 & 7) != 0) {
        if (*s1 != *s2) {
            return *(const unsigned char*)s1 - *(const unsigned char*)s2;
        }
        if (*s1 == '\0') {
            return 0;
        }
        s1++;
        s2++;
    }

    /* If s2 is also aligned, we can do 64-bit comparison */
    if (((uintptr_t)s2 & 7) == 0) {
        const u64_alias* ls1 = (const u64_alias*)s1;
        const u64_alias* ls2 = (const u64_alias*)s2;
        uint64_t v1, v2;

        while (1) {
            v1 = *ls1;
            v2 = *ls2;

            /* Check for null terminator in v1 using standard bit trick:
             * (v1 - 0x01...) & ~v1 & 0x80... detects zero byte.
             * Also check if words differ.
             */
            if (((v1 - 0x0101010101010101ULL) & ~v1 & 0x8080808080808080ULL) || (v1 != v2)) {
                /* Mismatch or null terminator found.
                 * Break to byte loop to find exact location. */
                s1 = (const char*)ls1;
                s2 = (const char*)ls2;
                break;
            }

            ls1++;
            ls2++;
        }
    }
#endif

    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char* strchr(const char *s, int c) {
#ifdef __x86_64__
    char ch = (char)c;

    /* Special case: Finding null terminator */
    if (ch == 0) {
        return (char*)s + strlen(s);
    }

    /* Align to 8 bytes */
    while (((uintptr_t)s & 7) != 0) {
        if (*s == ch) return (char *)s;
        if (*s == '\0') return 0;
        s++;
    }

    /* Process 8 bytes at a time */
    const uint64_t* ls = (const uint64_t*)s;
    uint64_t v;
    uint64_t ch_pattern = (uint8_t)ch * 0x0101010101010101ULL;

    while (1) {
        v = *ls;

        /* Check for zero byte (end of string) */
        uint64_t zero_mask = (v - 0x0101010101010101ULL) & ~v & 0x8080808080808080ULL;

        /* Check for target char */
        uint64_t xor_v = v ^ ch_pattern;
        uint64_t match_mask = (xor_v - 0x0101010101010101ULL) & ~xor_v & 0x8080808080808080ULL;

        if (zero_mask | match_mask) {
            /* Found something. Locate exact position. */
            s = (const char*)ls;
            /* Unrolled loop for remainder */
            if (s[0] == ch) return (char*)s; if (s[0] == 0) return 0;
            if (s[1] == ch) return (char*)s + 1; if (s[1] == 0) return 0;
            if (s[2] == ch) return (char*)s + 2; if (s[2] == 0) return 0;
            if (s[3] == ch) return (char*)s + 3; if (s[3] == 0) return 0;
            if (s[4] == ch) return (char*)s + 4; if (s[4] == 0) return 0;
            if (s[5] == ch) return (char*)s + 5; if (s[5] == 0) return 0;
            if (s[6] == ch) return (char*)s + 6; if (s[6] == 0) return 0;
            if (s[7] == ch) return (char*)s + 7; if (s[7] == 0) return 0;

            /* Should not reach here if masks were correct */
            return 0;
        }

        ls++;
    }
#else
    while (*s != (char)c) {
        if (!*s++) {
            return 0;
        }
    }
    return (char *)s;
#endif
}

int strncmp(const char* s1, const char* s2, size_t n) {
    if (n == 0) return 0;

#ifdef __x86_64__
    /* Optimization: Compare 8 bytes at a time if pointers are aligned */
    typedef uint64_t __attribute__((__may_alias__)) u64_alias;

    /* Align s1 to 8 bytes */
    while (n > 0 && ((uintptr_t)s1 & 7) != 0) {
        if (*s1 != *s2) {
            return *(const unsigned char*)s1 - *(const unsigned char*)s2;
        }
        if (*s1 == '\0') {
            return 0;
        }
        s1++;
        s2++;
        n--;
    }

    /* If s2 is also aligned, we can do 64-bit comparison */
    if (n >= 8 && ((uintptr_t)s2 & 7) == 0) {
        const u64_alias* ls1 = (const u64_alias*)s1;
        const u64_alias* ls2 = (const u64_alias*)s2;
        uint64_t v1, v2;

        while (n >= 8) {
            v1 = *ls1;
            v2 = *ls2;

            /* Check for null terminator in v1 using standard bit trick:
             * (v1 - 0x01...) & ~v1 & 0x80... detects zero byte.
             * Also check if words differ.
             */
            if (((v1 - 0x0101010101010101ULL) & ~v1 & 0x8080808080808080ULL) || (v1 != v2)) {
                /* Mismatch or null terminator found.
                 * Break to byte loop to find exact location. */
                break;
            }

            ls1++;
            ls2++;
            n -= 8;
        }
        s1 = (const char*)ls1;
        s2 = (const char*)ls2;
    }
#endif

    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/* ========================================================================= */
/* Funciones de Libreria Estandar (Simuladas)                                */
/* ========================================================================= */

int atoi(const char* str) {
    int res = 0;
    int sign = 1;

    /* Skip whitespace */
    while (*str == ' ' || (*str >= 9 && *str <= 13)) str++;

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return sign * res;
}

int atoi_s(const char* str, int32_t* out) {
    if (!str || !out) return -1;

    int64_t res = 0;
    int sign = 1;

    /* Skip whitespace */
    while (*str == ' ' || (*str >= 9 && *str <= 13)) str++;

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    if (!*str) return -1; /* No digits or empty string */

    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');

        if (sign == 1 && res > INT32_MAX) return -2;
        if (sign == -1 && -res < INT32_MIN) return -2;

        str++;
    }

    if (*str != '\0') return -3; /* Invalid char */

    *out = (int32_t)(sign * res);
    return 0;
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

    uint64_t uvalue;

    /* Manejar números negativos solo en base 10.
       Se utiliza aritmética sin signo para manejar INT64_MIN correctamente.
       La negación directa (-value) causaría desbordamiento si value es INT64_MIN.
       Al castear a uint64_t primero, la operación es segura y bien definida. */
    if (value < 0 && base == 10) {
        is_negative = 1;
        uvalue = -(uint64_t)value;
    } else {
        uvalue = (uint64_t)value;
    }
    
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
