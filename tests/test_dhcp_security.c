/*
 * Test for DHCP Security (Option Parsing)
 *
 * This test verifies that the DHCP option parser is robust against malformed
 * packets, including:
 * - Options extending beyond the buffer.
 * - Options with invalid lengths.
 * - Infinite loops (though strictly parsing sequentially avoids this).
 * - Zero-length options (padding).
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
/* #include <stdlib.h> - Conflict with local stdlib.h */
void exit(int status);

/* Include project headers */
#include <net/dhcp.h>
#include <net/defs.h>

/* Helper to build a basic DHCP packet */
void build_base_packet(struct dhcp_packet* dhcp, size_t total_size) {
    memset(dhcp, 0, total_size);
    dhcp->op = 2; /* Reply */
    dhcp->xid = htonl(0x12345678);
    dhcp->magic = htonl(DHCP_MAGIC);
}

void test_normal_options() {
    printf("Running test_normal_options...\n");
    uint8_t buffer[512];
    struct dhcp_packet* dhcp = (struct dhcp_packet*)buffer;
    build_base_packet(dhcp, sizeof(buffer));

    /* Add some valid options */
    uint8_t* opt = dhcp->options;

    /* Option 53: Message Type = Offer (2) */
    *opt++ = 53; *opt++ = 1; *opt++ = 2;

    /* Option 1: Subnet Mask 255.255.255.0 */
    *opt++ = 1; *opt++ = 4;
    *opt++ = 255; *opt++ = 255; *opt++ = 255; *opt++ = 0;

    /* Option 255: End */
    *opt++ = 255;

    /* Parse */
    uint32_t mask = 0, gw = 0, dns = 0;

    /* Calculate packet len: options start at offset 240.
       We used approx 10 bytes of options.
       Let's say the packet ends right after End option. */
    size_t packet_len = (opt - buffer);

    dhcp_parse_options(dhcp, packet_len, &mask, &gw, &dns);

    /* Check results */
    /* 255.255.255.0 in hex is 0x00FFFFFF (little endian) or 0xFFFFFF00 (big endian)?
       dhcp.c does memcpy(&mask, opt, 4).
       Network byte order is Big Endian. 255.255.255.0 -> FF FF FF 00.
       x86 is Little Endian. So memcpy keeps bytes as FF FF FF 00.
       But if we read it as uint32, it becomes 0x00FFFFFF.
       Wait, let's check expectation.
    */
    uint32_t expected_mask = 0;
    uint8_t mask_bytes[] = {255, 255, 255, 0};
    memcpy(&expected_mask, mask_bytes, 4);

    if (mask != expected_mask) {
        printf("FAIL: Mask mismatch. Got %08x, expected %08x\n", mask, expected_mask);
        exit(1);
    }
    printf("PASS\n");
}

void test_truncated_option_header() {
    printf("Running test_truncated_option_header...\n");
    uint8_t buffer[512];
    struct dhcp_packet* dhcp = (struct dhcp_packet*)buffer;
    build_base_packet(dhcp, sizeof(buffer));

    uint8_t* opt = dhcp->options;

    /* Option 53: Message Type */
    *opt++ = 53;
    /* Missing length and value! Packet ends here. */

    size_t packet_len = (opt - buffer); /* Points just after '53' */

    uint32_t mask = 0, gw = 0, dns = 0;
    dhcp_parse_options(dhcp, packet_len, &mask, &gw, &dns);

    /* Should not crash. */
    printf("PASS\n");
}

void test_truncated_option_value() {
    printf("Running test_truncated_option_value...\n");
    uint8_t buffer[512];
    struct dhcp_packet* dhcp = (struct dhcp_packet*)buffer;
    build_base_packet(dhcp, sizeof(buffer));

    uint8_t* opt = dhcp->options;

    /* Option 1: Mask */
    *opt++ = 1;
    *opt++ = 4; /* Claims 4 bytes follow */
    *opt++ = 255; /* But only 1 byte provided */

    size_t packet_len = (opt - buffer);

    uint32_t mask = 0, gw = 0, dns = 0;
    dhcp_parse_options(dhcp, packet_len, &mask, &gw, &dns);

    if (mask != 0) {
        printf("FAIL: Mask should be 0 because option was truncated.\n");
        exit(1);
    }
    printf("PASS\n");
}

void test_option_length_overflow() {
    printf("Running test_option_length_overflow...\n");
    uint8_t buffer[512];
    struct dhcp_packet* dhcp = (struct dhcp_packet*)buffer;
    build_base_packet(dhcp, sizeof(buffer));

    uint8_t* opt = dhcp->options;

    /* Option 1: Mask */
    *opt++ = 1;
    *opt++ = 200; /* Claims 200 bytes follow! */
    *opt++ = 255; *opt++ = 255; *opt++ = 255; *opt++ = 0;
    /* But packet ends shortly after */

    size_t packet_len = (opt - buffer) + 10; /* Give it a bit more, but less than 200 */

    uint32_t mask = 0, gw = 0, dns = 0;
    dhcp_parse_options(dhcp, packet_len, &mask, &gw, &dns);

    /* Should not crash and should not parse the mask because length is invalid */
    if (mask != 0) {
        printf("FAIL: Mask should be 0 because option length exceeded buffer.\n");
        exit(1);
    }
    printf("PASS\n");
}

void test_padding_flood() {
    printf("Running test_padding_flood...\n");
    uint8_t buffer[1024]; /* Large buffer */
    struct dhcp_packet* dhcp = (struct dhcp_packet*)buffer;
    build_base_packet(dhcp, sizeof(buffer));

    /* Fill options with 0 (Padding) */
    /* This tests that we don't get stuck or crash, and handle limits */

    size_t packet_len = 1000;
    uint32_t mask = 0, gw = 0, dns = 0;
    dhcp_parse_options(dhcp, packet_len, &mask, &gw, &dns);

    printf("PASS\n");
}

int main() {
    test_normal_options();
    test_truncated_option_header();
    test_truncated_option_value();
    test_option_length_overflow();
    test_padding_flood();
    return 0;
}
