#define __ETEROS_HOST_TEST__
#include <stdio.h>
/* Use custom declaration for exit to avoid collision with kernel stdlib.h */
void exit(int status);

#include <string.h>
#include <stdint.h>

/* Mock Globals to control the test */
uint8_t* mock_packet_data = NULL;
int mock_packet_len = 0;
int tcp_input_called = 0;
int tcp_input_len = 0;
uint8_t* tcp_input_ptr = NULL;

#include <sem.h>
/* Mocks */
void sem_init(sem_t* sem, int value) {
}

struct nic_driver;
extern struct nic_driver* current_nic;

int e1000_receive(void* buffer, uint16_t max_len) {
    if (mock_packet_data && mock_packet_len > 0) {
        int len = (mock_packet_len < (int)max_len) ? mock_packet_len : (int)max_len;
        memcpy(buffer, mock_packet_data, len);
        /* Clear mock so we don't return it again */
        mock_packet_data = NULL;
        return len;
    }
    return 0;
}

uint8_t* e1000_get_mac(void) {
    static uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    return mac;
}

int e1000_send_packet(const void* data, uint16_t len) {
    return 0;
}

void hal_console_write(const char* str) {
    // printf("[KERNEL] %s", str);
}

uint64_t timer_get_ticks(void) {
    return 1000;
}

void task_yield(void) {}

/* Include headers via relative path since we are in tests/ */
/* Note: We use -Iinclude during compilation */

#include <net/nic.h>
nic_driver_t mock_nic = {
    .name = "e1000",
    .init = NULL,
    .send = e1000_send_packet,
    .receive = e1000_receive,
    .get_mac = e1000_get_mac
};
nic_driver_t* current_nic = &mock_nic;

#include "kernel/net/core/stack.c"

/* Mock string operations that kernel uses */
void* eteros_memcpy(void* dest, const void* src, size_t n) {
    if (!dest || !src || n == 0) return dest;
    // Undefine the macro if it exists to avoid recursive call
    #undef memcpy
    return memcpy(dest, src, n);
}

void* eteros_memset(void* s, int c, size_t n) {
    if (!s || n == 0) return s;
    // Undefine the macro if it exists to avoid recursive call
    #undef memset
    return memset(s, c, n);
}

void itoa_s(int64_t value, char* str, size_t size, int base) {
    if (str && size > 0) {
        sprintf(str, "%ld", value);
    }
}

/* Implement functions needed by stack.c but not in it */

void tcp_input(socket_entry_t* sock, struct tcp_header* tcp, int len, uint32_t src_ip) {
    tcp_input_called++;
    tcp_input_len = len;
    tcp_input_ptr = (uint8_t*)tcp;
}

int tcp_connect(socket_entry_t* sock, uint32_t dest_ip, uint16_t dest_port) { return 0; }
int tcp_send(socket_entry_t* sock, const void* data, int len) { return 0; }
int tcp_close(socket_entry_t* sock) { return 0; }


/* Main Test */
void test_malformed_packet(const char* test_name, uint8_t* data, int len, int should_call_tcp_input) {
    printf("Running Test: %-30s ... ", test_name);

    mock_packet_data = data;
    mock_packet_len = len;
    tcp_input_called = 0;
    tcp_input_len = 0;
    tcp_input_ptr = NULL;

    net_poll();

    if (should_call_tcp_input) {
        if (tcp_input_called) {
            printf("PASS (Called)\n");
        } else {
            printf("FAIL (NOT Called)\n");
            exit(1);
        }
    } else {
        if (tcp_input_called) {
            printf("FAIL (Called Unexpectedly! Len=%d)\n", tcp_input_len);
            exit(1);
        } else {
            printf("PASS (Dropped)\n");
        }
    }
}

int main() {
    printf("Starting Stack Security Test...\n");

    /* Init Stack */
    net_init();

    /* Setup a socket */
    int sock_id = net_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_id < 0 || sock_id >= MAX_SOCKETS) {
        printf("Failed to create socket (id=%d)\n", sock_id);
        exit(1);
    }
    socket_entry_t* s = &socket_table[sock_id];
    s->local_port = 80; /* We will target port 80 */
    s->state = SOCKET_STATE_LISTEN; /* Listen state */

    /* Construct Base Valid Packet */
    uint8_t packet[200];
    memset(packet, 0, sizeof(packet));

    struct ethernet_header* eth = (struct ethernet_header*)packet;
    struct ip_header* ip = (struct ip_header*)(packet + 14);
    struct tcp_header* tcp = (struct tcp_header*)(packet + 14 + 20);

    /* Ethernet */
    eth->type = htons(ETHERNET_TYPE_IP);

    /* IP */
    ip->ver_ihl = 0x45; /* Ver 4, IHL 5 (20 bytes) */
    ip->len = htons(20 + 20); /* IP Header + TCP Header (no payload) */
    ip->proto = IP_PROTO_TCP;
    ip->dest = 0; /* my_ip is 0 initially, so it accepts all */

    /* TCP */
    tcp->dest_port = htons(80);
    tcp->flags = htons(0x5000); /* Data Offset 5 (20 bytes) */

    /* Test 1: Valid Packet (Minimum Size) */
    /* Eth(14) + IP(20) + TCP(20) = 54 bytes */
    test_malformed_packet("Valid Packet", packet, 54, 1);

    /* Test 2: Truncated Ethernet */
    test_malformed_packet("Truncated Ethernet", packet, 1, 0);

    /* Test 3: Truncated IP Header */
    test_malformed_packet("Truncated IP Header", packet, 14 + 10, 0);

    /* Test 4: Truncated TCP Header (IP says it's there, but packet ends) */
    test_malformed_packet("Truncated TCP Header", packet, 14 + 20 + 10, 0);

    /* Test 5: IP Length Mismatch (Over-read) */
    ip->len = htons(100);
    test_malformed_packet("IP Length > Received Length", packet, 54, 1);

    if (tcp_input_len > 20) {
         printf("FAIL: tcp_input called with len=%d > available(20). Buffer Over-read!\n", tcp_input_len);
         exit(1);
    } else {
         printf("PASS: tcp_input length clamped to %d\n", tcp_input_len);
    }

    /* Reset IP Len */
    ip->len = htons(20 + 20);

    /* Test 6: Invalid IHL */
    ip->ver_ihl = 0x4F; /* IHL 15 (60 bytes) */
    test_malformed_packet("Invalid IHL (Truncated IP)", packet, 54, 0);

    printf("All tests completed successfully.\n");
    return 0;
}
