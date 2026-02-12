/**
 * éterOS - Simple DHCP Client
 * Implementación básica de DHCP Discover -> Offer.
 */

#include "../../include/net/dhcp.h"
#include "../../include/net/e1000.h"
#include "../../include/string.h"
#include "../../include/vga.h"
#include "../../include/mm.h"
#include "../../include/io.h"
#include "../../include/timer.h"

/* Network Constants */
#define ETHERNET_TYPE_IP 0x0800
#define IP_PROTO_UDP 17
#define DHCP_MAGIC 0x63825363

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

/* Checksum Helper */
/* PRNG: Xorshift32 */
static uint32_t xorshift_state = 0xACE1; /* Initial seed */

static uint32_t rand32(void) {
    uint32_t x = xorshift_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    xorshift_state = x;
    return x;
}

static void srand(uint32_t seed) {
    if (seed == 0) seed = 12345; /* Seed cannot be zero */
    xorshift_state = seed;
}

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
    
    /* Seed PRNG with TSC and current ticks for better entropy */
    srand((uint32_t)rdtsc() ^ (uint32_t)timer_get_ticks());
    uint32_t xid = rand32();
    
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
    
    terminal_write_colored("[DHCP] Enviando Discover...\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    e1000_send_packet(buffer, total_len);
    
    /* 6. Wait for response (Polling with simple loop timer) */
    /* Wait approx 3 seconds. */
    char numbuf[16];
    
    uint8_t rx_buffer[1514];
    int attempts = 50000; /* Polling loop count */
    
    while(attempts--) {
        int len = e1000_receive(rx_buffer, sizeof(rx_buffer));
        if (len > 0) {
            /* Check if it's IP/UDP */
            struct ethernet_header* r_eth = (struct ethernet_header*)rx_buffer;
            if (r_eth->type == htons(ETHERNET_TYPE_IP)) {
                struct ip_header* r_ip = (struct ip_header*)(rx_buffer + sizeof(struct ethernet_header));
                if (r_ip->proto == IP_PROTO_UDP) {
                    struct udp_header* r_udp = (struct udp_header*)((uint8_t*)r_ip + (r_ip->ver_ihl & 0xF)*4);
                    
                    /* Check Port 68 */
                    if (r_udp->dest_port == htons(68)) {
                         struct dhcp_packet* r_dhcp = (struct dhcp_packet*)((uint8_t*)r_udp + sizeof(struct udp_header));
                         
                         if (r_dhcp->xid == htonl(xid) && r_dhcp->op == 2 /* Reply */) {
                             terminal_write_colored("[DHCP] Oferta Recibida!\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                             
                             /* Print Offered IP (yiaddr) */
                             uint8_t* ip = (uint8_t*)&r_dhcp->yiaddr;
                             
                             terminal_write_string("  IP Address: ");
                             itoa_s(ip[0], numbuf, sizeof(numbuf), 10); terminal_write_string(numbuf); terminal_write_string(".");
                             itoa_s(ip[1], numbuf, sizeof(numbuf), 10); terminal_write_string(numbuf); terminal_write_string(".");
                             itoa_s(ip[2], numbuf, sizeof(numbuf), 10); terminal_write_string(numbuf); terminal_write_string(".");
                             itoa_s(ip[3], numbuf, sizeof(numbuf), 10); terminal_write_string(numbuf);
                             terminal_write_string("\n");
                             
                             return;
                         }
                    }
                }
            }
        }
        
        /* Small delay loop */
        for(volatile int k=0; k<1000; k++);
    }
    
    terminal_write_colored("[DHCP] Timeout. Sin respuesta.\n", VGA_COLOR_RED, VGA_COLOR_BLACK);
}
