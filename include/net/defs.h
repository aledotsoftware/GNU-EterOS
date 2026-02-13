/**
 * éterOS - Definiciones de Red Genéricas
 *
 * Estructuras y constantes para protocolos de red básicos (Ethernet, IP, UDP).
 */

#ifndef ETEROS_NET_DEFS_H
#define ETEROS_NET_DEFS_H

#include "../types.h"

/* Network Constants */
#define ETHERNET_TYPE_IP 0x0800
#define IP_PROTO_UDP 17

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

#endif /* ETEROS_NET_DEFS_H */
