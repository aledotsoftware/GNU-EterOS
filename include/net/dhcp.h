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

/**
 * Parses DHCP options safely.
 *
 * @param dhcp Pointer to the DHCP packet struct (already validated by parse_offer).
 * @param packet_len Total length of the received packet buffer.
 * @param out_mask Pointer to store Subnet Mask (0 if not found).
 * @param out_gw Pointer to store Gateway IP (0 if not found).
 * @param out_dns Pointer to store DNS IP (0 if not found).
 * @return 0 on success (even if options are missing), -1 on critical parsing error.
 */
int dhcp_parse_options(const struct dhcp_packet* dhcp, size_t packet_len, uint32_t* out_mask, uint32_t* out_gw, uint32_t* out_dns);

#endif /* ETEROS_NET_DHCP_H */
