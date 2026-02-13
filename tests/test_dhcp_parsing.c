/*
 * Test for DHCP Parsing
 * Run: gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_dhcp_parsing.c kernel/net/dhcp_parser.c kernel/string.c -o tests/test_dhcp_parsing && ./tests/test_dhcp_parsing
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

/* Include project headers */
#include <net/dhcp.h>
#include <net/defs.h>

/* Helper to build a valid packet */
size_t build_valid_packet(uint8_t* buffer, uint32_t xid) {
    memset(buffer, 0, 1000);

    struct ethernet_header* eth = (struct ethernet_header*)buffer;
    eth->type = htons(ETHERNET_TYPE_IP);

    struct ip_header* ip = (struct ip_header*)(buffer + sizeof(struct ethernet_header));
    ip->ver_ihl = 0x45; /* Ver 4, IHL 5 */
    ip->proto = IP_PROTO_UDP;
    size_t ip_len = 20;

    struct udp_header* udp = (struct udp_header*)(buffer + sizeof(struct ethernet_header) + ip_len);
    udp->dest_port = htons(68);

    /* Fixed DHCP part size: 240 bytes (approx) */
    /* struct dhcp_packet size is 544+ bytes including options */
    size_t dhcp_size = sizeof(struct dhcp_packet);

    udp->len = htons(sizeof(struct udp_header) + dhcp_size);

    struct dhcp_packet* dhcp = (struct dhcp_packet*)(buffer + sizeof(struct ethernet_header) + ip_len + sizeof(struct udp_header));
    dhcp->xid = htonl(xid);
    dhcp->op = 2; /* Reply */
    dhcp->magic = htonl(DHCP_MAGIC);

    return sizeof(struct ethernet_header) + ip_len + sizeof(struct udp_header) + dhcp_size;
}

void test_valid_packet() {
    uint8_t buffer[1000];
    uint32_t xid = 0x12345678;
    size_t len = build_valid_packet(buffer, xid);

    const struct dhcp_packet* out_dhcp = NULL;
    int res = dhcp_parse_offer(buffer, len, xid, &out_dhcp);

    if (res != 0) {
        printf("FAIL: test_valid_packet returned %d\n", res);
        exit(1);
    }
    if (out_dhcp == NULL) {
        printf("FAIL: test_valid_packet out_dhcp is NULL\n");
        exit(1);
    }
    if (out_dhcp->xid != htonl(xid)) {
        printf("FAIL: test_valid_packet xid mismatch\n");
        exit(1);
    }
    printf("PASS: test_valid_packet\n");
}

void test_short_buffer() {
    uint8_t buffer[1000];
    uint32_t xid = 0x12345678;
    size_t len = build_valid_packet(buffer, xid);

    const struct dhcp_packet* out_dhcp = NULL;

    /* Try passing a shorter length than required */
    /* 1. Cut Ethernet header */
    if (dhcp_parse_offer(buffer, sizeof(struct ethernet_header) - 1, xid, &out_dhcp) == 0) {
        printf("FAIL: test_short_buffer (ethernet) should have failed\n");
        exit(1);
    }

    /* 2. Cut IP header */
    if (dhcp_parse_offer(buffer, sizeof(struct ethernet_header) + sizeof(struct ip_header) - 1, xid, &out_dhcp) == 0) {
        printf("FAIL: test_short_buffer (ip) should have failed\n");
        exit(1);
    }

    /* 3. Cut UDP header */
    if (dhcp_parse_offer(buffer, sizeof(struct ethernet_header) + 20 + sizeof(struct udp_header) - 1, xid, &out_dhcp) == 0) {
        printf("FAIL: test_short_buffer (udp) should have failed\n");
        exit(1);
    }

    /* 4. Cut DHCP header */
    /* The parser requires at least fixed size of DHCP */
    size_t dhcp_offset = sizeof(struct ethernet_header) + 20 + sizeof(struct udp_header);
    /* Try passing length just before end of fixed part */
    if (dhcp_parse_offer(buffer, dhcp_offset + 239, xid, &out_dhcp) == 0) {
        printf("FAIL: test_short_buffer (dhcp) should have failed\n");
        exit(1);
    }

    printf("PASS: test_short_buffer\n");
}

void test_invalid_values() {
    uint8_t buffer[1000];
    uint32_t xid = 0x12345678;
    size_t len = build_valid_packet(buffer, xid);

    const struct dhcp_packet* out_dhcp = NULL;

    /* 1. Wrong Ethernet Type */
    struct ethernet_header* eth = (struct ethernet_header*)buffer;
    uint16_t orig_type = eth->type;
    eth->type = htons(0x1234);
    if (dhcp_parse_offer(buffer, len, xid, &out_dhcp) == 0) {
        printf("FAIL: test_invalid_values (eth type) should have failed\n");
        exit(1);
    }
    eth->type = orig_type;

    /* 2. Wrong IP Proto */
    struct ip_header* ip = (struct ip_header*)(buffer + sizeof(struct ethernet_header));
    uint8_t orig_proto = ip->proto;
    ip->proto = 6; /* TCP */
    if (dhcp_parse_offer(buffer, len, xid, &out_dhcp) == 0) {
        printf("FAIL: test_invalid_values (ip proto) should have failed\n");
        exit(1);
    }
    ip->proto = orig_proto;

    /* 3. Wrong UDP Port */
    struct udp_header* udp = (struct udp_header*)(buffer + sizeof(struct ethernet_header) + 20);
    uint16_t orig_port = udp->dest_port;
    udp->dest_port = htons(80);
    if (dhcp_parse_offer(buffer, len, xid, &out_dhcp) == 0) {
        printf("FAIL: test_invalid_values (udp port) should have failed\n");
        exit(1);
    }
    udp->dest_port = orig_port;

    /* 4. Wrong XID */
    if (dhcp_parse_offer(buffer, len, xid + 1, &out_dhcp) == 0) {
        printf("FAIL: test_invalid_values (xid) should have failed\n");
        exit(1);
    }

    /* 5. Wrong Op Code */
    struct dhcp_packet* dhcp = (struct dhcp_packet*)(buffer + sizeof(struct ethernet_header) + 20 + sizeof(struct udp_header));
    dhcp->op = 1; /* Request, not Reply */
    if (dhcp_parse_offer(buffer, len, xid, &out_dhcp) == 0) {
        printf("FAIL: test_invalid_values (op) should have failed\n");
        exit(1);
    }
    dhcp->op = 2;

    printf("PASS: test_invalid_values\n");
}

int main() {
    test_valid_packet();
    test_short_buffer();
    test_invalid_values();
    return 0;
}
