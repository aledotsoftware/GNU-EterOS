#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// Mocks needed for tcp.c
uint32_t my_ip = 0x01020304;
uint8_t gateway_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

uint64_t timer_get_ticks(void) { return 1000; }
void task_yield(void) {}
void hal_console_write(const char* s) { printf("[KERNEL] %s", s); }

// Buffer to capture sent packet
uint8_t sent_packet[2000];
int sent_packet_len = 0;

// We need to match signature in include/net/e1000.h
// uint8_t* e1000_get_mac(void);
uint8_t mock_mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
uint8_t* e1000_get_mac_mock(void) { return mock_mac; }

void net_poll(void) {} // Mock net_poll

// define e1000_send_packet mock
int e1000_send_packet(const void* data, uint16_t len) {
    if (len > sizeof(sent_packet)) {
        printf("e1000_send_packet: Packet too large for capture buffer! (%d)\n", len);
        return -1;
    }
    printf("e1000_send_packet: Sending %d bytes\n", len);
    memcpy(sent_packet, data, len);
    sent_packet_len = len;
    return 0;
}

uint16_t net_checksum(void* vdata, size_t length) {
    return 0; // Dummy checksum
}

// Redirect e1000_get_mac to our mock
#define e1000_get_mac e1000_get_mac_mock

// Helper wrappers for __ETEROS_HOST_TEST__
#ifdef __ETEROS_HOST_TEST__
void* eteros_memcpy(void* dest, const void* src, size_t n) {
    return __builtin_memcpy(dest, src, n);
}

void* eteros_memset(void* s, int c, size_t n) {
    return __builtin_memset(s, c, n);
}
#endif

// Includes
#include "net/defs.h"
#include "net/socket.h"
#include "net/e1000.h"

// Define RX_BUFFER_SIZE if not in headers
#ifndef RX_BUFFER_SIZE
#define RX_BUFFER_SIZE 8192
#endif

// Include the source file under test
#include "../kernel/net/tcp.c"

// Test Case
void test_tcp_send_overflow() {
    socket_entry_t sock;
    memset(&sock, 0, sizeof(sock));
    sock.state = SOCKET_STATE_ESTABLISHED;
    sock.local_port = 1234;
    sock.remote_ip = 0x05060708;
    sock.remote_port = 80;
    sock.seq_num = 100;
    sock.ack_num = 50;
    sock.used = 1;

    // Payload larger than 1514 - headers (~54) = 1460
    // Try 2000 bytes
    char payload[2000];
    memset(payload, 'A', sizeof(payload));

    printf("Testing tcp_send with 2000 bytes payload...\n");

    sent_packet_len = 0;

    // tcp_send currently returns 'len' even if failed?
    // We expect the fix to make it return -1 or handled error.
    int res = tcp_send(&sock, payload, sizeof(payload));

    printf("tcp_send returned: %d\n", res);

    if (res == -1) {
        printf("PASS: tcp_send rejected oversized payload.\n");
    } else {
        printf("FAIL: tcp_send accepted oversized payload (res=%d).\n", res);
        // In a real overflow scenario, we might have crashed or corrupted stack.
        // Since we are mocking e1000_send_packet, the overflow happens in 'buffer' in tcp_send_packet.
        // That buffer is on stack of tcp_send_packet.
        // The overflow overwrites return address etc.
        // So valid behavior here is CRASH or WEIRDNESS if bug is present.
    }
}

int main() {
    test_tcp_send_overflow();
    return 0;
}
