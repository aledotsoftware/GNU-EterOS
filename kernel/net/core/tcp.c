#include <net/defs.h>
#include <net/socket.h>
#include <net/nic.h>
#include <string.h>
#include <timer.h>
#include <task.h>
#include <hal.h>
#include <fs/devfs.h>

extern uint32_t my_ip;
extern uint8_t gateway_mac[6];
extern uint16_t net_checksum(void* vdata, size_t length);

/* Pseudo Header for Checksum */
static int tcp_send_packet(socket_entry_t* sock, const void* payload, int len, int flags) {
    if (!current_nic) return -1;

    uint8_t buffer[1514];
    size_t header_len = sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct tcp_header);

    /* Security Fix: Check for buffer overflow */
    if (len < 0 || (size_t)len > (sizeof(buffer) - header_len)) {
        hal_console_write("[TCP] Error: Payload too large, dropping packet.\n");
        return -1;
    }

    struct ethernet_header* eth = (struct ethernet_header*)buffer;
    struct ip_header* ip = (struct ip_header*)(buffer + sizeof(struct ethernet_header));
    struct tcp_header* tcp = (struct tcp_header*)(buffer + sizeof(struct ethernet_header) + sizeof(struct ip_header));

    memset(buffer, 0, sizeof(buffer));

    /* Ethernet */
    memcpy(eth->dest, gateway_mac, 6);
    memcpy(eth->src, current_nic->get_mac(), 6);
    eth->type = htons(ETHERNET_TYPE_IP);

    /* IP */
    ip->ver_ihl = 0x45;
    ip->len = htons(sizeof(struct ip_header) + sizeof(struct tcp_header) + len);
    ip->ttl = 64;
    ip->proto = IP_PROTO_TCP;
    ip->src = my_ip;
    ip->dest = sock->remote_ip;
    ip->checksum = 0;
    ip->checksum = net_checksum(ip, sizeof(struct ip_header));

    /* TCP */
    tcp->src_port = htons(sock->local_port);
    tcp->dest_port = htons(sock->remote_port);
    tcp->seq = htonl(sock->seq_num);
    tcp->ack_seq = htonl(sock->ack_num);
    tcp->flags = htons((5 << 12) | flags); /* Offset 5 words */
    tcp->window = htons(RX_BUFFER_SIZE);
    tcp->checksum = 0;

    /* Copy Payload */
    if (len > 0) {
        memcpy(buffer + sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct tcp_header), payload, len);
    }

    /* Checksum */
    struct pseudo_header ph;
    ph.src = my_ip;
    ph.dest = sock->remote_ip;
    ph.zero = 0;
    ph.proto = IP_PROTO_TCP;
    ph.len = htons(sizeof(struct tcp_header) + len);

    uint8_t chk_buf[1600];
    int total_len = sizeof(struct tcp_header) + len;

    if (sizeof(ph) + total_len <= sizeof(chk_buf)) {
        memcpy(chk_buf, &ph, sizeof(ph));
        memcpy(chk_buf + sizeof(ph), tcp, sizeof(struct tcp_header));
        if (len > 0) {
            memcpy(chk_buf + sizeof(ph) + sizeof(struct tcp_header), payload, len);
        }
        tcp->checksum = net_checksum(chk_buf, sizeof(ph) + total_len);
    }

    return current_nic->send(buffer, sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct tcp_header) + len);
}

