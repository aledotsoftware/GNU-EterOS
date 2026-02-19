#include <net/defs.h>
#include <net/socket.h>
#include <net/e1000.h>
#include <string.h>
#include <timer.h>
#include <task.h>
#include <vga.h>
#include <mm.h>
#include <hal.h>

/* Global Network Info */
uint32_t my_ip = 0;
uint32_t gateway_ip = 0;
uint32_t dns_ip = 0;
uint8_t gateway_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
int network_ready = 0;

socket_entry_t socket_table[MAX_SOCKETS];

/* Forward Declarations */
void tcp_input(socket_entry_t* sock, struct tcp_header* tcp, int len, uint32_t src_ip);
int tcp_connect(socket_entry_t* sock, uint32_t dest_ip, uint16_t dest_port);
int tcp_send(socket_entry_t* sock, const void* data, int len);
int tcp_close(socket_entry_t* sock);

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

int net_init(void) {
    memset(socket_table, 0, sizeof(socket_table));
    network_ready = 0;
    return 0;
}

static void handle_arp(struct ethernet_header* eth, struct arp_packet* arp) {
    (void)eth;
    if (ntohs(arp->op) == 2) { /* Reply */
        if (arp->src_ip == gateway_ip) {
            memcpy(gateway_mac, arp->src_mac, 6);
        }
    } else if (ntohs(arp->op) == 1) { /* Request */
        if (arp->dest_ip == my_ip && my_ip != 0) {
            /* Reply to ARP Request */
            uint8_t buffer[64];
            struct ethernet_header* reth = (struct ethernet_header*)buffer;
            struct arp_packet* rarp = (struct arp_packet*)(buffer + sizeof(struct ethernet_header));

            uint8_t* my_mac = e1000_get_mac();

            memcpy(reth->dest, eth->src, 6);
            memcpy(reth->src, my_mac, 6);
            reth->type = htons(ETHERNET_TYPE_ARP);

            rarp->htype = htons(1);
            rarp->ptype = htons(ETHERNET_TYPE_IP);
            rarp->hlen = 6;
            rarp->plen = 4;
            rarp->op = htons(2); /* Reply */

            memcpy(rarp->src_mac, my_mac, 6);
            rarp->src_ip = my_ip;
            memcpy(rarp->dest_mac, arp->src_mac, 6);
            rarp->dest_ip = arp->src_ip;

            e1000_send_packet(buffer, sizeof(struct ethernet_header) + sizeof(struct arp_packet));
        }
    }
}

void net_poll(void) {
    uint8_t buffer[1514];
    int len = e1000_receive(buffer, sizeof(buffer));
    if (len > 0) {
        /* Security Check: Ethernet Header */
        if ((size_t)len < sizeof(struct ethernet_header)) return;

        struct ethernet_header* eth = (struct ethernet_header*)buffer;
        uint16_t type = ntohs(eth->type);

        if (type == ETHERNET_TYPE_ARP) {
            if ((size_t)len < sizeof(struct ethernet_header) + sizeof(struct arp_packet)) return;
            struct arp_packet* arp = (struct arp_packet*)(buffer + sizeof(struct ethernet_header));
            handle_arp(eth, arp);
        } else if (type == ETHERNET_TYPE_IP) {
            /* Security Check: IP Header presence */
            if ((size_t)len < sizeof(struct ethernet_header) + sizeof(struct ip_header)) return;

            struct ip_header* ip = (struct ip_header*)(buffer + sizeof(struct ethernet_header));

            /* Security Check: IP IHL */
            int ip_hdr_len = (ip->ver_ihl & 0xF) * 4;
            if (ip_hdr_len < 20 || (size_t)len < sizeof(struct ethernet_header) + ip_hdr_len) return;

            /* Basic IP Checks: Allow if addressed to us, or if broadcast, or if we don't have an IP yet */
            if (ip->dest == my_ip || ip->dest == 0xFFFFFFFF || my_ip == 0) {
                if (ip->proto == IP_PROTO_TCP) {
                    /* Security Check: TCP Header presence */
                    if ((size_t)len < sizeof(struct ethernet_header) + ip_hdr_len + sizeof(struct tcp_header)) return;

                    struct tcp_header* tcp = (struct tcp_header*)((uint8_t*)ip + ip_hdr_len);

                    /* Security Check: Payload Length Validation */
                    /* Calculate payload length based on IP Total Length field */
                    int total_ip_len = ntohs(ip->len);
                    int ip_payload_len = total_ip_len - ip_hdr_len;

                    /* Calculate actual available data in the received buffer */
                    int available_len = len - sizeof(struct ethernet_header) - ip_hdr_len;

                    /* Use the smaller of the two to avoid over-read */
                    int tcp_len = (ip_payload_len < available_len) ? ip_payload_len : available_len;

                    /* Ensure tcp_len covers at least the TCP header */
                    if (tcp_len < (int)sizeof(struct tcp_header)) return;

                    /* Find Socket */
                    for (int i = 0; i < MAX_SOCKETS; i++) {
                        socket_entry_t* s = &socket_table[i];
                        if (s->used && s->protocol == IPPROTO_TCP &&
                            s->local_port == ntohs(tcp->dest_port)) {

                            /* If connected, check remote IP/Port */
                            if (s->state != SOCKET_STATE_LISTEN) {
                                if (s->remote_ip != ip->src) {
                                     /* Strict check: */
                                     if (s->remote_ip != 0 && s->remote_ip != ip->src) continue;
                                }
                            }

                            tcp_input(s, tcp, tcp_len, ip->src);
                            break;
                        }
                    }
                } else if (ip->proto == IP_PROTO_UDP) {
                    /* UDP support could be added here for DHCP in network_task */
                    /* But currently dhcp_discover does its own polling. */
                }
            }
        }
    }
}

