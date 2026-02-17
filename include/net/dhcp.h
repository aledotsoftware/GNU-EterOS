/**
 * éterOS - DHCP Client Definitions
 */

#ifndef ETEROS_NET_DHCP_H
#define ETEROS_NET_DHCP_H

#include "../types.h"
#include "defs.h"

#define DHCP_MAGIC 0x63825363

struct dhcp_packet {
    uint8_t  op;        /* 1: request, 2: reply */
    uint8_t  htype;     /* 1: ethernet */
    uint8_t  hlen;      /* 6 */
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic;
    uint8_t  options[308]; /* Minimum size */
} __attribute__((packed));

void dhcp_discover(void);

/**
 * Parses a DHCP offer packet from a raw buffer.
 *
 * @param buffer Pointer to the raw packet buffer (starting with Ethernet header).
 * @param len Total length of the received buffer.
 * @param xid The transaction ID to match.
 * @param out_dhcp Output pointer to the start of the DHCP packet within the buffer.
 * @param out_dhcp_len Output pointer to the length of the DHCP payload (including options).
 *
 * @return 0 on success (valid offer matching xid), -1 on error or mismatch.
 */
int dhcp_parse_offer(const uint8_t* buffer, size_t len, uint32_t xid, const struct dhcp_packet** out_dhcp, size_t* out_dhcp_len);

/**
 * Parses DHCP options from a packet with strict bounds checking.
 *
 * @param packet Pointer to the DHCP packet.
 * @param len Length of the DHCP packet (header + options).
 * @param mask Output pointer for Subnet Mask.
 * @param gw Output pointer for Gateway IP.
 * @param dns Output pointer for DNS IP.
 *
 * @return 0 on success, -1 on error.
 */
int dhcp_parse_options(const struct dhcp_packet* packet, size_t len, uint32_t* mask, uint32_t* gw, uint32_t* dns);

#endif /* ETEROS_NET_DHCP_H */
