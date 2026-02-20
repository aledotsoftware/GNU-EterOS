#ifndef ETEROS_NET_SOCKET_H
#define ETEROS_NET_SOCKET_H

#include "../types.h"
#include <sem.h>

/* Address Families */
#define AF_INET     2
#define AF_INET6    10

/* Socket Types */
#define SOCK_STREAM 1
#define SOCK_DGRAM  2

/* Protocols */
#define IPPROTO_IP  0
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

/* Socket Options / Flags */
#define MSG_DONTWAIT 0x40

typedef int socket_t;

struct sockaddr {
    uint16_t sa_family;
    char     sa_data[14];
};

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t  sin_zero[8];
};

/* --- Socket API --- */

/* Initialize the network stack (called by kernel main) */
int net_init(void);

/* Process incoming packets (called by network task or poll loop) */
void net_poll(void);

/* Create a socket */
socket_t net_socket(int domain, int type, int protocol);

/* Connect to a remote address */
int net_connect(socket_t sock, const struct sockaddr_in* addr, int addrlen);

/* Send data */
int net_send(socket_t sock, const void* buf, int len, int flags);

/* Receive data */
int net_recv(socket_t sock, void* buf, int len, int flags);

/* Close socket */
int net_close(socket_t sock);

/* --- Internal Stack Structures (Visible for stack modules like tcp.c) --- */

#define SOCKET_STATE_CLOSED      0
#define SOCKET_STATE_LISTEN      1
#define SOCKET_STATE_SYN_SENT    2
#define SOCKET_STATE_SYN_RCVD    3
#define SOCKET_STATE_ESTABLISHED 4
#define SOCKET_STATE_FIN_WAIT1   5
#define SOCKET_STATE_FIN_WAIT2   6
#define SOCKET_STATE_CLOSE_WAIT  7
#define SOCKET_STATE_CLOSING     8
#define SOCKET_STATE_LAST_ACK    9
#define SOCKET_STATE_TIME_WAIT   10

/* Receive Buffer Size */
#define RX_BUFFER_SIZE 4096

typedef struct {
    int id;
    int used;
    int state;
    int type;
    int protocol;

    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;

    /* TCP Specifics */
    uint32_t seq_num;     /* Next seq to send */
    uint32_t ack_num;     /* Next seq expected */
    uint16_t window;

    /* Receive Buffer (Circular) */
    uint8_t rx_buf[RX_BUFFER_SIZE];
    int rx_head; /* Write index */
    int rx_tail; /* Read index */

} socket_entry_t;

/* Global Socket Table */
#define MAX_SOCKETS 16
extern socket_entry_t socket_table[MAX_SOCKETS];

/* Global Network Semaphore (stack.c) */
extern sem_t net_sem;

/* Helper to get socket pointer from ID */
static inline socket_entry_t* get_socket(socket_t id) {
    if (id < 0 || id >= MAX_SOCKETS) return 0;
    if (!socket_table[id].used) return 0;
    return &socket_table[id];
}

#endif /* ETEROS_NET_SOCKET_H */
