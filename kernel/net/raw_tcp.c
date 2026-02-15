#include <net/defs.h>
#include <net/e1000.h>
#include <string.h>
#include <timer.h>
#include <task.h>
#include <vga.h>
#include <hal.h>

extern uint32_t my_ip, gateway_ip;
extern uint8_t gateway_mac[6];
extern int net_arp_lookup(uint32_t target_ip);
extern uint16_t net_checksum(void* vdata, size_t length);

/* 172.67.140.40 is tudexgames.com (Cloudflare) */
#define TUDEX_IP 0x288C43AC 

int raw_tcp_get(const char* host, const char* path, char* response_buf, size_t max_len) {
    if (my_ip == 0) return -1;
    
    extern uint32_t ip_aton(const char* cp);
    uint32_t target_ip = ip_aton(host);
    if (target_ip == 0) {
        if (strcmp(host, "tudexgames.com") == 0) target_ip = TUDEX_IP;
        else if (strcmp(host, "google.com") == 0) target_ip = 0x4850fa8e;
        else return -1;
    }

    hal_console_write("[RAW-TCP] Connecting to ");
    hal_console_write(host);
    hal_console_write("...\n");

    /* 1. ARP for Gateway */
    if (gateway_mac[0] == 0xFF) {
        if (net_arp_lookup(gateway_ip) != 0) return -1;
    }
    
    uint8_t buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    
    struct ethernet_header* eth = (struct ethernet_header*)buffer;
    struct ip_header* ip = (struct ip_header*)(buffer + sizeof(struct ethernet_header));
    struct tcp_header* tcp = (struct tcp_header*)(buffer + sizeof(struct ethernet_header) + sizeof(struct ip_header));
    
    uint16_t my_port = 12345 + (timer_get_ticks() % 1000);
    uint32_t seq = 0x100;
    
    /* 2. SEND SYN */
    memcpy(eth->dest, gateway_mac, 6);
    memcpy(eth->src, e1000_get_mac(), 6);
    eth->type = htons(ETHERNET_TYPE_IP);
    
    ip->ver_ihl = 0x45;
    ip->len = htons(sizeof(struct ip_header) + sizeof(struct tcp_header));
    ip->ttl = 64;
    ip->proto = IP_PROTO_TCP;
    ip->src = my_ip;
    ip->dest = target_ip;
    ip->checksum = 0;
    ip->checksum = net_checksum(ip, sizeof(struct ip_header));
    
    tcp->src_port = htons(my_port);
    tcp->dest_port = htons(80);
    tcp->seq = htonl(seq);
    tcp->ack_seq = 0;
    tcp->flags = htons(0x5002); /* SYN, Offset 5 (20 bytes) */
    tcp->window = htons(2048);
    tcp->checksum = 0;

    /* Pseudo-header for SYN Checksum */
    struct pseudo_header ph_syn;
    ph_syn.src = my_ip;
    ph_syn.dest = target_ip;
    ph_syn.zero = 0;
    ph_syn.proto = IP_PROTO_TCP;
    ph_syn.len = htons(sizeof(struct tcp_header));

    uint8_t chk_buf_syn[64];
    memcpy(chk_buf_syn, &ph_syn, 12);
    memcpy(chk_buf_syn + 12, tcp, sizeof(struct tcp_header));
    tcp->checksum = net_checksum(chk_buf_syn, 12 + sizeof(struct tcp_header));
    
    e1000_send_packet(buffer, sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct tcp_header));
    
    /* 3. WAIT SYN/ACK */
    uint64_t start = timer_get_ticks();
    uint8_t rx[1514];
    uint32_t server_seq = 0;
    
    while (timer_get_ticks() - start < 500) {
        int rlen = e1000_receive(rx, sizeof(rx));
        if (rlen > 40) {
            struct ip_header* rip = (struct ip_header*)(rx + 14);
            if (rip->proto == IP_PROTO_TCP && rip->src == target_ip) {
                struct tcp_header* rtcp = (struct tcp_header*)(rx + 14 + 20);
                if (ntohs(rtcp->flags) & TCP_SYN && ntohs(rtcp->flags) & TCP_ACK) {
                    server_seq = ntohl(rtcp->seq);
                    break;
                }
            }
        }
        task_yield();
    }
    
    if (server_seq == 0) return -2; /* Timeout SYN/ACK */
    
    /* 4. SEND ACK + HTTP GET */
    char get_req[256];
    strlcpy(get_req, "GET ", 256);
    strlcat(get_req, path, 256);
    strlcat(get_req, " HTTP/1.0\r\nHost: tudexgames.com\r\nConnection: close\r\n\r\n", 256);
    size_t get_len = strlen(get_req);
    
    ip->len = htons(sizeof(struct ip_header) + sizeof(struct tcp_header) + get_len);
    ip->checksum = 0;
    ip->checksum = net_checksum(ip, sizeof(struct ip_header));
    
    tcp->seq = htonl(seq + 1);
    tcp->ack_seq = htonl(server_seq + 1);
    tcp->flags = htons(0x5018); /* ACK + PSH */
    tcp->checksum = 0;
    
    memcpy(buffer + 14 + 20 + 20, get_req, get_len);
    
    /* Correct TCP Checksum with Pseudo-header */
    struct pseudo_header ph_get;
    ph_get.src = my_ip;
    ph_get.dest = target_ip;
    ph_get.zero = 0;
    ph_get.proto = IP_PROTO_TCP;
    ph_get.len = htons(sizeof(struct tcp_header) + get_len);
    
    uint8_t chk_buf[512];
    memcpy(chk_buf, &ph_get, 12);
    memcpy(chk_buf + 12, tcp, sizeof(struct tcp_header) + get_len);
    tcp->checksum = net_checksum(chk_buf, 12 + sizeof(struct tcp_header) + get_len);
    
    e1000_send_packet(buffer, 14 + 20 + 20 + get_len);
    
    /* 5. RECEIVE DATA */
    start = timer_get_ticks();
    while (timer_get_ticks() - start < 3000) { /* 3s for response */
        int rlen = e1000_receive(rx, sizeof(rx));
        if (rlen > 40) {
            struct ip_header* rip = (struct ip_header*)(rx + 14);
            if (rip->proto == IP_PROTO_TCP && rip->src == target_ip) {
                struct tcp_header* rtcp = (struct tcp_header*)(rx + 14 + 20);
                int data_off = (ntohs(rtcp->flags) >> 12) * 4;
                int data_len = ntohs(rip->len) - 20 - data_off;
                if (data_len > 0) {
                    size_t u_data_len = (size_t)data_len;
                    size_t copy_len = (u_data_len < max_len - 1) ? u_data_len : max_len - 1;
                    memcpy(response_buf, rx + 14 + 20 + data_off, copy_len);
                    response_buf[copy_len] = 0;
                    return (int)copy_len;
                }
                /* Also check for FIN */
                if (ntohs(rtcp->flags) & 0x0001) break;
            }
        }
        task_yield();
    }
    
    return -3; /* Timeout Data */
}
