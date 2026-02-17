/*
 * Security Test for DHCP Parsing
 * Verifies robust handling of malformed DHCP options (buffer overflows, etc.)
 *
 * Run: gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_dhcp_security.c kernel/net/dhcp_parser.c kernel/string.c -o tests/test_dhcp_security && ./tests/test_dhcp_security
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include <net/dhcp.h>

/* Helper to print test result */
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL: %s\n", msg); \
        exit(1); \
    } \
} while(0)

void test_valid_options() {
    struct dhcp_packet packet;
    memset(&packet, 0, sizeof(packet));

    /* Construct valid options */
    uint8_t* opt = packet.options;

    /* Option 1: Subnet Mask (4 bytes) */
    *opt++ = 1;
    *opt++ = 4;
    *opt++ = 255; *opt++ = 255; *opt++ = 255; *opt++ = 0;

    /* Option 3: Gateway (4 bytes) */
    *opt++ = 3;
    *opt++ = 4;
    *opt++ = 192; *opt++ = 168; *opt++ = 1; *opt++ = 1;

    /* Option 255: End */
    *opt++ = 255;

    uint32_t mask = 0, gw = 0, dns = 0;
    size_t packet_len = (uint8_t*)opt - (uint8_t*)&packet; /* Exact length used */

    /* To test standard behavior, pass a length that includes these options */
    int res = dhcp_parse_options(&packet, packet_len, &mask, &gw, &dns);

    TEST_ASSERT(res == 0, "Valid options parse failed");
    TEST_ASSERT(mask == 0x00FFFFFF, "Mask not parsed correctly (little endian host assumed? No, memcpy preserves bytes)");

    /* Check byte order manually since host is likely Little Endian */
    /* mask bytes: FF FF FF 00. As uint32: 0x00FFFFFF on LE */
    uint32_t expected_mask = 0;
    uint8_t expected_mask_bytes[] = {255, 255, 255, 0};
    memcpy(&expected_mask, expected_mask_bytes, 4);

    TEST_ASSERT(mask == expected_mask, "Mask value mismatch");

    printf("PASS: test_valid_options\n");
}

void test_truncated_option_header() {
    struct dhcp_packet packet;
    memset(&packet, 0, sizeof(packet));

    uint8_t* opt = packet.options;
    *opt++ = 1; /* Type 1 */
    /* Missing Length */

    /* Packet ends right here */
    size_t packet_len = (uint8_t*)opt - (uint8_t*)&packet;

    uint32_t mask = 0xDEADBEEF, gw = 0, dns = 0;

    int res = dhcp_parse_options(&packet, packet_len, &mask, &gw, &dns);

    TEST_ASSERT(res == 0, "Truncated header should not error, just stop parsing");
    TEST_ASSERT(mask == 0xDEADBEEF, "Mask should not be modified");

    printf("PASS: test_truncated_option_header\n");
}

void test_truncated_option_data() {
    struct dhcp_packet packet;
    memset(&packet, 0, sizeof(packet));

    uint8_t* opt = packet.options;
    *opt++ = 1; /* Type 1 */
    *opt++ = 4; /* Length 4 */
    *opt++ = 255;
    *opt++ = 255;
    /* Missing last 2 bytes */

    /* Packet ends right here */
    size_t packet_len = (uint8_t*)opt - (uint8_t*)&packet;

    uint32_t mask = 0xDEADBEEF, gw = 0, dns = 0;

    int res = dhcp_parse_options(&packet, packet_len, &mask, &gw, &dns);

    TEST_ASSERT(res == 0, "Truncated data should not error, just stop parsing");
    TEST_ASSERT(mask == 0xDEADBEEF, "Mask should not be modified (partial read avoided)");

    printf("PASS: test_truncated_option_data\n");
}

void test_option_length_overflow() {
    struct dhcp_packet packet;
    memset(&packet, 0, sizeof(packet));

    uint8_t* opt = packet.options;
    *opt++ = 1; /* Type 1 */
    *opt++ = 200; /* Length 200 - Way past end of what we provide */
    *opt++ = 255;

    /* Packet ends right here (only 3 bytes of options provided) */
    size_t packet_len = (uint8_t*)opt - (uint8_t*)&packet;

    uint32_t mask = 0xDEADBEEF, gw = 0, dns = 0;

    int res = dhcp_parse_options(&packet, packet_len, &mask, &gw, &dns);

    TEST_ASSERT(res == 0, "Overflow length should handle gracefully");
    TEST_ASSERT(mask == 0xDEADBEEF, "Mask should not be modified");

    printf("PASS: test_option_length_overflow\n");
}

void test_padding_loop() {
    struct dhcp_packet packet;
    memset(&packet, 0, sizeof(packet));

    uint8_t* opt = packet.options;
    *opt++ = 0; /* Pad */
    *opt++ = 0; /* Pad */
    *opt++ = 0; /* Pad */
    *opt++ = 255; /* End */

    size_t packet_len = (uint8_t*)opt - (uint8_t*)&packet;

    uint32_t mask = 0, gw = 0, dns = 0;
    int res = dhcp_parse_options(&packet, packet_len, &mask, &gw, &dns);

    TEST_ASSERT(res == 0, "Padding loop failed");

    printf("PASS: test_padding_loop\n");
}

int main() {
    printf("Running DHCP Security Tests...\n");
    test_valid_options();
    test_truncated_option_header();
    test_truncated_option_data();
    test_option_length_overflow();
    test_padding_loop();
    printf("All Security Tests Passed.\n");
    return 0;
}
