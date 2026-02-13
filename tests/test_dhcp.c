#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

/* Mock Headers */
#include "../include/types.h"
#include "../include/string.h"
#include "../include/vga.h"
#include "../include/timer.h"
#include "../include/net/e1000.h"

/* Mock Variables */
static uint8_t mock_mac[] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint64_t mock_ticks = 0;

/* Mock Implementations */

/* Timer */
uint64_t timer_get_ticks(void) {
    return mock_ticks++;
}
void timer_sleep(uint32_t ms) {
    (void)ms;
    mock_ticks += ms;
}

/* E1000 */
uint8_t* e1000_get_mac(void) {
    return mock_mac;
}

int e1000_send_packet(const void* data, uint16_t len) {
    (void)data;
    (void)len;
    return 0; // Success
}

int e1000_receive(void* buffer, uint16_t max_len) {
    (void)buffer;
    (void)max_len;
    return 0; // No packet
}

/* VGA */
void terminal_write_colored(const char* str, vga_color_t fg, vga_color_t bg) {
    (void)fg; (void)bg;
    printf("%s", str);
}

void terminal_write_string(const char* str) {
    printf("%s", str);
}

/* Include Dependencies */
/* We link kernel/string.c to get eteros_* implementations */
#include "../kernel/string.c"

/* Include Code Under Test */
#include "../kernel/net/dhcp.c"

int main() {
    printf("Running DHCP Utils Test...\n");

    /* Test htons */
    {
        uint16_t v = 0x1234;
        uint16_t expected = 0x3412;
        uint16_t result = htons(v);
        printf("htons(0x%04X) = 0x%04X (Expected 0x%04X)\n", v, result, expected);
        assert(result == expected);

        /* Test symmetry */
        assert(htons(htons(v)) == v);

        /* Test 0 and FFFF */
        assert(htons(0) == 0);
        assert(htons(0xFFFF) == 0xFFFF);
    }

    /* Test htonl */
    {
        uint32_t v = 0x12345678;
        uint32_t expected = 0x78563412;
        uint32_t result = htonl(v);
        printf("htonl(0x%08X) = 0x%08X (Expected 0x%08X)\n", v, result, expected);
        assert(result == expected);

        /* Test symmetry */
        assert(htonl(htonl(v)) == v);

        /* Test edge cases */
        assert(htonl(0) == 0);
        assert(htonl(0xFFFFFFFF) == 0xFFFFFFFF);
    }

    /* Test ip_checksum */
    {
        /* Case 1: Simple buffer (even length) */
        /* Data: 0x01, 0x02 -> Word: 0x0201 (LE) */
        /* Sum = 0x0201. Checksum = ~0x0201 = 0xFDFE */
        uint8_t buf1[] = { 0x01, 0x02 };
        uint16_t sum1 = ip_checksum(buf1, 2);
        printf("checksum({0x01, 0x02}) = 0x%04X\n", sum1);
        assert(sum1 == 0xFDFE);

        /* Case 2: Simple buffer (odd length) */
        /* Data: 0x01, 0x02, 0x03 */
        /* Words: 0x0201 (LE) */
        /* Last byte: 0x03 -> padded as 0x0003 (LE)? No. */
        /* Code: memcpy(&word, data + length - 1, 1); acc += word; */
        /* memcpy reads 1 byte into word (uint16_t initialized to 0). */
        /* So word becomes 0x0003 (LE host). */
        /* Sum = 0x0201 + 0x0003 = 0x0204 */
        /* Checksum = ~0x0204 = 0xFDFB */
        uint8_t buf2[] = { 0x01, 0x02, 0x03 };
        uint16_t sum2 = ip_checksum(buf2, 3);
        printf("checksum({0x01, 0x02, 0x03}) = 0x%04X\n", sum2);
        assert(sum2 == 0xFDFB);

        /* Case 3: Carry handling */
        /* Data: 0xFF, 0xFF, 0x01, 0x00 */
        /* Words: 0xFFFF, 0x0001 */
        /* Sum = 0x10000. */
        /* While (acc >> 16): acc = (0x0000) + (0x0001) = 0x0001 */
        /* Checksum = ~0x0001 = 0xFFFE */
        uint8_t buf3[] = { 0xFF, 0xFF, 0x01, 0x00 };
        uint16_t sum3 = ip_checksum(buf3, 4);
        printf("checksum({0xFF, 0xFF, 0x01, 0x00}) = 0x%04X\n", sum3);
        assert(sum3 == 0xFFFE);

        /* Case 4: Zero buffer */
        /* Code loop doesn't run. Returns ~0 = 0xFFFF. */
        uint8_t buf4[1]; /* Unused content if length 0 */
        uint16_t sum4 = ip_checksum(buf4, 0);
        printf("checksum(len 0) = 0x%04X\n", sum4);
        assert(sum4 == 0xFFFF);
    }

    printf("All DHCP Utils tests passed!\n");
    return 0;
}
