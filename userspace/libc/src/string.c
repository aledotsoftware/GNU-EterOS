/**
 * éterOS Mini-LibC - String Functions
 * musl-compatible memory and string operations
 */

#include <string.h>
#include <stdint.h>

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    /* Fast 8-byte copy path */
    while (n >= 8) {
        *(uint64_t *)d = *(const uint64_t *)s;
        d += 8; s += 8; n -= 8;
    }
    while (n--) *d++ = *s++;
    return dest;
}

void *memset(void *s, int c, size_t n) {
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
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if (d < s) {
        while (n--) *d++ = *s++;
    } else if (d > s) {
        d += n; s += n;
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
    const uint64_t *lp = (const uint64_t *)p;
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
    size_t i;
    for (i = 0; i < maxlen && s[i]; i++);
    return i;
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
    char *d = dest;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
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
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == 0) ? (char *)s : (void*)0;
}

char *strrchr(const char *s, int c) {
    const char *last = (void*)0;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == 0) return (char *)s;
    return (char *)last;
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