void tcp_input(socket_entry_t* sock, struct tcp_header* tcp, int len, uint32_t src_ip) {
    (void)src_ip;
    uint32_t seq = ntohl(tcp->seq);
    uint32_t ack = ntohl(tcp->ack_seq);
    uint16_t flags = ntohs(tcp->flags);

    /* Calculate Header Length and Data Length */
    int doff = (flags >> 12) * 4;

    /* SECURITY FIX: Validate TCP Data Offset (doff) to prevent integer underflow and out-of-bounds reads */
    if (doff < 20 || doff > len) {
        hal_console_write("[TCP] Warning: Invalid TCP Data Offset or Length, dropping packet.\n");
        return;
    }

    int data_len = len - doff;

    if (sock->state == SOCKET_STATE_SYN_SENT) {
        if ((flags & TCP_SYN) && (flags & TCP_ACK)) {
            sock->state = SOCKET_STATE_ESTABLISHED;
            sock->ack_num = seq + 1;
            sock->seq_num = ack; /* Remote acked our SYN, so our next seq is ack */

            /* Send ACK */
            tcp_send_packet(sock, NULL, 0, TCP_ACK);
        }
    } else if (sock->state == SOCKET_STATE_ESTABLISHED) {
        if (data_len > 0) {
            int available = (sock->rx_tail - sock->rx_head - 1 + RX_BUFFER_SIZE) % RX_BUFFER_SIZE;
            if (data_len > available) {
                hal_console_write("[TCP] Warning: Receive buffer full, dropping packet.\n");
                return; /* Drop packet, do not ACK */
            }

            /* Copy data to rx_buf */
            uint8_t* data = (uint8_t*)tcp + doff;
            for (int i=0; i<data_len; i++) {
                sock->rx_buf[sock->rx_head] = data[i];
                sock->rx_head = (sock->rx_head + 1) % RX_BUFFER_SIZE;
            }

            sock->ack_num += data_len;
            /* Send ACK */
            hal_console_write("[TCP] Data rx, sending ACK\n");
            tcp_send_packet(sock, NULL, 0, TCP_ACK);
        } else if (flags & TCP_ACK) {
             /* Pure ACK, update sequence? */
             /* In strict TCP, we update window, etc. Here we ignore. */
        }

        if (flags & TCP_FIN) {
            sock->state = SOCKET_STATE_CLOSE_WAIT;
            sock->ack_num++;
            tcp_send_packet(sock, NULL, 0, TCP_ACK);
        }
    } else if (sock->state == SOCKET_STATE_FIN_WAIT1) {
        if (flags & TCP_ACK) {
            if (ack == sock->seq_num + 1) {
                sock->state = SOCKET_STATE_FIN_WAIT2;
            }
        }
        if (flags & TCP_FIN) {
            sock->ack_num++;
            tcp_send_packet(sock, NULL, 0, TCP_ACK);
            if (sock->state == SOCKET_STATE_FIN_WAIT2) {
                sock->state = SOCKET_STATE_TIME_WAIT;
            } else {
                sock->state = SOCKET_STATE_CLOSING;
            }
        }
    } else if (sock->state == SOCKET_STATE_FIN_WAIT2) {
        if (flags & TCP_FIN) {
            sock->ack_num++;
            tcp_send_packet(sock, NULL, 0, TCP_ACK);
            sock->state = SOCKET_STATE_TIME_WAIT;
            /* In minimalist stack, maybe just close? */
            sock->used = 0; // Free socket
        }
    }
}

int tcp_connect(socket_entry_t* sock, uint32_t dest_ip, uint16_t dest_port) {
    sock->remote_ip = dest_ip;
    sock->remote_port = dest_port;

    /* Security Fix: Generate a secure random Initial Sequence Number */
    uint32_t secure_isn = 0;
    get_random_bytes((uint8_t*)&secure_isn, sizeof(secure_isn));
    sock->seq_num = secure_isn;

    sock->ack_num = 0;
    sock->state = SOCKET_STATE_SYN_SENT;

    hal_console_write("[TCP] Connecting... sending SYN\n");
    tcp_send_packet(sock, NULL, 0, TCP_SYN);

    /* Wait for ESTABLISHED */
    uint64_t start = timer_get_ticks();
    while (sock->state != SOCKET_STATE_ESTABLISHED) {
        if (timer_get_ticks() - start > 500) { /* 5s Timeout */
            hal_console_write("[TCP] Connection Timeout.\n");
            return -1;
        }

        /* If we don't rely on background task, we must poll here.
           Since we don't know if background task is running,
           and calling net_poll is safe (it just checks e1000),
           we can call it.
        */
        extern void net_poll(void);
        net_poll();
        task_yield();
    }

    hal_console_write("[TCP] Connected! (ESTABLISHED)\n");
    return 0;
}

int tcp_send(socket_entry_t* sock, const void* data, int len) {
    if (sock->state != SOCKET_STATE_ESTABLISHED) return -1;

    /* Simple Send: PSH+ACK */
    tcp_send_packet(sock, data, len, TCP_PSH | TCP_ACK);
    sock->seq_num += len;

    return len;
}

int tcp_close(socket_entry_t* sock) {
    if (sock->state == SOCKET_STATE_ESTABLISHED) {
        sock->state = SOCKET_STATE_FIN_WAIT1;
        tcp_send_packet(sock, NULL, 0, TCP_FIN | TCP_ACK);
    } else {
        sock->used = 0;
    }
    return 0;
}
