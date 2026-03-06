/**
 * éterOS Mini-LibC - String Functions
 * musl-compatible memory and string operations
 */

#include <string.h>
#include <stdint.h>

void *memcpy(void *dest, const void *src, size_t n) {
#ifdef __x86_64__
    void *original_dest = dest;
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    /* ⚡ BOLT Optimization: Fast path for small blocks (< 64 bytes) to avoid
       the setup overhead of the `rep` microcode on modern x86_64. */
    if (n < 64) {
        while (n >= 8) {
            *(uint64_t *)d = *(const uint64_t *)s;
            d += 8; s += 8; n -= 8;
        }
        if (n >= 4) {
            *(uint32_t *)d = *(const uint32_t *)s;
            d += 4; s += 4; n -= 4;
        }
        if (n >= 2) {
            *(uint16_t *)d = *(const uint16_t *)s;
            d += 2; s += 2; n -= 2;
        }
        if (n) {
            *d = *s;
        }
        return original_dest;
    }

    size_t qwords = n / 8;
    size_t remainder = n % 8;

    __asm__ volatile (
        "cld\n\t"
        "rep movsq\n\t"
        "movq %3, %%rcx\n\t"
        "rep movsb"
        : "+D"(d), "+S"(s), "+c"(qwords)
        : "r"(remainder)
        : "memory"
    );
    return original_dest;
#else
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    /* Fast 8-byte copy path */
    while (n >= 8) {
        *(uint64_t *)d = *(const uint64_t *)s;
        d += 8; s += 8; n -= 8;
    }
    while (n--) *d++ = *s++;
    return dest;
#endif
}

