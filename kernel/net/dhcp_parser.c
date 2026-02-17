#include "../../include/net/dhcp.h"
#include "../../include/net/defs.h"
#include "../../include/types.h"
#include "../../include/string.h"

/**
 * Parses a DHCP offer packet from a raw buffer.
 * Validates Ethernet, IP, UDP, and DHCP headers.
 */
int dhcp_parse_offer(const uint8_t* buffer, size_t len, uint32_t xid, const struct dhcp_packet** out_dhcp) {
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
    return 0;
}

int dhcp_parse_options(const struct dhcp_packet* dhcp, size_t packet_len, uint32_t* out_mask, uint32_t* out_gw, uint32_t* out_dns) {
    if (dhcp == NULL) return -1;

    /* Initialize outputs */
    if (out_mask) *out_mask = 0;
    if (out_gw) *out_gw = 0;
    if (out_dns) *out_dns = 0;

    /* Calculate bounds */
    /* dhcp points to the start of DHCP header. packet_len is the number of valid bytes starting at dhcp. */
    const uint8_t* start = (const uint8_t*)dhcp;
    const uint8_t* end = start + packet_len;

    /* Ensure we at least have the fixed header */
    size_t fixed_size = sizeof(struct dhcp_packet) - 308;
    if (packet_len < fixed_size) {
        return -1;
    }

    /* Start parsing options */
    const uint8_t* opt = dhcp->options;

    while (opt < end) {
        /* Check if we can read the type byte */
        if (opt >= end) break;

        uint8_t type = *opt;

        if (type == 255) { /* End */
            break;
        }

        if (type == 0) { /* Pad */
            opt++;
            continue;
        }

        /* Check if we can read the length byte */
        if (opt + 1 >= end) {
            break; /* Truncated */
        }

        uint8_t len = *(opt + 1);

        /* Check if the value is within bounds */
        if (opt + 2 + len > end) {
            break; /* Truncated value */
        }

        const uint8_t* val = opt + 2;

        /* Parse interest options */
        if (type == 1 && len == 4) { /* Subnet Mask */
            if (out_mask) memcpy(out_mask, val, 4);
        } else if (type == 3 && len >= 4) { /* Router/Gateway */
            if (out_gw && *out_gw == 0) memcpy(out_gw, val, 4);
        } else if (type == 6 && len >= 4) { /* DNS */
            if (out_dns && *out_dns == 0) memcpy(out_dns, val, 4);
        }

        /* Advance */
        opt += 2 + len;
    }

    return 0;
}
