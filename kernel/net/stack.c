#include <net/defs.h>
#include <net/e1000.h>
#include <string.h>
#include <vga.h>
#include <timer.h>
#include <task.h>

/* Global Network Info */
uint32_t my_ip = 0;
uint32_t gateway_ip = 0;
uint32_t dns_ip = 0;
uint8_t gateway_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
int network_ready = 0;

/* Checksum helper for IP/TCP */
uint16_t net_checksum(void* vdata, size_t length) {
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

/* Very simple ARP lookup */
int net_arp_lookup(uint32_t target_ip) {
    uint8_t buffer[64];
    memset(buffer, 0, sizeof(buffer));
    
    struct ethernet_header* eth = (struct ethernet_header*)buffer;
    struct arp_packet* arp = (struct arp_packet*)(buffer + sizeof(struct ethernet_header));
    
    uint8_t* my_mac = e1000_get_mac();
    
    /* Ethernet */
    memset(eth->dest, 0xFF, 6);
    memcpy(eth->src, my_mac, 6);
    eth->type = htons(ETHERNET_TYPE_ARP);
    
    /* ARP */
    arp->htype = htons(1);
    arp->ptype = htons(ETHERNET_TYPE_IP);
    arp->hlen = 6;
    arp->plen = 4;
    arp->op = htons(1); /* Request */
    memcpy(arp->src_mac, my_mac, 6);
    arp->src_ip = my_ip;
    memset(arp->dest_mac, 0, 6);
    arp->dest_ip = target_ip;
    
    e1000_send_packet(buffer, sizeof(struct ethernet_header) + sizeof(struct arp_packet));
    
    /* Polling for response */
    uint8_t rx[1514];
    uint64_t start = timer_get_ticks();
    while (timer_get_ticks() - start < 100) {
        int len = e1000_receive(rx, sizeof(rx));
        if (len >= (int)(sizeof(struct ethernet_header) + sizeof(struct arp_packet))) {
            struct ethernet_header* reth = (struct ethernet_header*)rx;
            if (ntohs(reth->type) == ETHERNET_TYPE_ARP) {
                struct arp_packet* rarp = (struct arp_packet*)(rx + sizeof(struct ethernet_header));
                if (ntohs(rarp->op) == 2 && rarp->src_ip == target_ip) {
                    memcpy(gateway_mac, rarp->src_mac, 6);
                    return 0;
                }
            }
        }
        task_yield();
    }
    return -1;
}
