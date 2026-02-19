#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

// Manually declare standard functions that might be shadowed by kernel headers
void* malloc(size_t size);
void free(void* ptr);

// Mock functions required by tcp.c
uint32_t my_ip = 0x01020304;
uint8_t gateway_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

// net_checksum mock
uint16_t net_checksum(void* vdata, size_t length) {
    return 0;
}

// e1000 mock
uint8_t* e1000_get_mac(void) {
    static uint8_t mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    return mac;
}

int e1000_send_packet(const void* data, uint16_t len) {
    // printf("Mock e1000_send_packet: len=%d\n", len);
    return 0;
}

// timer mock
uint64_t timer_get_ticks(void) {
    return 0;
}

// task mock
void task_yield(void) {}

// hal mock
void hal_console_write(const char* str) {
    // printf("%s", str);
}

void net_poll(void) {}

// Undefine macros so we use standard libc functions directly
// This avoids recursion and allows ASAN to intercept calls
#undef memcpy
#undef memset

// Re-declare standard functions since kernel string.h doesn't include them
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);

// Include source file to test
#include "../kernel/net/tcp.c"

int main() {
    socket_entry_t sock;
    memset(&sock, 0, sizeof(sock));
    sock.state = SOCKET_STATE_ESTABLISHED;
    sock.remote_ip = 0x0A000001;
    sock.remote_port = 80;
    sock.local_port = 12345;
    sock.seq_num = 1000;
    sock.ack_num = 2000;

    // Test Case 1: Normal packet
    printf("Test 1: Normal packet... ");
    char payload[100];
    memset(payload, 'A', sizeof(payload));
    int res = tcp_send_packet(&sock, payload, sizeof(payload), TCP_PSH | TCP_ACK);
    assert(res == 0);
    printf("Passed\n");

    // Test Case 2: Overflow packet
    printf("Test 2: Overflow packet... ");

    char* large_payload = malloc(3000);
    memset(large_payload, 'B', 3000);

    res = tcp_send_packet(&sock, large_payload, 2000, TCP_PSH | TCP_ACK);
    assert(res == -1);
    printf("Detected and rejected (Passed)\n");

    // Test Case 3: tcp_send overflow logic
    printf("Test 3: tcp_send overflow logic... ");
    uint32_t old_seq = sock.seq_num;

    res = tcp_send(&sock, large_payload, 2000);
    assert(res == -1);

    // Check sequence number
    assert(sock.seq_num == old_seq);
    printf("Rejected and sequence number preserved (Passed)\n");

    free(large_payload);

    return 0;
}
