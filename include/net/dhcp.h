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
 *
 * @return 0 on success (valid offer matching xid), -1 on error or mismatch.
 */
int dhcp_parse_offer(const uint8_t* buffer, size_t len, uint32_t xid, const struct dhcp_packet** out_dhcp);

#endif /* ETEROS_NET_DHCP_H */