/* ARP Lookup (Blocking with timeout) */
int net_arp_lookup(uint32_t target_ip) {
    /* Send ARP Request */
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
    
    hal_console_write("[ARP] Looking up MAC for ");
    uint8_t* tip = (uint8_t*)&target_ip;
    char buf[16];
    itoa_s(tip[0], buf, sizeof(buf), 10); hal_console_write(buf); hal_console_write(".");
    itoa_s(tip[1], buf, sizeof(buf), 10); hal_console_write(buf); hal_console_write(".");
    itoa_s(tip[2], buf, sizeof(buf), 10); hal_console_write(buf); hal_console_write(".");
    itoa_s(tip[3], buf, sizeof(buf), 10); hal_console_write(buf);
    hal_console_write("...\n");

    e1000_send_packet(buffer, sizeof(struct ethernet_header) + sizeof(struct arp_packet));
    
    /* Wait for response via net_poll */
    uint64_t start = timer_get_ticks();
    while (timer_get_ticks() - start < 150) { /* 1.5s timeout */
        net_poll(); /* Process incoming packets */
        if (target_ip == gateway_ip && gateway_mac[0] != 0xFF) {
            hal_console_write("[ARP] Resolved Gateway MAC.\n");
            return 0; /* Found! */
        }
        task_yield();
    }
    
    hal_console_write("[ARP] Timeout. MAC not resolved.\n");
    return -1;
}

socket_t net_socket(int domain, int type, int protocol) {
    if (domain != AF_INET || type != SOCK_STREAM || protocol != IPPROTO_TCP) return -1;

    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!socket_table[i].used) {
            socket_table[i].used = 1;
            socket_table[i].id = i;
            socket_table[i].state = SOCKET_STATE_CLOSED;
            socket_table[i].local_port = 0;
            socket_table[i].rx_head = 0;
            socket_table[i].rx_tail = 0;
            socket_table[i].protocol = protocol;
            return i;
        }
    }
    return -1;
}

int net_connect(socket_t sock, const struct sockaddr_in* addr, int addrlen) {
    (void)addrlen;
    socket_entry_t* s = get_socket(sock);
    if (!s) return -1;

    /* Resolve Gateway ARP if needed */
    if (gateway_mac[0] == 0xFF) {
        if (net_arp_lookup(gateway_ip) != 0) return -2;
    }

    /* Bind ephemeral port if 0 */
    if (s->local_port == 0) {
        s->local_port = 49152 + (timer_get_ticks() % 16384);
    }

    return tcp_connect(s, addr->sin_addr, ntohs(addr->sin_port));
}

int net_send(socket_t sock, const void* buf, int len, int flags) {
    (void)flags;
    socket_entry_t* s = get_socket(sock);
    if (!s) return -1;
    return tcp_send(s, buf, len);
}

int net_recv(socket_t sock, void* buf, int len, int flags) {
    socket_entry_t* s = get_socket(sock);
    if (!s) return -1;

    /* Blocking Read */
    uint8_t* out = (uint8_t*)buf;
    int read = 0;

    while (read < len) {
        /* Check if data available */
        if (s->rx_head != s->rx_tail) {
            out[read++] = s->rx_buf[s->rx_tail];
            s->rx_tail = (s->rx_tail + 1) % RX_BUFFER_SIZE;
        } else {
            /* No data */
            if (s->state == SOCKET_STATE_CLOSED || s->state == SOCKET_STATE_TIME_WAIT || s->state == SOCKET_STATE_CLOSE_WAIT) {
                if (read > 0) return read;
                return 0; /* EOF */
            }
            if (flags & MSG_DONTWAIT) return (read > 0) ? read : -1;

            /* Wait */
            net_poll(); /* Ensure we process packets while waiting */
            task_yield();
        }
    }
    return read;
}

int net_close(socket_t sock) {
    socket_entry_t* s = get_socket(sock);
    if (!s) return -1;

    tcp_close(s);

    /* Allow socket to be reused? Wait for TCP state machine to finish */
    return 0;
}
