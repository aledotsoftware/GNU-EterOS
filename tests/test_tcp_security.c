/* Defines to block kernel headers */
#define ETEROS_NET_DEFS_H
#define ETEROS_NET_SOCKET_H
#define ETEROS_NET_E1000_H
#define ETEROS_STRING_H
#define ETEROS_TIMER_H
#define ETEROS_TASK_H
#define ETEROS_HAL_H

/* System headers */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* Mock Types/Constants from blocked headers */

/* net/defs.h */
#define ETHERNET_TYPE_IP  0x0800
#define IP_PROTO_TCP 6
#define htons(x) ((uint16_t)((((x) & 0xff) << 8) | (((x) & 0xff00) >> 8)))
#define htonl(x) ((uint32_t)((((x) & 0xff) << 24) | (((x) & 0xff00) << 8) | \
                  (((x) & 0xff0000) >> 8) | (((x) >> 24) & 0xff)))
#define ntohs htons
#define ntohl htonl

struct ethernet_header {
    uint8_t dest[6];
    uint8_t src[6];
    uint16_t type;
} __attribute__((packed));

struct ip_header {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t len;
    uint16_t id;
    uint16_t frag_offset;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t checksum;
    uint32_t src;
    uint32_t dest;
} __attribute__((packed));

struct tcp_header {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq;
    uint32_t ack_seq;
    uint16_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed));

struct pseudo_header {
    uint32_t src;
    uint32_t dest;
    uint8_t zero;
    uint8_t proto;
    uint16_t len;
} __attribute__((packed));

#define TCP_ACK (1 << 4)
#define TCP_PSH (1 << 3)
#define TCP_SYN (1 << 1)
#define TCP_FIN (1 << 0)

/* net/socket.h */
#define RX_BUFFER_SIZE 4096
#define SOCKET_STATE_ESTABLISHED 4
#define SOCKET_STATE_SYN_SENT    2
#define SOCKET_STATE_CLOSE_WAIT  7
#define SOCKET_STATE_FIN_WAIT1   5
#define SOCKET_STATE_FIN_WAIT2   6
#define SOCKET_STATE_TIME_WAIT   10
#define SOCKET_STATE_CLOSING     8

typedef int socket_t; /* needed for tcp_input signature inside tcp.c? No, tcp.c uses socket_entry_t* */
/* But tcp.c functions use socket_entry_t */

typedef struct {
    int id;
    int used;
    int state;
    int type;
    int protocol;
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t window;
    uint8_t rx_buf[RX_BUFFER_SIZE];
    int rx_head;
    int rx_tail;
} socket_entry_t;

/* Mocks */
uint32_t my_ip = 0x01020304;
uint8_t gateway_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
uint8_t mock_mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

uint8_t* e1000_get_mac(void) { return mock_mac; }

int e1000_send_packet(const void* data, uint16_t len) {
    if (len > 1514) {
        printf("VULNERABILITY CONFIRMED: e1000_send_packet called with len %d (> 1514)\n", len);
        /* In a real exploit, this would be the point where we know stack is smashed. */
        return -1;
    }
    printf("e1000_send_packet called with len %d. Safe.\n", len);
    return 0;
}

#include <net/nic.h>

nic_driver_t mock_nic = {
    .name = "mock",
    .get_mac = e1000_get_mac,
    .send = e1000_send_packet,
    .receive = NULL
};

nic_driver_t* current_nic = &mock_nic;

uint16_t net_checksum(void* vdata, size_t length) { return 0; }
void hal_console_write(const char* str) { printf("[KERNEL] %s", str); }
uint64_t timer_get_ticks(void) { return 1000; }
void task_yield(void) {}
void net_poll(void) {}

/* Include source */
#include "../kernel/net/core/tcp.c"

int main() {
    printf("Starting TCP Security Test...\n");
    socket_entry_t sock;
    memset(&sock, 0, sizeof(sock));
    sock.state = SOCKET_STATE_ESTABLISHED;
    sock.remote_ip = 0x0A000001;
    sock.remote_port = 80;
    sock.local_port = 12345;
    sock.seq_num = 100;

    int payload_len = 2000;
    uint8_t* large_payload = (uint8_t*)malloc(payload_len);
    memset(large_payload, 'A', payload_len);

    printf("Attempting to send %d bytes payload...\n", payload_len);

    /* We expect this to call e1000_send_packet with len ~2054 if vulnerable */
    /* Or return error/print warning if fixed */
    int ret = tcp_send_packet(&sock, large_payload, payload_len, TCP_PSH | TCP_ACK);
    if (ret == -1) {
        printf("SUCCESS: Large packet blocked correctly.\n");
    } else {
        printf("FAILURE: Large packet allowed (ret=%d).\n", ret);
    }

    free(large_payload);

    /* Verify normal packet works */
    printf("Sending normal packet (100 bytes)...\n");
    uint8_t normal_payload[100];
    memset(normal_payload, 'B', 100);
    ret = tcp_send_packet(&sock, normal_payload, 100, TCP_PSH | TCP_ACK);
    if (ret != -1) {
        printf("SUCCESS: Normal packet sent.\n");
    } else {
        printf("FAILURE: Normal packet blocked.\n");
    }

    /* Test receive buffer overflow */
    printf("Testing receive buffer overflow...\n");
    socket_entry_t test_sock;
    memset(&test_sock, 0, sizeof(test_sock));
    test_sock.state = SOCKET_STATE_ESTABLISHED;
    test_sock.rx_head = 0;
    test_sock.rx_tail = 0;

    struct tcp_header fake_tcp;
    memset(&fake_tcp, 0, sizeof(fake_tcp));
    fake_tcp.flags = htons(5 << 12);

    int large_rx_len = 5000; /* Greater than RX_BUFFER_SIZE (4096) */
    uint8_t* fake_packet = (uint8_t*)malloc(sizeof(struct tcp_header) + large_rx_len);
    memcpy(fake_packet, &fake_tcp, sizeof(struct tcp_header));
    memset(fake_packet + sizeof(struct tcp_header), 'C', large_rx_len);

    uint32_t old_rx_head = test_sock.rx_head;
    tcp_input(&test_sock, (struct tcp_header*)fake_packet, sizeof(struct tcp_header) + large_rx_len, 0);

    if (test_sock.rx_head == old_rx_head) {
        printf("SUCCESS: Large rx packet dropped correctly.\n");
    } else {
        printf("FAILURE: Large rx packet processed (buffer overflowed!).\n");
        return -1;
    }
    free(fake_packet);

    return 0;
}
