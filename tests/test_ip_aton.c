#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <net/defs.h>

/* Helper to convert uint32_t to human readable string (for debugging) */
void print_ip(uint32_t ip) {
    uint8_t bytes[4];
    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;
    printf("%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
}

void test_valid_ips() {
    printf("Testing Valid IPs...\n");

    /* 1.2.3.4 -> 0x04030201 (Little Endian) */
    uint32_t val = ip_aton("1.2.3.4");
    assert(val == 0x04030201);

    /* 0.0.0.0 -> 0x00000000 */
    /* Note: ip_aton returns 0 on failure, so 0.0.0.0 is technically valid but ambiguous return */
    /* Implementation logic: digits=1 ('0'), part=0. Loop runs 4 times. Returns 0. Correct. */
    val = ip_aton("0.0.0.0");
    assert(val == 0);

    /* 255.255.255.255 -> 0xFFFFFFFF */
    val = ip_aton("255.255.255.255");
    assert(val == 0xFFFFFFFF);
}

void test_invalid_ips() {
    printf("Testing Invalid IPs...\n");

    /* Overflow per octet */
    assert(ip_aton("256.0.0.0") == 0);
    assert(ip_aton("300.2.3.4") == 0);
    assert(ip_aton("1.2.3.256") == 0);

    /* Missing octets */
    assert(ip_aton("1.2.3") == 0);
    assert(ip_aton("1.2") == 0);
    assert(ip_aton("1") == 0);

    /* Extra octets / garbage */
    assert(ip_aton("1.2.3.4.5") == 0);
    assert(ip_aton("1.2.3.4a") == 0);
    assert(ip_aton("1.2.3.4 ") == 0); // Trailing space

    /* Format errors */
    assert(ip_aton("1.2.3.") == 0);   // Trailing dot
    assert(ip_aton(".2.3.4") == 0);   // Leading dot
    assert(ip_aton("1..3.4") == 0);   // Double dot
    assert(ip_aton("1.2.3.4.") == 0); // Trailing dot after valid IP
}

int main() {
    printf("Running ip_aton tests with REAL implementation...\n");
    test_valid_ips();
    test_invalid_ips();
    printf("All ip_aton tests passed!\n");
    return 0;
}
