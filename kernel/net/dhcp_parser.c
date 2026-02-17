#include "../../include/net/dhcp.h"
#include "../../include/net/defs.h"
#include "../../include/types.h"
#include "../../include/string.h"

/**
 * Parses a DHCP offer packet from a raw buffer.
 * Validates Ethernet, IP, UDP, and DHCP headers.
 */
int dhcp_parse_offer(const uint8_t* buffer, size_t len, uint32_t xid, const struct dhcp_packet** out_dhcp, size_t* out_dhcp_len) {
    if (buffer == NULL || out_dhcp == NULL) {
        return -1;
    }

    /* 1. Check Ethernet Header */
    if (len < sizeof(struct ethernet_header)) {
        return -1;
    }

    const struct ethernet_header* eth = (const struct ethernet_header*)buffer;
    if (eth->type != htons(ETHERNET_TYPE_IP)) {
        return -1;
    }

    /* 2. Check IP Header */
    size_t ip_offset = sizeof(struct ethernet_header);
    if (len < ip_offset + sizeof(struct ip_header)) {
        return -1;
    }

    const struct ip_header* ip = (const struct ip_header*)(buffer + ip_offset);
    if (ip->proto != IP_PROTO_UDP) {
        return -1;
    }

    /* Calculate IP header length from IHL */
    uint8_t ihl = (ip->ver_ihl & 0x0F);
    size_t ip_header_len = ihl * 4;

    if (ip_header_len < 20) {
        return -1; /* Invalid IHL */
    }

    if (len < ip_offset + ip_header_len) {
        return -1; /* Buffer too short for IP header */
    }

    /* 3. Check UDP Header */
    size_t udp_offset = ip_offset + ip_header_len;
    if (len < udp_offset + sizeof(struct udp_header)) {
        return -1;
    }

    const struct udp_header* udp = (const struct udp_header*)(buffer + udp_offset);

    /* Check Destination Port (Client: 68) */
    if (udp->dest_port != htons(68)) {
        return -1;
    }

    /* Verify UDP length */
    uint16_t udp_len = ntohs(udp->len);
    if (udp_len < sizeof(struct udp_header)) {
        return -1;
    }

    /* Ensure buffer has enough data for the full UDP packet */
    if (len < udp_offset + udp_len) {
        return -1;
    }

    /* 4. Check DHCP Packet */
    size_t dhcp_offset = udp_offset + sizeof(struct udp_header);
    size_t dhcp_len = udp_len - sizeof(struct udp_header);

    /* Check fixed part of DHCP packet (excluding options array for flexibility,
       but typically we expect the full struct) */
    /* The struct dhcp_packet includes options[308].
       We definitely need the fixed header (240 bytes). */
    size_t fixed_dhcp_size = sizeof(struct dhcp_packet) - 308;

    if (dhcp_len < fixed_dhcp_size) {
        return -1;
    }

    /* Ensure the buffer actually contains this data (redundant with udp_len check but safe) */
    if (len < dhcp_offset + fixed_dhcp_size) {
        return -1;
    }

    const struct dhcp_packet* dhcp = (const struct dhcp_packet*)(buffer + dhcp_offset);

    /* Validate Transaction ID and Op Code (Reply = 2) */
    if (dhcp->xid != htonl(xid)) {
        return -1;
    }

    if (dhcp->op != 2) {
        return -1;
    }

    *out_dhcp = dhcp;
    if (out_dhcp_len) {
        *out_dhcp_len = dhcp_len;
    }
    return 0;
}

/**
 * Parses DHCP options from a packet with strict bounds checking.
 */
int dhcp_parse_options(const struct dhcp_packet* packet, size_t len, uint32_t* mask, uint32_t* gw, uint32_t* dns) {
    if (!packet || !mask || !gw || !dns) return -1;

    /* The 'len' parameter is the size of the DHCP payload (struct + options).
       We must ensure we don't read past (packet + len). */
    const uint8_t* buffer_end = (const uint8_t*)packet + len;

    /* Ensure packet->options is within the buffer */
    const uint8_t* options_start = packet->options;
    if (options_start >= buffer_end) {
        /* No options present or truncated */
        return 0;
    }

    const uint8_t* opt = options_start;

    while (opt < buffer_end) {
        if (*opt == 255) break; /* End Option */

        if (*opt == 0) { /* Padding */
            opt++;
            continue;
        }

        /* Need at least 2 bytes for Type + Length */
        if (opt + 2 > buffer_end) break;

        uint8_t type = opt[0];
        uint8_t length = opt[1];

        /* Move to data */
        opt += 2;

        /* Check if the option value fits in the buffer */
        if (opt + length > buffer_end) break;

        /* Process Option */
        if (type == 1 && length >= 4) { /* Subnet Mask */
            memcpy(mask, opt, 4);
        } else if (type == 3 && length >= 4) { /* Router/Gateway */
            memcpy(gw, opt, 4);
        } else if (type == 6 && length >= 4) { /* DNS */
            memcpy(dns, opt, 4);
        }

        /* Advance to next option */
        opt += length;
    }

    return 0;
}