void *memset(void *s, int c, size_t n) {
#ifdef __x86_64__
    void *original_dest = s;
    uint64_t val = (uint8_t)c;
    /* Create 64-bit pattern using multiplication */
    uint64_t pattern = val * 0x0101010101010101ULL;

    size_t qwords = n / 8;
    size_t remainder = n % 8;

    __asm__ volatile (
        "cld\n\t"
        "rep stosq\n\t"
        "movq %3, %%rcx\n\t"
        "rep stosb"
        : "+D"(s), "+c"(qwords)
        : "a"(pattern), "r"(remainder)
        : "memory"
    );
    return original_dest;
#else
    uint8_t *p = (uint8_t *)s;
    uint8_t val = (uint8_t)c;

    /* Fast 8-byte fill path */
    if (n >= 8) {
        uint64_t wide = val;
        wide |= wide << 8;
        wide |= wide << 16;
        wide |= wide << 32;
        while (n >= 8) {
            *(uint64_t *)p = wide;
            p += 8; n -= 8;
        }
    }
    while (n--) *p++ = val;
    return s;
#endif
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if (d < s) {
        /* Fast forward copy */
        while (n >= 8) {
            *(uint64_t *)d = *(const uint64_t *)s;
            d += 8; s += 8; n -= 8;
        }
        while (n--) *d++ = *s++;
    } else if (d > s) {
        d += n; s += n;
        /* Fast backward copy */
        while (n >= 8) {
            d -= 8; s -= 8; n -= 8;
            *(uint64_t *)d = *(const uint64_t *)s;
        }
        while (n--) *--d = *--s;
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *a = (const uint8_t *)s1;
    const uint8_t *b = (const uint8_t *)s2;

#ifdef __x86_64__
    // Optimization for x86_64: Compare 8-byte blocks
    // This significantly reduces the number of iterations for large blocks.
    // We use uint64_t to compare 8 bytes at a time.
    while (n >= 8) {
        if (*(const uint64_t *)a != *(const uint64_t *)b) {
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

    while (n--) {
        if (*a != *b) return (int)*a - (int)*b;
        a++; b++;
    }
    return 0;
}

size_t strlen(const char *s) {
#ifdef __x86_64__
    const char *p = s;

    /* Align to 8 bytes */
    while ((uintptr_t)p & 7) {
        if (!*p) return p - s;
        p++;
    }

    /* Process 8 bytes at a time */
    typedef uint64_t __attribute__((__may_alias__)) u64_alias;
    const u64_alias *lp = (const u64_alias *)p;

    uint64_t v;
    while (1) {
        v = *lp++;
        /* Check if any byte is 0 using SWAR magic:
           (v - 0x01...) & ~v & 0x80... detects zero byte */
        if (((v - 0x0101010101010101ULL) & ~v & 0x8080808080808080ULL)) {
            const char *cp = (const char *)(lp - 1);
            if (cp[0] == 0) return cp - s;
            if (cp[1] == 0) return cp - s + 1;
            if (cp[2] == 0) return cp - s + 2;
            if (cp[3] == 0) return cp - s + 3;
            if (cp[4] == 0) return cp - s + 4;
            if (cp[5] == 0) return cp - s + 5;
            if (cp[6] == 0) return cp - s + 6;
            return cp - s + 7;
        }
    }
#else
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
#endif
}

size_t strnlen(const char *s, size_t maxlen) {
#ifdef __x86_64__
    const char *char_ptr = s;
    const uint64_t *longword_ptr;
    uint64_t longword, himagic, lomagic;

    /* Handle unaligned bytes */
    while (maxlen > 0 && ((uintptr_t)char_ptr & 7) != 0) {
        if (*char_ptr == '\0')
            return char_ptr - s;
        char_ptr++;
        maxlen--;
    }

    /* Process 8 bytes at a time */
    longword_ptr = (const uint64_t*)char_ptr;
    himagic = 0x8080808080808080ULL;
    lomagic = 0x0101010101010101ULL;

    /* ⚡ BOLT Optimization: SWAR (SIMD Within A Register) for strnlen.
       Process 8 bytes at a time to drastically reduce loop iterations.
       This significantly speeds up strncpy, strlcat, and strlcpy. */
    while (maxlen >= 8) {
        longword = *longword_ptr++;

        if (((longword - lomagic) & ~longword & himagic) != 0) {
            const char* cp = (const char*)(longword_ptr - 1);
            if (cp[0] == 0) return cp - s;
            if (cp[1] == 0) return cp - s + 1;
            if (cp[2] == 0) return cp - s + 2;
            if (cp[3] == 0) return cp - s + 3;
            if (cp[4] == 0) return cp - s + 4;
            if (cp[5] == 0) return cp - s + 5;
            if (cp[6] == 0) return cp - s + 6;
            if (cp[7] == 0) return cp - s + 7;
        }
        maxlen -= 8;
    }

    /* Handle remaining bytes */
    char_ptr = (const char*)longword_ptr;
    while (maxlen > 0) {
        if (*char_ptr == '\0')
            return char_ptr - s;
        char_ptr++;
        maxlen--;
    }

    return char_ptr - s;
#else
    size_t i;
    for (i = 0; i < maxlen && s[i]; i++);
    return i;
#endif
}

int strcmp(const char *s1, const char *s2) {
#ifdef __x86_64__
    /* Optimization: Compare 8 bytes at a time if pointers are aligned */
    typedef uint64_t __attribute__((__may_alias__)) u64_alias;

    /* Align s1 to 8 bytes */
    while (((uintptr_t)s1 & 7) != 0) {
        if (*s1 != *s2) {
            return *(const unsigned char *)s1 - *(const unsigned char *)s2;
        }
        if (*s1 == '\0') {
            return 0;
        }
        s1++;
        s2++;
    }

    /* If s2 is also aligned, we can do 64-bit comparison */
    if (((uintptr_t)s2 & 7) == 0) {
        const u64_alias *ls1 = (const u64_alias *)s1;
        const u64_alias *ls2 = (const u64_alias *)s2;
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
                s1 = (const char *)ls1;
                s2 = (const char *)ls2;
                break;
            }

            ls1++;
            ls2++;
        }
    }
#endif

    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && *s1 == *s2) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

char *strncpy(char *dest, const char *src, size_t n) {
    /* ⚡ BOLT Optimization: Leverage strnlen and block memory operations (memcpy/memset)
       instead of a slow byte-by-byte copy loop. */
    size_t len = strnlen(src, n);
    memcpy(dest, src, len);
    if (len < n) {
        memset(dest + len, 0, n - len);
    }
    return dest;
}

/**
 * strlcpy - Copy a C-string into a sized buffer
 * @dest: Where to copy the string to
 * @src: Where to copy the string from
 * @size: size of destination buffer
 */
size_t strlcpy(char *dest, const char *src, size_t size) {
    size_t len = strlen(src);
    if (size > 0) {
        size_t to_copy = (len >= size) ? size - 1 : len;
        memcpy(dest, src, to_copy);
        dest[to_copy] = '\0';
    }
    return len;
}

/**
 * strlcat - Append a length-limited, C-string to another
 * @dest: The string to be appended to
 * @src: The string to append to it
 * @size: Total size of the destination buffer
 */
size_t strlcat(char *dest, const char *src, size_t size) {
    size_t dlen = strnlen(dest, size);
    if (dlen >= size) return size + strlen(src);
    return dlen + strlcpy(dest + dlen, src, size - dlen);
}

char *strchr(const char *s, int c) {
#ifdef __x86_64__
    const char *p = s;
    const uint64_t one_bits = 0x0101010101010101ULL;
    const uint64_t high_bits = 0x8080808080808080ULL;

    /* Align to 8 bytes */
    while ((uintptr_t)p & 7) {
        if (*p == (char)c) return (char *)p;
        if (!*p) return (void*)0;
        p++;
    }

    /* Process 8 bytes at a time */
    typedef uint64_t __attribute__((__may_alias__)) u64_alias;
    const u64_alias *lp = (const u64_alias *)p;

    uint64_t v;
    uint64_t char_mask = (unsigned char)c * one_bits;

    while (1) {
        v = *lp;
        /* Check if any byte is 0 using SWAR magic:
           (v - 0x01...) & ~v & 0x80... detects zero byte */
        uint64_t zero_bytes = (v - one_bits) & ~v & high_bits;

        /* Check if any byte matches c:
           We XOR with char_mask, then detect zero bytes in result */
        uint64_t match_bytes = ((v ^ char_mask) - one_bits) & ~(v ^ char_mask) & high_bits;

        if (zero_bytes | match_bytes) {
            /* Found something, break to byte loop */
            p = (const char *)lp;
            break;
        }
        lp++;
    }

    /* Byte-wise loop to find exact location */
    while (1) {
        if (*p == (char)c) return (char *)p;
        if (!*p) return (void*)0;
        p++;
    }
#else
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == 0) ? (char *)s : (void*)0;
#endif
}

