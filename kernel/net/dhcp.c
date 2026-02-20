/**
 * éterOS - Simple DHCP Client
 * Implementación básica de DHCP Discover -> Offer.
 */

#include "../../include/net/dhcp.h"
#include "../../include/net/e1000.h"
#include "../../include/string.h"
#include "../../include/vga.h"
#include "../../include/mm.h"
#include "../../include/timer.h"
#include "../../include/hal.h"

/* Constants and structs are now in include/net/dhcp.h and include/net/defs.h */

/* Checksum Helper */
static uint16_t ip_checksum(void* vdata, size_t length) {
    char* data = (char*)vdata;
    uint32_t acc = 0;
    for (size_t i = 0; i + 1 < length; i += 2) {
        uint16_t word;
        memcpy(&word, data + i, 2);
        acc += word;
    }
    if (length & 1) {
        uint16_t word = 0;
        memcpy(&word, data + length - 1, 1);
        acc += word;
    }
    while (acc >> 16) acc = (acc & 0xFFFF) + (acc >> 16);
    return (uint16_t)~acc;
}

void dhcp_discover(void) {
    uint8_t buffer[600]; /* Size enough for DHCP packet */
    memset(buffer, 0, sizeof(buffer));
    
    uint8_t* mac = e1000_get_mac();
    
    struct ethernet_header* eth = (struct ethernet_header*)buffer;
    struct ip_header* ip = (struct ip_header*)(buffer + sizeof(struct ethernet_header));
    struct udp_header* udp = (struct udp_header*)(buffer + sizeof(struct ethernet_header) + sizeof(struct ip_header));
    struct dhcp_packet* dhcp = (struct dhcp_packet*)(buffer + sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct udp_header));
    
    uint32_t xid = 0x12345678; /* Static XID */
    
    /* 1. Build DHCP Payload */
    dhcp->op = 1;       /* Boot Request */
    dhcp->htype = 1;    /* Ethernet */
    dhcp->hlen = 6;     /* MAC Len */
    dhcp->xid = htonl(xid);
    dhcp->magic = htonl(DHCP_MAGIC);
    memcpy(dhcp->chaddr, mac, 6);
    
    /* Options: Message Type = Discover */
    int i = 0;
    dhcp->options[i++] = 53; /* Option 53: Message Type */
    dhcp->options[i++] = 1;  /* Len */
    dhcp->options[i++] = 1;  /* 1 = Discover */
    dhcp->options[i++] = 255; /* End */
    
    /* 2. Build UDP Header */
    uint16_t dhcp_len = sizeof(struct dhcp_packet);
    uint16_t udp_len = sizeof(struct udp_header) + dhcp_len;
    
    udp->src_port = htons(68);
    udp->dest_port = htons(67);
    udp->len = htons(udp_len);
    udp->checksum = 0; /* Optional in IPv4 */
    
    /* 3. Build IP Header */
    ip->ver_ihl = 0x45; /* Ver 4, IHL 5 words (20 bytes) */
    ip->tos = 0;
    ip->len = htons(sizeof(struct ip_header) + udp_len);
    ip->id = htons(1);
    ip->frag_offset = 0;
    ip->ttl = 64;
    ip->proto = IP_PROTO_UDP;
    ip->src = 0;          /* 0.0.0.0 */
    ip->dest = 0xFFFFFFFF; /* 255.255.255.255 */
    ip->checksum = 0;
    ip->checksum = ip_checksum(ip, sizeof(struct ip_header));
    
    /* 4. Build Ethernet Header */
    memset(eth->dest, 0xFF, 6); /* Broadcast */
    memcpy(eth->src, mac, 6);
    eth->type = htons(ETHERNET_TYPE_IP);
    
    /* 5. Send Packet */
    uint16_t total_len = sizeof(struct ethernet_header) + sizeof(struct ip_header) + udp_len;
    
    hal_console_write("[DHCP] Enviando Discover...\n");
    if (e1000_send_packet(buffer, total_len) < 0) {
        hal_console_write("[DHCP] Error: No se pudo enviar el paquete.\n");
        return;
    }
    
    /* 6. Wait for response (Polling with simple loop timer) */
    /* Wait approx 3 seconds. */
    char numbuf[16];
    
    uint8_t rx_buffer[1514];
    uint64_t start_ticks = timer_get_ticks();
    uint64_t timeout_ticks = 5 * TIMER_HZ; /* 5 seconds */
    
    while(timer_get_ticks() - start_ticks < timeout_ticks) {
        int len = e1000_receive(rx_buffer, sizeof(rx_buffer));
        if (len > 0) {
            const struct dhcp_packet* r_dhcp = NULL;
            size_t dhcp_len = 0;
            if (dhcp_parse_offer(rx_buffer, len, xid, &r_dhcp, &dhcp_len) == 0) {
                 hal_console_write("[DHCP] Oferta Recibida!\n");

                 /* Parse Options for Mask, Gateway, DNS */
                 uint32_t mask = 0;
                 uint32_t gw = 0;
                 uint32_t dns = 0;

                 dhcp_parse_options(r_dhcp, dhcp_len, &mask, &gw, &dns);

                 /* Print Offered IP (yiaddr) */
                 const uint8_t* ip = (const uint8_t*)&r_dhcp->yiaddr;
                 hal_console_write("  IP Address: ");
                 itoa_s(ip[0], numbuf, sizeof(numbuf), 10); hal_console_write(numbuf); hal_console_write(".");
                 itoa_s(ip[1], numbuf, sizeof(numbuf), 10); hal_console_write(numbuf); hal_console_write(".");
                 itoa_s(ip[2], numbuf, sizeof(numbuf), 10); hal_console_write(numbuf); hal_console_write(".");
                 itoa_s(ip[3], numbuf, sizeof(numbuf), 10); hal_console_write(numbuf);
                 hal_console_write("\n");

                 if (gw != 0) {
                     const uint8_t* gip = (const uint8_t*)&gw;
                     hal_console_write("  Gateway:    ");
                     itoa_s(gip[0], numbuf, sizeof(numbuf), 10); hal_console_write(numbuf); hal_console_write(".");
                     itoa_s(gip[1], numbuf, sizeof(numbuf), 10); hal_console_write(numbuf); hal_console_write(".");
                     itoa_s(gip[2], numbuf, sizeof(numbuf), 10); hal_console_write(numbuf); hal_console_write(".");
                     itoa_s(gip[3], numbuf, sizeof(numbuf), 10); hal_console_write(numbuf);
                     hal_console_write("\n");
                 }

                 /* Store in Global Stack */
                 my_ip = r_dhcp->yiaddr;
                 gateway_ip = (gw != 0) ? gw : ((my_ip & 0x00FFFFFF) | 0x02000000); 
                 dns_ip = (dns != 0) ? dns : 0x08080808; 
                 network_ready = 1;

                 return;
            }
        }
        
        /* Yield CPU until next interrupt (timer or packet) */
        timer_sleep(10);
    }
    
    hal_console_write("[DHCP] Timeout. Sin respuesta de servidor DHCP.\n");
}
