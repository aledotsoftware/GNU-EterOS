/**
 * éterOS - Definiciones de Red Genéricas
 *
 * Estructuras y constantes para protocolos de red básicos (Ethernet, IP, UDP, TCP).
 */

#ifndef ETEROS_NET_DEFS_H
#define ETEROS_NET_DEFS_H

#include "../types.h"

/* Network Constants */
#define ETHERNET_TYPE_IP  0x0800
#define ETHERNET_TYPE_ARP 0x0806
#define IP_PROTO_UDP 17
#define IP_PROTO_TCP 6

/* Byte Order Utils (x86 is Little Endian) */
static inline uint16_t htons(uint16_t v) { return (v << 8) | (v >> 8); }
static inline uint32_t htonl(uint32_t v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v & 0xFF0000) >> 8) | ((v >> 24) & 0xFF);
}
#define ntohs htons
#define ntohl htonl

/* Structures */
struct ethernet_header {
    uint8_t dest[6];
    uint8_t src[6];
    uint16_t type;
} __attribute__((packed));

struct ip_header {
    uint8_t  ver_ihl;   /* Version (4) + IHL (4) */
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

struct udp_header {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t len;
    uint16_t checksum;
} __attribute__((packed));

struct arp_packet {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t op;
    uint8_t  src_mac[6];
    uint32_t src_ip;
    uint8_t  dest_mac[6];
    uint32_t dest_ip;
} __attribute__((packed));

struct tcp_header {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq;
    uint32_t ack_seq;
    uint16_t flags; /* [4 bits offset][6 bits reserved][6 bits flags] */
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

#define TCP_FIN (1 << 0)
#define TCP_SYN (1 << 1)
#define TCP_RST (1 << 2)
#define TCP_PSH (1 << 3)
#define TCP_ACK (1 << 4)

/* Checksum helper (implemented in stack.c) */
uint16_t net_checksum(void* vdata, size_t length);

/* IP Utils (kernel/net/ip_utils.c) */
uint32_t ip_aton(const char* cp);

/* Global Network State (stack.c) */
extern int network_ready;
extern uint32_t my_ip;
extern uint32_t gateway_ip;
extern uint32_t dns_ip;
extern uint8_t gateway_mac[6];

#endif /* ETEROS_NET_DEFS_H */