char *strrchr(const char *s, int c) {
    /* ⚡ BOLT Optimization: Use optimized strlen to find length and search backwards.
       Avoids full string forward traversal when searching for characters near the end. */
    size_t len = strlen(s);
    if (c == 0) return (char *)(s + len);

    const char *p = s + len;
    char ch = (char)c;

    while (p > s) {
        p--;
        if (*p == ch) return (char *)p;
    }
    return (void*)0;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (strncmp(haystack, needle, nlen) == 0)
            return (char *)haystack;
        haystack++;
    }
    return (void*)0;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

void *memchr(const void *s, int c, size_t n) {
#ifdef __x86_64__
    const unsigned char *p = (const unsigned char *)s;
    const uint64_t one_bits = 0x0101010101010101ULL;
    const uint64_t high_bits = 0x8080808080808080ULL;

    /* Align to 8 bytes */
    while (n && ((uintptr_t)p & 7)) {
        if (*p == (unsigned char)c) return (void *)p;
        p++;
        n--;
    }

    /* Process 8 bytes at a time */
    typedef uint64_t __attribute__((__may_alias__)) u64_alias;
    const u64_alias *lp = (const u64_alias *)p;

    uint64_t v;
    uint64_t char_mask = (unsigned char)c * one_bits;

    while (n >= 8) {
        v = *lp;

        /* Check if any byte matches c:
           We XOR with char_mask, then detect zero bytes in result */
        uint64_t match_bytes = ((v ^ char_mask) - one_bits) & ~(v ^ char_mask) & high_bits;

        if (match_bytes) {
            /* Found something, break to byte loop */
            p = (const unsigned char *)lp;
            break;
        }
        lp++;
        n -= 8;
    }

    p = (const unsigned char *)lp;
    /* Byte-wise loop to find exact location */
    while (n--) {
        if (*p == (unsigned char)c) return (void *)p;
        p++;
    }

    return (void *)0;
#else
    const unsigned char *p = (const unsigned char *)s;
    while (n--) {
        if (*p == (unsigned char)c) return (void *)p;
        p++;
    }
    return (void *)0;
#endif
}

char *strerror(int errnum) {
    switch (errnum) {
        case 0:     return "Success";
        case 1:     return "Operation not permitted";
        case 2:     return "No such file or directory";
        case 3:     return "No such process";
        case 4:     return "Interrupted system call";
        case 5:     return "Input/output error";
        case 6:     return "No such device or address";
        case 7:     return "Argument list too long";
        case 8:     return "Exec format error";
        case 9:     return "Bad file descriptor";
        case 10:    return "No child processes";
        case 11:    return "Resource temporarily unavailable";
        case 12:    return "Cannot allocate memory";
        case 13:    return "Permission denied";
        case 14:    return "Bad address";
        case 16:    return "Device or resource busy";
        case 17:    return "File exists";
        case 18:    return "Invalid cross-device link";
        case 19:    return "No such device";
        case 20:    return "Not a directory";
        case 21:    return "Is a directory";
        case 22:    return "Invalid argument";
        case 23:    return "Too many open files in system";
        case 24:    return "Too many open files";
        case 25:    return "Inappropriate ioctl for device";
        case 27:    return "File too large";
        case 28:    return "No space left on device";
        case 29:    return "Illegal seek";
        case 30:    return "Read-only file system";
        case 36:    return "File name too long";
        case 38:    return "Function not implemented";
        case 39:    return "Directory not empty";
        case 61:    return "No data available";
        case 75:    return "Value too large for defined data type";
        default:    return "Unknown error";
    }
}
