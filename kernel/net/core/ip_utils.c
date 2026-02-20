#include <net/defs.h>
#include <types.h>

/**
 * Parses an IPv4 address string safely.
 * Strict validation: No more than 3 digits per octet, value <= 255, correct separators.
 * Returns 0 on failure or invalid format.
 * Returns IP in little-endian integer representation (matching byte array order).
 * e.g. "1.2.3.4" -> 0x04030201
 */
uint32_t ip_aton(const char* cp) {
    uint32_t val = 0;

    for (int i = 0; i < 4; i++) {
        uint32_t part = 0;
        int digits = 0;

        /* Parse number */
        while (*cp >= '0' && *cp <= '9') {
            if (digits > 2) return 0; /* Max 3 digits (0-255) */

            part = part * 10 + (*cp - '0');
            digits++;
            cp++;
        }

        if (digits == 0) return 0; /* Empty part */
        if (part > 255) return 0;  /* Value > 255 */

        val |= (part << (i * 8));

        if (i < 3) {
            if (*cp != '.') return 0; /* Expected dot */
            cp++;
        }
    }

    if (*cp != '\0') return 0; /* Trailing garbage */

    return val;
}
