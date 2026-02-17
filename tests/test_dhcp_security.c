#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

/* Defines for host test environment */
#define __ETEROS_HOST_TEST__

/* Mock kernel/net/dhcp.h if needed, or include it directly */
#include <net/dhcp.h>
#include <net/defs.h>

/* We need to undefine libc macros if they conflict, but here we are in user space so it's fine.
   However, kernel/string.c might redefine them if __ETEROS_HOST_TEST__ is defined.
   Let's see kernel/string.c content later. For now, rely on standard headers. */

/* Prototype for the function we are testing (if not yet in header) */
int dhcp_parse_options(const struct dhcp_packet* dhcp, size_t packet_len, uint32_t* out_mask, uint32_t* out_gw, uint32_t* out_dns);

void test_buffer_overflow_prevention() {
    printf("Running test_buffer_overflow_prevention...\n");

    /* Create a buffer that looks like a DHCP packet but ends abruptly */
    uint8_t buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    struct dhcp_packet* dhcp = (struct dhcp_packet*)buffer;
    dhcp->op = 2;
    dhcp->magic = htonl(DHCP_MAGIC);

    /* Valid fixed part is 240 bytes (up to options) */
    size_t fixed_size = sizeof(struct dhcp_packet) - 308;

    /* Case 1: Packet ends exactly at fixed size (no options) */
    uint32_t mask=0, gw=0, dns=0;
    int res = dhcp_parse_options(dhcp, fixed_size, &mask, &gw, &dns);
    if (res != 0) {
        printf("FAIL: Should succeed with no options (res=%d)\n", res);
        exit(1);
    }

    /* Case 2: Packet has one byte of options (valid padding) */
    dhcp->options[0] = 0; /* Pad */
    res = dhcp_parse_options(dhcp, fixed_size + 1, &mask, &gw, &dns);
    if (res != 0) {
        printf("FAIL: Should succeed with 1 byte padding\n");
        exit(1);
    }

    /* Case 3: Packet says Option 1 (Mask, len 4) but buffer ends immediately */
    dhcp->options[0] = 1; /* Mask */
    /* We don't write length or data, just stop the buffer here */
    res = dhcp_parse_options(dhcp, fixed_size + 1, &mask, &gw, &dns);
    /* Should fail or at least not crash/overflow. Return 0 if it just stops parsing safely?
       Ideally, if an option is truncated, we might want to return -1 or just ignore it.
       Let's assume safe parsing means ignore truncated options. */

    /* Case 4: Packet says Option 1, Length 4, but buffer only has 2 bytes of data */
    dhcp->options[0] = 1; /* Mask */
    dhcp->options[1] = 4; /* Len */
    dhcp->options[2] = 0xFF;
    dhcp->options[3] = 0xFF;
    /* Missing last 2 bytes */
    res = dhcp_parse_options(dhcp, fixed_size + 4, &mask, &gw, &dns);
    /* Should handle safely. */

    printf("PASS: test_buffer_overflow_prevention\n");
}

void test_infinite_loop_prevention() {
    printf("Running test_infinite_loop_prevention...\n");

    uint8_t buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    struct dhcp_packet* dhcp = (struct dhcp_packet*)buffer;
    dhcp->op = 2;
    dhcp->magic = htonl(DHCP_MAGIC);
    size_t fixed_size = sizeof(struct dhcp_packet) - 308;

    /* Fill options with 0 (Padding) */
    /* If the parser doesn't advance past padding or doesn't have a max length check,
       it might loop forever if we lie about packet length or if it's just very long.
       Actually, padding advances by 1, so it terminates eventually. */

    /* What if we have a loop of Option type 0? */
    memset(dhcp->options, 0, 308);
    uint32_t mask=0, gw=0, dns=0;

    /* This should finish quickly */
    dhcp_parse_options(dhcp, fixed_size + 308, &mask, &gw, &dns);

    printf("PASS: test_infinite_loop_prevention\n");
}

void test_valid_options() {
    printf("Running test_valid_options...\n");

    uint8_t buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    struct dhcp_packet* dhcp = (struct dhcp_packet*)buffer;
    dhcp->op = 2;
    dhcp->magic = htonl(DHCP_MAGIC);
    size_t fixed_size = sizeof(struct dhcp_packet) - 308;

    int i = 0;
    /* Subnet Mask: 255.255.255.0 */
    dhcp->options[i++] = 1;
    dhcp->options[i++] = 4;
    dhcp->options[i++] = 255;
    dhcp->options[i++] = 255;
    dhcp->options[i++] = 255;
    dhcp->options[i++] = 0;

    /* End option */
    dhcp->options[i++] = 255;

    uint32_t mask=0, gw=0, dns=0;
    int res = dhcp_parse_options(dhcp, fixed_size + i, &mask, &gw, &dns);

    if (res != 0) {
        printf("FAIL: Valid options failed\n");
        exit(1);
    }

    if (mask != 0x00FFFFFF) { /* Little Endian 255.255.255.0 -> 0x00FFFFFF? Wait.
                                 memcpy reads bytes 255, 255, 255, 0.
                                 In memory: FF FF FF 00.
                                 As uint32 (little endian): 0x00FFFFFF.
                                 Wait, first byte is LSB? No.
                                 0xFF at addr 0 (LSB), 0xFF at addr 1...
                                 val = 0xFF + (0xFF<<8) + (0xFF<<16) + (0x00<<24) = 0x00FFFFFF. Correct. */

       /* Actually, let's verify exact bytes to be sure about endianness. */
       if (memcmp(&mask, "\xFF\xFF\xFF\x00", 4) != 0) {
           printf("FAIL: Mask parsed incorrectly. Got %08X\n", mask);
           exit(1);
       }
    }

    printf("PASS: test_valid_options\n");
}

int main() {
    test_buffer_overflow_prevention();
    test_infinite_loop_prevention();
    test_valid_options();
    return 0;
}
